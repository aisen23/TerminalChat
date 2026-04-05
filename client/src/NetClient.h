#pragma once

namespace tc
{
	namespace asio = boost::asio;
	class NetClientHandler
	{
	public:
		virtual void onReceive(net::Message<MsgTypes> msg) = 0;
		virtual void pushTask(std::move_only_function<void()> task) = 0;
	};

	class NetClient
	{
	public:
		NetClient(NetClientHandler& handler);
		~NetClient();

		void start();
		void stop();

		void connect();
		void send(std::string message);

		bool isConnected();
		bool isConnectible();
		const std::string& ip();
		uint32_t port();

		void setFullAddress(std::string ip, uint32_t port);

	private:
		asio::awaitable<void> connectImpl();
		void receiveMsgLoop();

	private:
		NetClientHandler& m_handler;
		asio::io_context m_context;
		asio::executor_work_guard<asio::io_context::executor_type> m_workGuard;
		std::jthread m_worker;
		std::jthread m_recvWorker;

		std::shared_ptr<net::Connection<MsgTypes>> m_connection;
		Queue<net::OwnedMessage<MsgTypes>> m_messagesIn;
		std::string m_ip;
		uint32_t m_port{};
		std::mutex m_mtx;
		bool m_connecting{ false };
	};
}
