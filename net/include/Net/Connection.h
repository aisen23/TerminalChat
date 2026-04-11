#pragma once

#include "Message.h"

#include <Common/Queue.h>
#include <Common/Log.h>

#include <boost/asio.hpp>

#include <memory>
#include <print>
#include <string>

namespace net
{
	namespace asio = boost::asio;

	std::vector<std::string> getLocalIpAddresses();

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
				std::println("Server cannot connect to itself");
				co_return false;
			}

			if (!m_socket.is_open())
			{
				std::println("Failed to connect to client: socket is closed");
				co_return false;
			}

			try
			{
				uint64_t magic = 0x1234567890ABCDEF;
				co_await asio::async_write(m_socket, asio::buffer(&magic, sizeof(magic)), asio::use_awaitable);

				uint64_t client_magic = 0;
				co_await asio::async_read(m_socket, asio::buffer(&client_magic, sizeof(client_magic)), asio::use_awaitable);

				if (client_magic != (magic ^ 0xDEADBEEF))
				{
					tc::log::error("Handshake failed for client {}", uid);
					co_return false;
				}
			}
			catch (const std::exception& e)
			{
				tc::log::error("Handshake error: {}", e.what());
				co_return false;
			}

			m_id = uid;
			auto executor = co_await asio::this_coro::executor;
			asio::co_spawn(executor, readMessage(), asio::detached);
			tc::log::info("Connection validated for UID: {}", m_id);
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
				tc::log::error("Failed to connect to endpoint: {}", ec.message());
				co_return false;
			}

			try
			{
				uint64_t magic = 0;
				co_await asio::async_read(m_socket, asio::buffer(&magic, sizeof(magic)), asio::use_awaitable);

				magic ^= 0xDEADBEEF;
				co_await asio::async_write(m_socket, asio::buffer(&magic, sizeof(magic)), asio::use_awaitable);
			}
			catch (const std::exception& e)
			{
				tc::log::error("Handshake error: {}", e.what());
				co_return false;
			}

			auto executor = co_await asio::this_coro::executor;
			asio::co_spawn(executor, readMessage(), asio::detached);
			co_return true;
		}

		void disconnect()
		{
			if (isConnected())
				asio::post(m_context, [this]() { m_socket.close(); });
		}

		bool isConnected() const
		{
			return m_socket.is_open();
		}

		void send(Message<T> msg)
		{
			asio::post(m_context, [this, msg = std::move(msg)]() mutable {
				bool wasWriting = false;
				{
					std::scoped_lock lock(m_writingMtx);
					wasWriting = m_writingMessage;
					m_messagesOut.pushBack(std::move(msg));
					if (!wasWriting)
						m_writingMessage = true;
				}
				if (!wasWriting)
				{
					asio::co_spawn(m_context, writeMessage(), asio::detached);
				}
			});
		}

	private:
		asio::awaitable<void> readMessage()
		{
			try
			{
				for (;;)
				{
					if (!m_socket.is_open())
						co_return;

					MessageHeader<T> header{};
					co_await asio::async_read(m_socket, asio::buffer(&header, sizeof(header)), asio::use_awaitable);

					Message<T> msg;
					msg.header = header;

					if (msg.header.size > 0)
					{
						if (msg.header.size > MAX_MESSAGE_SIZE)
						{
							tc::log::error("Connection read error: message too large ({})", msg.header.size);
							boost::system::error_code ec;
							m_socket.close(ec);
							co_return;
						}

						msg.body.resize(msg.header.size);
						co_await asio::async_read(
							m_socket,
							asio::buffer(msg.body.data(), msg.body.size()),
							asio::use_awaitable);
					}

					OwnedMessage<T> owned;
					owned.remote = this->shared_from_this();
					owned.msg = std::move(msg);
					m_messagesIn.pushBack(std::move(owned));
				}
			}
			catch (const boost::system::system_error& e)
			{
				if (e.code() == asio::error::eof || 
					e.code() == asio::error::connection_reset || 
					e.code() == asio::error::connection_aborted ||
					e.code() == asio::error::operation_aborted ||
					e.code().value() == 1236 || e.code().value() == 10054 || e.code().value() == 995)
				{
					tc::log::info("Connection read closed by peer.");
				}
				else
				{
					tc::log::error("Connection read error: {} [code: {}]", e.what(), e.code().value());
				}
			}
			catch (const std::exception& e)
			{
				tc::log::error("Connection read error: {}", e.what());
			}

			if (m_socket.is_open())
			{
				boost::system::error_code ec;
				m_socket.close(ec);
			}

			co_return;
		}

		asio::awaitable<void> writeMessage()
		{
			try
			{
				for (;;)
				{
					if (!m_socket.is_open())
						break;

					Message<T> msg;
					{
						std::scoped_lock lock(m_writingMtx);
						auto optMsg = m_messagesOut.popFront();
						if (!optMsg)
						{
							m_writingMessage = false;
							break;
						}
						msg = std::move(*optMsg);
					}

					std::vector<asio::const_buffer> buffers;
					buffers.emplace_back(asio::buffer(&msg.header, sizeof(msg.header)));
					if (!msg.body.empty())
						buffers.emplace_back(asio::buffer(msg.body.data(), msg.body.size()));

					co_await asio::async_write(m_socket, buffers, asio::use_awaitable);
				}
			}
			catch (const boost::system::system_error& e)
			{
				if (e.code() == asio::error::eof || 
					e.code() == asio::error::connection_reset || 
					e.code() == asio::error::connection_aborted ||
					e.code() == asio::error::operation_aborted ||
					e.code().value() == 1236 || e.code().value() == 10054 || e.code().value() == 995)
				{
					tc::log::info("Connection write closed by peer.");
				}
				else
				{
					tc::log::error("Connection write error: {} [code: {}]", e.what(), e.code().value());
				}
				std::scoped_lock lock(m_writingMtx);
				m_writingMessage = false;
			}
			catch (const std::exception& e)
			{
				tc::log::error("Connection write error: {}", e.what());
				std::scoped_lock lock(m_writingMtx);
				m_writingMessage = false;
			}

			if (!m_socket.is_open())
			{
				boost::system::error_code ec;
				m_socket.close(ec);
			}

			co_return;
		}

	protected:
		asio::ip::tcp::socket m_socket;
		asio::io_context& m_context;
		tc::Queue<Message<T>> m_messagesOut;
		tc::Queue<OwnedMessage<T>>& m_messagesIn;
		Owner m_owner = Owner::Server;
		std::string m_id;
		std::mutex m_writingMtx;
		bool m_writingMessage = false;
	};
}
