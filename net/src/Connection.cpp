#include "pch.h"
#include "Net/Connection.h"

namespace net
{
	std::vector<std::string> getLocalIpAddresses()
	{
		std::vector<std::string> ips;
		try
		{
			asio::io_context io_context;
			asio::ip::tcp::resolver resolver(io_context);
			auto results = resolver.resolve(asio::ip::host_name(), "");
			for (const auto& entry : results)
			{
				if (entry.endpoint().address().is_v4())
				{
					std::string ip = entry.endpoint().address().to_string();
					if (ip != "127.0.0.1")
					{
						ips.push_back(std::move(ip));
					}
				}
			}
		}
		catch (...) {}
		return ips;
	}
}
