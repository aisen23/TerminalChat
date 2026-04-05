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
		std::println("Starting server on port 8080...");
		m_netServer = std::make_unique<NetServer>(*this, 8080);
		m_netServer->start();
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

	void Application::onReceive(std::shared_ptr<net::Connection<MsgTypes>> client, net::Message<MsgTypes> msg)
	{
		pushTask([this, client, msg = std::move(msg)]() mutable {
			switch (msg.header.id)
			{
			case MsgTypes::ChatText:
			{
				std::string text;
				msg >> text;
				std::println("Chat from {}: {}", client->GetId(), text);

				net::Message<MsgTypes> broadcastMsg;
				broadcastMsg.header.id = MsgTypes::ChatText;
				std::string outgoing = std::format("[{}]: {}", client->GetId(), text);
				broadcastMsg << outgoing;
				m_netServer->sendToAll(std::move(broadcastMsg), client);
				break;
			}
			case MsgTypes::ChangeUuid:
			{
				std::string newName;
				msg >> newName;
				std::println("User {} changed name to {}", client->GetId(), newName);
				// In a real server we'd set the nickname, for now we just acknowledge.
				break;
			}
			case MsgTypes::Ping:
			{
				std::println("Ping from {}", client->GetId());
				client->send(std::move(msg));
				break;
			}
			}
		});
	}

	void Application::onClientConnect(std::shared_ptr<net::Connection<MsgTypes>> client)
	{
		pushTask([this, client]() {
			std::println("New client connected: {}", client->GetId());
		});
	}

	void Application::onClientDisconnect(std::shared_ptr<net::Connection<MsgTypes>> client)
	{
		pushTask([this, client]() {
			std::println("Client disconnected: {}", client->GetId());
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
