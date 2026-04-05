#pragma once

namespace tc
{
	namespace asio = boost::asio;
	class NetServerHandler
	{
	public:
		virtual void onReceive(std::shared_ptr<net::Connection<MsgTypes>> client, net::Message<MsgTypes> msg) = 0;
		virtual void pushTask(std::move_only_function<void()> task) = 0;
		virtual void onClientConnect(std::shared_ptr<net::Connection<MsgTypes>> client) = 0;
		virtual void onClientDisconnect(std::shared_ptr<net::Connection<MsgTypes>> client) = 0;
	};

	class NetServer
	{
	public:
		NetServer(NetServerHandler& handler, uint32_t port);
		~NetServer();

		void start();
		void stop();

		void sendToClient(std::shared_ptr<net::Connection<MsgTypes>> client, net::Message<MsgTypes> msg);
		void sendToAll(net::Message<MsgTypes> msg, std::shared_ptr<net::Connection<MsgTypes>> ignoreClient = nullptr);

	private:
		asio::awaitable<void> acceptLoop();
		void receiveMsgLoop();

	private:
		NetServerHandler& m_handler;
		asio::io_context m_context;
		asio::executor_work_guard<asio::io_context::executor_type> m_workGuard;
		asio::ip::tcp::acceptor m_acceptor;
		std::vector<std::jthread> m_workers;
		std::jthread m_recvWorker;

		Queue<net::OwnedMessage<MsgTypes>> m_messagesIn;
		std::vector<std::shared_ptr<net::Connection<MsgTypes>>> m_connections;
		std::mutex m_mtx;
	};
}
