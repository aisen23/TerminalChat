#include "pch.h"
#include "Application.h"
#include "Utils.h"

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
				if (task)
				{
					lock.unlock();
					(*task)();
					lock.lock();
				}
			}
		}

		m_netClient.reset();
		tc::log::info("NetClient is shutdown.");

		return 0;
	}

	void Application::checkNetClient()
	{
		static bool reported = false;
		if (!m_netClient->isConnecting() && !m_netClient->isConnected())
		{
			if (m_netClient->isConnectible())
			{
				reported = false;
				m_netClient->start();
				m_netClient->connect(m_uuid, m_name);
			}
			else
			{
				if (!reported && !m_netClient->ip().empty())
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

		bool stateChanged = false;
		if (m_uuid.empty())
		{
			m_uuid = utils::generateUUID();
			log::debug("Generated UUID: {}", m_uuid);
			stateChanged = true;
		}

		if (m_name.empty())
		{
			std::string name;
			while (name.empty())
			{
				std::print("Enter your name: ");
				std::getline(std::cin, name);
			}
			stateChanged |= setName(name);
		}

		if (stateChanged)
			saveData();
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

				bool format = true;
				if (line.starts_with("!connect"))
					connect(std::move(line));
				else if ("!disconnect" == line)
					pushTask([this] { m_netClient->disconnect(); saveData(); });
				else if (isIn(line, "!quit", "!exit"))
				{
					stop();
					format = false;
				}
				else if (line.starts_with("!setname"))
				{
					auto parts = utils::split(line, ' ');
					std::string newName = parts.size() > 1 ? parts.at(1) : std::string{};
					if (!newName.empty())
						pushTask([this, newName = std::move(newName)]() mutable {
							if (setName(std::move(newName)))
							{
								saveData();
								sendName();
							}
						});
				}
				else if (line == "!ping")
				{
					net::Message<MsgTypes> msg{ MsgTypes::Ping };
					m_pingTime = std::chrono::steady_clock::now();
					format = m_netClient->send(std::move(msg));
				}
				else
					format = m_netClient->send(line);

				if (format)
					formatText(line, true);
			}});
	}

	void Application::onReceive(net::Message<MsgTypes> msg)
	{
		tc::log::debug("Received message: id={}, size={}", static_cast<int>(msg.header.id), msg.size());
		switch (msg.header.id) {
		case MsgTypes::ChatText:
		{
			std::string text;
			msg >> text;
			formatText(text, false);
			break;
		}
		case MsgTypes::Ping:
		{
			std::chrono::duration<double, std::milli> latency = std::chrono::steady_clock::now() - m_pingTime;
			std::string text = std::format("Pong! Latency: {:.2f} ms", latency.count());
			formatText(text, false);
			break;
		}
		case MsgTypes::ConnectionAccepted:
		{
			log::info("Connection accepted!");
			log::printMessage("SERVER", "You are in chat!");
			break;
		}
		}
	}

	void Application::pushTask(std::move_only_function<void()> task)
	{
		std::scoped_lock lock(m_mtx);
		m_tasks.pushBack(std::move(task));
		m_cv.notify_one();
	}

	void Application::stop()
	{
		tc::log::info("Stop requested...");
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
			tc::log::error("Failed to open file.");
			return;
		}

		json j;
		j["name"] = m_name;
		j["uuid"] = m_uuid;
		if (m_netClient)
		{
			j["ip"] = m_netClient->ip();
			j["port"] = m_netClient->port();
		}
		file << j.dump(4);
	}

	void Application::loadData()
	{
		auto dir = utils::writableConfigPath();
		std::ifstream file( dir / "client_data.json");
		if (!file.is_open())
		{
			tc::log::warn("Failed to open file.");
			return;
		}

		json j;
		file >> j;

		if (j.contains("name"))
			m_name = j["name"];

		if (j.contains("uuid"))
			m_uuid = j["uuid"];

		if (j.contains("ip") && j.contains("port"))
		{
			std::string ip = j["ip"];
			uint32_t port = j["port"];
			if (!ip.empty() && port != 0)
				m_netClient->setFullAddress(std::move(ip), port);
		}
	}

	bool Application::setName(std::string name)
	{
		if (name.empty() || m_name == name)
			return false;

		return std::exchange(m_name, name) != m_name;
	}

	void Application::sendName()
	{
		if (m_name.empty())
		{
			log::warn("Name cannot be empty.");
			return;
		}

		net::Message<MsgTypes> msg{ MsgTypes::ChangeNickname };
		msg << m_name;

		if (m_netClient->isConnected())
			m_netClient->send(std::move(msg));
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

				pushTask([this, host = std::move(host), port] {
					m_netClient->setFullAddress(std::move(host), port);
					saveData();
				});
			}
			catch (const std::exception& e)
			{
				tc::log::warn("Incorrect port={}, details: {}", portStr, e.what());
			}
		}
	}

	void Application::formatText(const std::string& text, bool addMe)
	{
		auto now = std::chrono::system_clock::now();
		auto timeT = std::chrono::system_clock::to_time_t(now);
		std::tm tm{};
#ifdef _WIN32
		localtime_s(&tm, &timeT);
#else
		localtime_r(&timeT, &tm);
#endif
		pushTask([tm, text = std::string(text), addMe] {
			std::print("\x1b[1A\x1b[2K\r[{:02}:{:02}:{:04} {:02}:{:02}:{:02}] {}{}\n", tm.tm_mon + 1, tm.tm_mday, tm.tm_year + 1900, tm.tm_hour, tm.tm_min, tm.tm_sec, addMe ? "[Me] : " : "", text);
			});
	}
}
