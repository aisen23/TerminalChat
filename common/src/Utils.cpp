#include "pch.h"
#include "Common/Utils.h"

namespace tc::utils
{
	bool TC_COMMON_API tc::utils::isIpValid(std::string_view ip)
	{
		if (ip.empty())
			return false;

		boost::system::error_code ec;
		boost::asio::ip::make_address_v4(ip, ec);
		return !ec;
	}

	bool TC_COMMON_API isPortValid(uint32_t port)
	{
		return port > 2000 && port <= 65535;
	}
}
