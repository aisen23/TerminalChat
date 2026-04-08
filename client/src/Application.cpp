#include "pch.h"
#include "Application.h"

using json = nlohmann::json;

namespace tc
{
	namespace fs = std::filesystem;
	int Application::run()
	{
		using namespace std::chrono_literals;
		log::info("Starting client application...");
		init();
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
		tc::log::info("NetClient is shutdown.");

		return 0;
	}

	void Application::checkNetClient()
	{
		static bool reported = false;
		if (!m_netClient->isConnected())
		{
			if (m_netClient->isConnectible())
			{
				reported = false;
				m_netClient->connect();
			}
			else
			{
				if (!reported)
				{
					reported = true;
					tc::log::warn("Incorrect host ip={} or port={}", m_netClient->ip(), m_netClient->port());
				}
				m_netClient->stop();
			}
		}
	}

	void Application::init()
	{
		m_netClient = std::make_unique<NetClient>(*this);
		loadData();

		if (m_name.empty())
		{
			std::string name;
			while (name.empty())
			{
				std::println("Enter your name:");
				std::getline(std::cin, name);
			}
			setName(name);
		}
	}

	void Application::startInputWorker()
	{
		m_inputWorker = std::jthread([this] {
			std::string line;
			while (std::getline(std::cin, line))
			{
				while (!line.empty() && (line.back() == '\r' || line.back() == '\n' || line.back() == ' '))
					line.pop_back();

				if (line.empty())
					continue;

				if (line.starts_with("!connect"))
					connect(std::move(line));
				else if ("!disconnect" == line)
					pushTask([this] { m_netClient->stop(); });
				else if (isIn(line, "!quit", "!exit"))
				{
					stop();
					break;
				}
				else if (line.starts_with("!setname"))
				{
					auto parts = utils::split(line, ' ');
					std::string newName = parts.size() > 1 ? parts.at(1) : std::string{};
					if (!newName.empty())
						pushTask([this, newName = std::move(newName)]() mutable { setName(std::move(newName)); });
				}
				else
					m_netClient->send(std::move(line));
			}
			});
	}

	void Application::onReceive(net::Message<MsgTypes> msg)
	{
		std::println("Received message: id={}, size={}", static_cast<int>(msg.header.id), msg.size());
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

	void Application::saveData()
	{
		auto dir = utils::writableConfigPath();
		std::ofstream file{ dir / "client_data.json" };
		if (!file.is_open())
		{
			std::println("[ERROR] Failed to open file.");
			return;
		}

		json j;
		j["name"] = m_name;
		file << j.dump(4);
	}

	void Application::loadData()
	{
		auto dir = utils::writableConfigPath();
		std::ifstream file( dir / "client_data.json");
		if (!file.is_open())
		{
			std::println("[ERROR] Failed to open file.");
			return;
		}

		json j;
		file >> j;

		m_name = j["name"];
	}

	void Application::setName(std::string name)
	{
		m_name = std::move(name);
		net::Message<MsgTypes> msg{ MsgTypes::ChangeNickname };
		msg << m_name;
		//if (m_netClient->isConnected())
		//	m_netClient->send(std::move(msg));

		saveData();
	}

	void Application::connect(std::string&& data)
	{
		auto printFormatError = [] { tc::log::warn("Use format !connect <ip>:<port>"); };

		auto parts = utils::split(data, ' ');
		if (parts.size() < 2)
		{
			printFormatError();
			return;
		}

		parts = utils::split(parts.at(1), ':');
		if (parts.size() < 2)
		{
			printFormatError();
			return;
		}

		std::string host = parts.at(0);
		const std::string portStr = parts.at(1);
		if (!portStr.empty())
		{
			try
			{
				std::size_t pos;
				const uint32_t port = std::stoul(portStr, &pos);
				if (pos != portStr.size())
					throw std::runtime_error("Extra character detected.");

				pushTask([this, host = std::move(host), port] { m_netClient->setFullAddress(std::move(host), port); m_netClient->start(); });
			}
			catch (const std::exception& e)
			{
				tc::log::warn("Incorrect port={}, details: {}", portStr, e.what());
			}
		}
	}
}
