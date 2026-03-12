#include "pch.h"
#include "Application.h"

namespace tc
{
	int Application::run()
	{
		using namespace std::chrono_literals;
		startInputWorker();

		while (true)
		{
			std::unique_lock lock(m_mtx);

			checkNetClient();

			m_cv.wait_for(lock, 1s, [this] { return m_stop || !m_tasks.empty(); });

			if (m_stop)
				break;

			if (!m_tasks.empty())
			{
				auto task = m_tasks.popFront();
				(*task)();
			}
		}

		m_netClient.reset();
		std::println("NetClient is shutdowned.");
	}

	void Application::checkNetClient()
	{
		if (!m_netClient->isConnected())
		{
			if (m_netClient->isConnectible())
				m_netClient->connect();
			else
			{
				std::println("Incorrect host ip={} or port={}", m_netClient->ip(), m_netClient->port());
				m_netClient->stop();
			}
		}
	}

	void Application::startInputWorker()
	{
		m_inputWorker = std::jthread([this] {
			std::string line;
			while (std::getline(std::cin, line))
			{
				if (line.empty())
					continue;

				if (line.starts_with("!connect"))
				{
					const std::string fullAddr = line.substr(9);
					size_t colonPos = fullAddr.find(':');

					if (colonPos != std::string::npos)
					{
						const std::string host = fullAddr.substr(0, colonPos);
						const std::string portStr = fullAddr.substr(colonPos + 1);
						if (!portStr.empty())
						{
							try
							{
								std::size_t pos;
								const uint32_t port = std::stoul(portStr, &pos);
								if (pos != portStr.size())
									throw std::runtime_error("Extra character detected.");

								pushTask([this, host = std::move(host), port] { m_netClient->setFullAddress(host, port); m_netClient->start(); });
							}
							catch (const std::exception& e)
							{
								std::println("Incorrect port={}, details: {}", portStr, e.what());
							}
						}
					}
					else
					{
						std::println("Error: Use format !connect <ip>:<port>");
					}
				}
				else if ("!disconnect" == line)
					pushTask([this] { m_netClient->stop(); });
				else if (isIn(line, "!quit", "!exit"))
					stop();
				else
					m_netClient->send(line);
			}
			});
	}

	void Application::onReceive(net::Message<MsgTypes> msg)
	{
	}
	
	void Application::pushTask(std::move_only_function<void()> task)
	{
		std::scoped_lock lock(m_mtx);
		m_tasks.pushBack(std::move(task));
		m_cv.notify_one();
	}

	void Application::stop()
	{
		std::println("Stop requested...");
		std::scoped_lock lock(m_mtx);
		m_stop = true;
		m_cv.notify_one();
	}
}
