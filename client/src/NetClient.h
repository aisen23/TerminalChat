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

		void connect(const std::string& uuid, const std::string& name);
		void disconnect();
		bool send(const std::string& message);
		bool send(net::Message<MsgTypes> msg);

		bool isConnected();
		bool isConnecting();
		bool isConnectible();
		const std::string& ip();
		uint32_t port();

		void setFullAddress(std::string ip, uint32_t port);

	private:
		asio::awaitable<void> connectImpl(const std::string& uuid, const std::string& name);

	private:
		NetClientHandler& m_handler;
		asio::io_context m_context;
		asio::executor_work_guard<asio::io_context::executor_type> m_workGuard;
		std::jthread m_worker;

		std::shared_ptr<net::Connection<MsgTypes>> m_connection;
		std::string m_ip;
		uint32_t m_port{};
		std::mutex m_mtx;
		bool m_connecting{ false };
	};
}
