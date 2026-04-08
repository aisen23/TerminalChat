#include "pch.h"
#include "NetServer.h"

namespace tc
{
	NetServer::NetServer(NetServerHandler& handler, uint32_t port)
		: m_handler(handler)
		, m_workGuard{ asio::make_work_guard(m_context) }
		, m_acceptor(m_context, asio::ip::tcp::endpoint(asio::ip::tcp::v4(), static_cast<unsigned short>(port)))
	{
	}

	NetServer::~NetServer()
	{
		stop();
	}

	void NetServer::start()
	{
		std::scoped_lock lock(m_mtx);
		if (!m_workers.empty())
			return;

		asio::co_spawn(m_context, acceptLoop(), asio::detached);

		size_t threadCount = std::thread::hardware_concurrency() / 2;
		if (threadCount < 1)
			threadCount = 1;

		for (size_t i = 0; i < threadCount; ++i)
			m_workers.emplace_back([this] { m_context.run(); });

		m_recvWorker = std::jthread([this] { receiveMsgLoop(); });
	}

	void NetServer::stop()
	{
		m_messagesIn.shutdown();

		std::vector<std::jthread> workers;
		std::jthread recvWorker;
		{
			std::scoped_lock lock(m_mtx);
			boost::system::error_code ec;
			m_acceptor.close(ec);

			m_context.stop();

			workers = std::move(m_workers);
			recvWorker = std::move(m_recvWorker);
		}

		for (auto& w : workers)
			if (w.joinable())
				w.join();

		if (recvWorker.joinable())
			recvWorker.join();

		{
			std::scoped_lock lock(m_mtx);
			m_clients.clear();
			m_context.restart();
		}
	}

	void NetServer::sendToClient(std::shared_ptr<Client> client, net::Message<MsgTypes> msg)
	{
		if (client && client->connection && client->connection->isConnected())
			client->connection->send(std::move(msg));
	}

	void NetServer::sendToAll(net::Message<MsgTypes> msg, std::shared_ptr<Client> ignoreClient)
	{
		std::scoped_lock lock(m_mtx);
		for (auto& client : m_clients)
			if (client != ignoreClient && client->connection && client->connection->isConnected())
				client->connection->send(msg);
	}

	asio::awaitable<void> NetServer::acceptLoop()
	{
		try
		{
			for (;;)
			{
				auto socket = co_await m_acceptor.async_accept(asio::use_awaitable);

				auto connection = std::make_shared<net::Connection<MsgTypes>>(
					net::Connection<MsgTypes>::Owner::Server,
					m_context,
					std::move(socket),
					m_messagesIn);

				static int idCounter = 1000;
				std::string uid = std::format("User{}", idCounter++);

				bool ok = co_await connection->connectToClient(uid);
				if (ok)
				{
					auto client = std::make_shared<Client>(Client{connection, uid, static_cast<uint32_t>(idCounter)});
					{
						std::scoped_lock lock(m_mtx);
						m_clients.push_back(client);
					}
					std::println("[SERVER] Client connected: {}", uid);
					m_handler.onClientConnect(client);
				}
			}
		}
		catch (const boost::system::system_error& e)
		{
			if (e.code() != asio::error::operation_aborted)
				std::println("[SERVER] Accept error: {}", e.what());
		}
		catch (const std::exception& e)
		{
			std::println("[SERVER] Accept error: {}", e.what());
		}
		co_return;
	}

	void NetServer::receiveMsgLoop()
	{
		for (;;)
		{
			m_messagesIn.wait();
			if (m_messagesIn.stopped())
				break;

			auto optMsg = m_messagesIn.popFront();
			if (!optMsg)
				continue;

			try
			{
				auto ownedMsg = std::move(*optMsg);
				std::shared_ptr<Client> foundClient;
				{
					std::scoped_lock lock(m_mtx);
					for (auto& client : m_clients)
					{
						if (client->connection == ownedMsg.remote)
						{
							foundClient = client;
							break;
						}
					}
				}

				if (foundClient)
					m_handler.onReceive(foundClient, std::move(ownedMsg.msg));
			}
			catch (const std::exception& e)
			{
				std::println("[SERVER] Error processing message: {}", e.what());
			}
		}
	}
}
