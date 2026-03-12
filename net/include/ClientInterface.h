#pragma once

#include "Connection.h"
#include "Queue.h"

#include <boost/asio.hpp>

#include <print>

namespace net
{
	namespace asio = boost::asio;
    template<typename T>
    class ClientInterface {
    public:
        ClientInterface() = default;
        virtual ~ClientInterface() { disconnect(); }

        asio::awaitable<bool> connect(const std::string& host, const uint16_t port)
		{
			boost::system::error_code ec;
			auto executor = co_await asio::this_coro::executor;
			asio::ip::tcp::resolver resolver(executor);
			const auto endpoints = co_await resolver.async_resolve(host, std::to_string(port), asio::redirect_error(asio::use_awaitable, ec));
			if (ec)
			{
				std::println("DNS resolution error: {}, code={}", ec.message(), ec.value());
				co_return false;
			}
			
			m_connection = std::make_unique<Connection<T>>(Connection<T>::owner::client, executor, asio::ip::tcp::socket(executor), m_messagesIn);

			co_return co_await m_connection->connectToServer(endpoints);
		}

        void disconnect()
		{
            if (m_connection)
				m_connection->disconnect();

            m_connection.reset();
			onDisconnect();
        }

        bool isConnected() const
		{
            return m_connection && m_connection->isConnected();
        }

        void send(const Message<T>& msg)
		{
            if (isConnected())
				m_connection->send(msg);
        }

        tc::Queue<OwnedMessage<T>>& incoming() { return m_qMessagesIn; }

	private:
		virtual onDisconnect() = 0;

    protected:
        std::unique_ptr<Connection<T>> m_connection;

    private:
        tc::Queue<OwnedMessage<T>> m_messagesIn;
		asio::io_context m_ioContext;
		std::unique_ptr<asio::executor_work_guard<asio::io_context::executor_type>> m_workGuard;
		std::thread m_workerThread;
    };
}
