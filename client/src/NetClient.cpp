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
		if (m_worker.joinable())
			return;

		m_worker = std::jthread([this] { m_context.run(); });
	}

	void NetClient::stop()
	{
		std::scoped_lock lock(m_mtx);
		m_connection.reset();
		m_context.stop();
		if (m_worker.joinable())
			m_worker.join();

		m_context.restart();

		m_ip.clear();
		m_port = 0;
	}

	void NetClient::connect()
	{
		asio::co_spawn(
			m_context,
			connectImpl(),
			asio::detached
		);
	}

	void NetClient::send(std::string message)
	{
		net::Message<MsgTypes> msg;
		if ("!ping" == message)
			msg.header.id = MsgTypes::Ping;
		else
		{
			msg.header.id = MsgTypes::ChatText;
			net::Message<MsgTypes> msg{ type };
			msg << message;
		}
	}

	bool NetClient::isConnected() const
	{
		std::scoped_lock lock(m_mtx);
		return m_connection && m_connection->isConnected();
	}

	bool NetClient::isConnectible() const
	{
		std::scoped_lock lock(m_mtx);
		return utils::isIpValid(m_ip) && utils::isPortValid(m_port);
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

			m_connection = std::make_unique<net::Connection<MsgTypes>>(
				net::Connection<MsgTypes>::Owner::Client,
				m_context,
				asio::ip::tcp::socket(m_context),
				m_messagesIn);

			bool ok = co_await m_connection->connectToServer(endpoints);

			if (!ok)
				std::println("Connection failed");
			else
				std::println("Connected!");
		}
		catch (const std::exception& e)
		{
			std::println("Connect error: {}", e.what());
		}
	}
}
