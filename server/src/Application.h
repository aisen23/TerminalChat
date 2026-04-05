#pragma once
#include "NetServer.h"

namespace tc
{
	class Application : public NetServerHandler
	{
	public:
		Application() = default;
		int run();

		void onReceive(std::shared_ptr<net::Connection<MsgTypes>> client, net::Message<MsgTypes> msg) override;
		void onClientConnect(std::shared_ptr<net::Connection<MsgTypes>> client) override;
		void onClientDisconnect(std::shared_ptr<net::Connection<MsgTypes>> client) override;
		void pushTask(std::move_only_function<void()> task) override;

		void stop();

	private:
		void init();
		void startInputWorker();

		std::unique_ptr<NetServer> m_netServer;
		
		std::mutex m_mtx;
		std::condition_variable m_cv;
		bool m_stop = false;

		tc::Queue<std::move_only_function<void()>> m_tasks;
		std::jthread m_inputWorker;
	};
}
