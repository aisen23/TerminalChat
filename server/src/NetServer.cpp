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
			m_clientMap.clear();
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
		for (auto const& [uuid, client] : m_clientMap)
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

				bool ok = co_await connection->connectToClient();
				if (ok)
				{
					const std::string& uuid = connection->getUuid();
					const std::string& name = connection->getName();
					auto client = std::make_shared<Client>(Client{connection, name, uuid});
					{
						connection->onDisconnect = [this, client](auto /*conn*/) {
							m_handler.pushTask([this, client] {
								std::shared_ptr<Client> toDisconnect;
								{
									std::scoped_lock lock(m_mtx);
									if (auto it = m_clientMap.find(client->uuid); it != m_clientMap.end())
									{
										toDisconnect = it->second;
										m_clientMap.erase(it);
									}
								}
								if (toDisconnect)
									m_handler.onClientDisconnect(toDisconnect);
							});
						};

						std::scoped_lock lock(m_mtx);
						m_clientMap[uuid] = client;
						log::debug("Clients number: {}", m_clientMap.size());
					}
					std::println("[SERVER] Client connected with UUID: {}", uuid);
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
		using namespace std::chrono_literals;
		for (;;)
		{
			m_messagesIn.wait();
			if (m_messagesIn.stopped() && m_messagesIn.empty())
				break;

			if (m_messagesIn.empty())
				continue;

			auto optMsg = m_messagesIn.popFront();
			if (!optMsg)
				continue;

			try
			{
				auto ownedMsg = std::move(*optMsg);
				std::shared_ptr<Client> foundClient;
				{
					std::scoped_lock lock(m_mtx);
					if (auto it = m_clientMap.find(ownedMsg.remote->getUuid()); it != m_clientMap.end())
						foundClient = it->second;
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
