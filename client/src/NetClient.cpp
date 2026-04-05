#include "pch.h"
#include "NetClient.h"

namespace tc
{
	NetClient::NetClient(NetClientHandler& handler)
		: m_handler(handler)
		, m_workGuard{ asio::make_work_guard(m_context) }
	{
	}

	NetClient::~NetClient()
	{
		stop();
	}

	void NetClient::start()
	{
		std::scoped_lock lock(m_mtx);
		m_messagesIn.restart();
		if (m_worker.joinable())
			return;

		m_worker = std::jthread([this] { m_context.run(); });
	}

	void NetClient::stop()
	{
		m_messagesIn.shutdown();

		std::jthread worker;
		std::jthread recvWorker;
		{
			std::scoped_lock lock(m_mtx);
			m_context.stop();

			worker = std::move(m_worker);
			recvWorker = std::move(m_recvWorker);
		}

		if (worker.joinable())
			worker.join();
		if (recvWorker.joinable())
			recvWorker.join();

		{
			std::scoped_lock lock(m_mtx);
			m_connection.reset();
			m_context.restart();
			m_ip.clear();
			m_port = 0;
		}
	}

	void NetClient::connect()
	{
		std::scoped_lock lock(m_mtx);
		if (m_connecting || (m_connection && m_connection->isConnected()))
			return;

		m_connecting = true;
		asio::co_spawn(m_context, connectImpl(), asio::detached);
	}

	void NetClient::send(std::string message)
	{
		if (message.empty())
			return;

		net::Message<MsgTypes> msg;
		if ("!ping" == message)
			msg.header.id = MsgTypes::Ping;
		else
		{
			msg.header.id = MsgTypes::ChatText;
			msg << std::move(message);
		}

		if (!isConnected())
		{
			std::println("Not connected, cannot send message");
			return;
		}

		m_connection->send(std::move(msg));
	}

	bool NetClient::isConnected()
	{
		std::scoped_lock lock(m_mtx);
		return m_connection && m_connection->isConnected();
	}

	bool NetClient::isConnectible()
	{
		std::scoped_lock lock(m_mtx);
		return utils::isIpValid(m_ip) && utils::isPortValid(m_port);
	}

	const std::string& NetClient::ip()
	{
		std::scoped_lock lock(m_mtx);
		return m_ip;
	}

	uint32_t NetClient::port()
	{
		std::scoped_lock lock(m_mtx);
		return m_port;
	}

	void NetClient::setFullAddress(std::string ip, uint32_t port)
	{
		m_ip = std::move(ip);
		m_port = port;
	}

	asio::awaitable<void> NetClient::connectImpl()
	{
		try
		{
			asio::ip::tcp::resolver resolver{ m_context };
			auto endpoints = co_await resolver.async_resolve(m_ip, std::format("{}", m_port), asio::use_awaitable);

			auto connection = std::make_shared<net::Connection<MsgTypes>>(
				net::Connection<MsgTypes>::Owner::Client,
				m_context,
				asio::ip::tcp::socket(m_context),
				m_messagesIn);

			bool ok = co_await connection->connectToServer(endpoints);

			std::scoped_lock lock(m_mtx);
			m_connecting = false;

			if (!ok)
			{
				std::println("Connection failed");
				m_connection.reset();
			}
			else
			{
				m_connection = std::move(connection);
				std::println("Connected!");

				if (!m_recvWorker.joinable())
					m_recvWorker = std::jthread([this] { receiveMsgLoop(); });
			}
		}
		catch (const std::exception& e)
		{
			std::scoped_lock lock(m_mtx);
			m_connecting = false;
			std::println("Connect error: {}", e.what());
		}

		co_return;
	}

	void NetClient::receiveMsgLoop()
	{
		for (;;)
		{
			{
				std::scoped_lock lock(m_mtx);
				if (!m_connection && m_messagesIn.stopped())
				{
					std::println("[CLIENT] No connection or message queue is stopped.");
					break;
				}
			}

			m_messagesIn.wait();
			auto optMsg = m_messagesIn.popFront();
			if (!optMsg)
				continue;

			try
			{
				m_handler.onReceive(std::move((*optMsg).msg));
			}
			catch (const std::exception& e)
			{
				std::println("[CLIENT] Error processing message: {}", e.what());
			}
		}
	}
}
