#pragma once
#include <unordered_map>

namespace tc
{
	namespace asio = boost::asio;

	struct Client
	{
		std::shared_ptr<net::Connection<MsgTypes>> connection{};
		std::string name{};
		std::string uuid{};
	};

	class NetServerHandler
	{
	public:
		virtual void onReceive(std::shared_ptr<Client> client, net::Message<MsgTypes> msg) = 0;
		virtual void pushTask(std::move_only_function<void()> task) = 0;
		virtual void onClientConnect(std::shared_ptr<Client> client) = 0;
		virtual void onClientDisconnect(std::shared_ptr<Client> client) = 0;
	};

	class NetServer
	{
	public:
		NetServer(NetServerHandler& handler, uint32_t port);
		~NetServer();

		void start();
		void stop();

		void sendToClient(std::shared_ptr<Client> client, net::Message<MsgTypes> msg);
		void sendToAll(net::Message<MsgTypes> msg, std::shared_ptr<Client> ignoreClient = nullptr);

	private:
		asio::awaitable<void> acceptLoop();

	private:
		NetServerHandler& m_handler;
		asio::io_context m_context;
		asio::ip::tcp::acceptor m_acceptor;
		std::jthread m_worker;

		std::unordered_map<std::string, std::shared_ptr<Client>> m_clientMap;
		std::mutex m_mtx;
	};
}
