#include "pch.h"
#include "Application.h"
#include "NetServer.h"

namespace tc
{
	int Application::run()
	{
		using namespace std::chrono_literals;
		log::info("Starting server application...");

		init();
		startInputWorker();

		while (true)
		{
			std::unique_lock lock(m_mtx);

			m_cv.wait_for(lock, 1s, [this] { return m_stop || !m_tasks.empty(); });

			if (m_stop)
				break;

			if (!m_tasks.empty())
			{
				auto task = m_tasks.popFront();
				if (task)
					(*task)();
			}
		}

		m_netServer.reset();
		std::println("[INFO] NetServer shutdown complete.");

		return 0;
	}

	void Application::init()
	{
		uint32_t port = 0;
		while (port == 0)
		{
			std::print("Enter port number (default {}): ", MIN_PORT);
			std::string line;
			std::getline(std::cin, line);
			if (line.empty())
			{
				port = MIN_PORT;
				break;
			}
			try
			{
				port = std::stoul(line);
				if (!utils::isPortValid(port))
				{
					std::println("Port must be a valid user port between {} and {}.", MIN_PORT, MAX_PORT);
					port = 0;
				}
			}
			catch (...)
			{
				std::println("Invalid input. Please enter a valid numeric port.");
			}
		}

		while (port <= MAX_PORT)
		{
			try
			{
				m_netServer = std::make_unique<NetServer>(*this, port);
				m_netServer->start();

				std::println("Server started. Listening on:");
				for (const auto& ip : net::getLocalIpAddresses())
				{
					std::println("  {}:{}", ip, port);
				}

				return;
			}
			catch (const std::exception& e)
			{
				log::warn("Port {} is busy ({}), trying next...", port, e.what());
				port++;
			}
		}

		log::fatal("Unable to start server: no available ports found in range.");
		stop();
	}

	void Application::startInputWorker()
	{
		m_inputWorker = std::jthread([this] {
			std::string line;
			while (std::getline(std::cin, line))
			{
				if (line.empty())
					continue;

				if (isIn(line, "!quit", "!exit"))
				{
					stop();
					break;
				}
				else if (line.starts_with("!kick"))
				{
					// Optional kick command
				}
				else
				{
					// Server global message
					pushTask([this, msgText = std::move(line)]() mutable {
						net::Message<MsgTypes> msg;
						msg.header.id = MsgTypes::ChatText;
						std::string prefix = "[SERVER] ";
						msgText = prefix + msgText;
						msg << msgText;
						m_netServer->sendToAll(std::move(msg));
					});
				}
			}
		});
	}

	void Application::onReceive(std::shared_ptr<Client> client, net::Message<MsgTypes> msg)
	{
		pushTask([this, client, msg = std::move(msg)]() mutable {
			switch (msg.header.id)
			{
			case MsgTypes::ChatText:
			{
				std::string text;
				msg >> text;
				std::println("Chat from {}: {}", client->name, text);

				net::Message<MsgTypes> broadcastMsg;
				broadcastMsg.header.id = MsgTypes::ChatText;
				std::string outgoing = std::format("[{}]: {}", client->name, text);
				broadcastMsg << outgoing;
				m_netServer->sendToAll(std::move(broadcastMsg), client);
				break;
			}
			case MsgTypes::ChangeNickname:
			{
				std::string newName;
				msg >> newName;
				std::println("User {} changed nickname to {}", client->name, newName);
				client->name = newName;
				// In a real server we'd set the nickname, for now we just acknowledge.
				break;
			}
			case MsgTypes::Ping:
			{
				std::println("Ping from {}", client->name);
				if (client->connection && client->connection->isConnected())
					client->connection->send(std::move(msg));
				break;
			}
			}
		});
	}

	void Application::onClientConnect(std::shared_ptr<Client> client)
	{
		pushTask([this, client]() {
			std::println("New client connected: {}", client->name);
			m_netServer->sendToClient(client, net::Message<MsgTypes>{ MsgTypes::ConnectionAccepted });
		});
	}

	void Application::onClientDisconnect(std::shared_ptr<Client> client)
	{
		pushTask([this, client]() {
			std::println("Client disconnected: {}", client->name);
		});
	}

	void Application::pushTask(std::move_only_function<void()> task)
	{
		std::scoped_lock lock(m_mtx);
		m_tasks.pushBack(std::move(task));
		m_cv.notify_one();
	}

	void Application::stop()
	{
		std::println("Server stop requested...");
		std::scoped_lock lock(m_mtx);
		m_stop = true;
		m_cv.notify_one();
	}
}
