#pragma once
#include "NetClient.h"

namespace tc
{
	namespace asio = boost::asio;
	using Task = std::move_only_function<void()>;
	class Application : public NetClientHandler
	{
	public:
		int run();

	private:
		void checkNetClient();
		void init();
		void startInputWorker();

		void onReceive(net::Message<MsgTypes> msg) override;
		void pushTask(std::move_only_function<void()> task) override;

		void stop();

		void saveData();
		void loadData();

		void setName(std::string name);
		void connect(std::string&& data);
		void formatText(const std::string& text, bool addMe);

	private:
		std::unique_ptr<NetClient> m_netClient;
		bool m_stop{ false };

		std::mutex m_mtx;
		std::condition_variable m_cv;
		Queue<Task> m_tasks;
		std::jthread m_inputWorker;
		std::string m_name;
		std::chrono::time_point<std::chrono::steady_clock> m_pingTime;
	};
}
