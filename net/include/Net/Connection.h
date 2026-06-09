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
		Connection(Owner owner, asio::io_context& context, asio::ip::tcp::socket socket)
			: m_owner{ owner }, m_context{ context }, m_socket(std::move(socket))
		{}
		virtual ~Connection()
		{}

		std::function<void(std::shared_ptr<Connection<T>>)> onDisconnect;
		std::function<void(OwnedMessage<T>)> onMessageReceived;
		const std::string& getUuid() const { return m_uuid; }
		const std::string& getName() const { return m_name; }

		asio::awaitable<bool> connectToClient()
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
				asio::steady_timer timer(m_context);
				timer.expires_after(std::chrono::seconds(5)); // 5-second handshake timeout

				uint64_t magic = 0x1234567890ABCDEF;
				co_await (asio::async_write(m_socket, asio::buffer(&magic, sizeof(magic)), asio::use_awaitable) || timer.async_wait(asio::use_awaitable));

				uint64_t client_magic = 0;
				co_await (asio::async_read(m_socket, asio::buffer(&client_magic, sizeof(client_magic)), asio::use_awaitable) || timer.async_wait(asio::use_awaitable));

				if (client_magic != (magic ^ 0xDEADBEEF))
				{
					tc::log::error("Handshake failed: invalid magic response");
					co_return false;
				}

				uint32_t uuidLen = 0;
				co_await (asio::async_read(m_socket, asio::buffer(&uuidLen, sizeof(uuidLen)), asio::use_awaitable) || timer.async_wait(asio::use_awaitable));
				if (uuidLen > 512)
				{
					tc::log::error("Handshake failed: UUID length too large ({})", uuidLen);
					co_return false;
				}
				m_uuid.resize(uuidLen);
				co_await (asio::async_read(m_socket, asio::buffer(m_uuid.data(), uuidLen), asio::use_awaitable) || timer.async_wait(asio::use_awaitable));

				uint32_t nameLen = 0;
				co_await (asio::async_read(m_socket, asio::buffer(&nameLen, sizeof(nameLen)), asio::use_awaitable) || timer.async_wait(asio::use_awaitable));
				if (nameLen > 128)
				{
					tc::log::error("Handshake failed: Name length too large ({})", nameLen);
					co_return false;
				}
				m_name.resize(nameLen);
				co_await (asio::async_read(m_socket, asio::buffer(m_name.data(), nameLen), asio::use_awaitable) || timer.async_wait(asio::use_awaitable));
			}
			catch (const std::exception& e)
			{
				tc::log::error("Handshake error: {}", e.what());
				co_return false;
			}

			asio::co_spawn(m_socket.get_executor(), [self = this->shared_from_this()]() -> asio::awaitable<void> {
				co_await self->readMessage();
			}, asio::detached);
			tc::log::info("Connection validated for UID: {}", m_uuid);
			co_return true;
		}

		asio::awaitable<bool> connectToServer(const asio::ip::tcp::resolver::results_type& endpoints, const std::string& uuid, const std::string& name)
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
				asio::steady_timer timer(m_context);
				timer.expires_after(std::chrono::seconds(5)); // 5-second handshake timeout

				uint64_t magic = 0;
				co_await (asio::async_read(m_socket, asio::buffer(&magic, sizeof(magic)), asio::use_awaitable) || timer.async_wait(asio::use_awaitable));

				magic ^= 0xDEADBEEF;
				co_await (asio::async_write(m_socket, asio::buffer(&magic, sizeof(magic)), asio::use_awaitable) || timer.async_wait(asio::use_awaitable));

				uint32_t uuidLen = static_cast<uint32_t>(uuid.size());
				co_await (asio::async_write(m_socket, asio::buffer(&uuidLen, sizeof(uuidLen)), asio::use_awaitable) || timer.async_wait(asio::use_awaitable));
				co_await (asio::async_write(m_socket, asio::buffer(uuid.data(), uuidLen), asio::use_awaitable) || timer.async_wait(asio::use_awaitable));

				uint32_t nameLen = static_cast<uint32_t>(name.size());
				co_await (asio::async_write(m_socket, asio::buffer(&nameLen, sizeof(nameLen)), asio::use_awaitable) || timer.async_wait(asio::use_awaitable));
				co_await (asio::async_write(m_socket, asio::buffer(name.data(), nameLen), asio::use_awaitable) || timer.async_wait(asio::use_awaitable));
			}
			catch (const std::exception& e)
			{
				tc::log::error("Handshake error: {}", e.what());
				co_return false;
			}

			asio::co_spawn(m_socket.get_executor(), [self = this->shared_from_this()]() -> asio::awaitable<void> {
				co_await self->readMessage();
			}, asio::detached);
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
			asio::post(m_context, [self = this->shared_from_this(), msg = std::move(msg)]() mutable {
				bool wasWriting = false;
				{
					std::scoped_lock lock(self->m_writingMtx);
					wasWriting = self->m_writingMessage;
					self->m_messagesOut.pushBack(std::move(msg));
					if (!wasWriting)
						self->m_writingMessage = true;
				}
				if (!wasWriting)
				{
					asio::co_spawn(self->m_context, [self]() -> asio::awaitable<void> {
						co_await self->writeMessage();
					}, asio::detached);
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
					if (onMessageReceived)
						onMessageReceived(std::move(owned));
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
					tc::log::error("Connection read error: {} [code: {}]", e.what(), e.code().value());
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

			if (onDisconnect)
				onDisconnect(this->shared_from_this());

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
		Owner m_owner = Owner::Server;
		std::string m_uuid;
		std::string m_name;
		std::mutex m_writingMtx;
		bool m_writingMessage = false;
	};
}
