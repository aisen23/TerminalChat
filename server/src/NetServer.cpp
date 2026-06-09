#include "pch.h"
#include "NetServer.h"

namespace tc
{
	NetServer::NetServer(NetServerHandler& handler, uint32_t port)
		: m_handler(handler)
		, m_acceptor(m_context, asio::ip::tcp::endpoint(asio::ip::tcp::v4(), static_cast<unsigned short>(port)))
	{
	}

	NetServer::~NetServer()
	{
		stop();
	}

	void NetServer::start()
	{
		asio::co_spawn(m_context, acceptLoop(), asio::detached);

		m_worker = std::jthread([this] { m_context.run(); });
	}

	void NetServer::stop()
	{
		{
			std::scoped_lock lock(m_mtx);
			boost::system::error_code ec;
			m_acceptor.close(ec);
			m_context.stop();
		}

		if (m_worker.joinable())
			m_worker.join();

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
					std::move(socket));

				asio::co_spawn(m_context, [this, connection]() -> asio::awaitable<void> {
					bool ok = co_await connection->connectToClient();
					if (ok)
					{
						const std::string& uuid = connection->getUuid();
						const std::string& name = connection->getName();
						auto client = std::make_shared<Client>(Client{ connection, name, uuid });

						connection->onMessageReceived = [this, client](net::OwnedMessage<MsgTypes> ownedMsg) {
							m_handler.onReceive(client, std::move(ownedMsg.msg));
						};
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
					co_return;
				}, asio::detached);
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
}
