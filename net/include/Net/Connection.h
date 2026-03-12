#pragma once

#include "Message.h"

#include <Common/Queue.h>

#include <boost/asio.hpp>

#include <memory>
#include <string>

namespace net
{
	namespace asio = boost::asio;
	template<typename T>
	class Connection : public std::enable_shared_from_this<Connection<T>>
	{
	public:
		enum class Owner
		{
			Client,
			Server
		};
	public:
		Connection(Owner owner, asio::io_context& context, asio::ip::tcp::socket socket, tc::Queue<OwnedMessage<T>>& messagesIn)
			: m_owner{ owner }, m_context{ context }, m_socket(std::move(socket)), m_messagesIn{ messagesIn }
		{}
		virtual ~Connection()
		{}
		const std::string& GetId() const { return m_id; }

		asio::awaitable<bool> connectToClient(const std::string& uid)
		{
			if (m_owner != Owner::Server)
			{

				co_return false;
			}

			if (!m_socket.is_open())
			{
				std::println("Failed to connect to client: socket is closed");
				co_return false;
			}

			// TODO: handshake
			m_id = uid;
			auto executor = co_await asio::this_coro::executor;
			asio::co_spawn(executor, readHeader(), asio::detached);

			std::println("Connection validated for UID: {}", m_id);
			co_return true;
		}

		asio::awaitable<bool> connectToServer(const asio::ip::tcp::resolver::results_type& endpoints)
		{
			if (m_owner != Owner::Client)
			{
				std::println("Server cannot connect to itself");
				co_return false;
			}

			boost::system::error_code ec;
			co_await asio::async_connect(
				m_socket,
				endpoints,
				asio::redirect_error(asio::use_awaitable, ec)
			);

			if (ec)
			{
				std::println("Failed to connect to endpoint: {}", ec.message());
				co_return false;
			}

			auto executor = co_await asio::this_coro::executor;
			asio::co_spawn(executor, readHeader(), asio::detached);
			co_return true;
		}

		void disconnect()
		{
			if (isConnected())
				asio::post(m_asioContext, [this]() { m_socket.close(); });
		}

		bool isConnected() const
		{
			return m_socket.is_open();
		}

	protected:
		asio::ip::tcp::socket m_socket;
		asio::io_context& m_context;
		tc::Queue<Message<T>> m_messagesOut;
		tc::Queue<OwnedMessage<T>>& m_messagesIn;
		Owner m_owner = Owner::server;
		std::string m_id = 0;
	};
}
