#pragma once
#include "NetClient.h"

#include <Common/Queue.h>

#include <functional>

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
		void startInputWorker();

		void onReceive(net::Message<MsgTypes> msg) override;
		void pushTask(std::move_only_function<void()> task) override;

		void stop();

	private:
		std::unique_ptr<NetClient> m_netClient;
		bool m_stop{ false };

		std::mutex m_mtx;
		std::condition_variable m_cv;
		Queue<Task> m_tasks;
		std::jthread m_inputWorker;
	};
}
