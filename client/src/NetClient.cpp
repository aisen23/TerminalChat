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

	void NetClient::disconnect()
	{
		std::scoped_lock lock(m_mtx);
		if (m_connection && m_connection->isConnected())
		{
			tc::log::info("Disconnecting...");
			m_connection->disconnect();
		}

		m_ip.clear();
		m_port = 0;
	}

	bool NetClient::send(const std::string& message)
	{
		if (message.empty())
			return false;

		if (!isConnected())
		{
			log::printAs("CLIENT", "Not connected, cannot send message");
			return false;
		}

		net::Message<MsgTypes> msg;
		msg.header.id = MsgTypes::ChatText;
		msg << message;

		m_connection->send(std::move(msg));
		return true;
	}

	bool NetClient::send(net::Message<MsgTypes> msg)
	{
		if (!isConnected())
		{
			log::printAs("CLIENT", "Not connected, cannot send message");
			return false;
		}

		m_connection->send(std::move(msg));
		return true;
	}

	bool NetClient::isConnected()
	{
		std::scoped_lock lock(m_mtx);
		return m_connection && m_connection->isConnected();
	}

	bool NetClient::isConnecting()
	{
		std::scoped_lock lock(m_mtx);
		return m_connecting;
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
		std::scoped_lock lock(m_mtx);
		m_ip = std::move(ip);
		m_port = port;
	}

	asio::awaitable<void> NetClient::connectImpl()
	{
		try
		{
			asio::ip::tcp::resolver resolver{ m_context };
			log::print("Resolving {}:{}...", m_ip, m_port);
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
				tc::log::info("Connected!");

				if (!m_recvWorker.joinable())
					m_recvWorker = std::jthread([this] { receiveMsgLoop(); });
			}
		}
		catch (const std::exception& e)
		{
			std::scoped_lock lock(m_mtx);
			m_connecting = false;
			tc::log::error("Connect error: {}", e.what());
		}

		co_return;
	}

	void NetClient::receiveMsgLoop()
	{
		for (;;)
		{
			m_messagesIn.wait();

			if (m_messagesIn.stopped() && m_messagesIn.empty())
			{
				std::println("[CLIENT] Message queue is stopped.");
				break;
			}

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
