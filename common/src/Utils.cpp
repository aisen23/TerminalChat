#include "pch.h"
#include "Common/Utils.h"

#include "Common/ScopeGuard.h"

namespace tc::utils
{
	namespace fs = std::filesystem;
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

	std::filesystem::path TC_COMMON_API writableConfigPath()
	{
		fs::path configPath;
#ifdef _WIN32
		char* localAppData = nullptr;
		auto guard = makeScopeGuard([&localAppData] {
			if (localAppData)
				free(localAppData);
			});
		size_t len = 0;
		errno_t err = _dupenv_s(&localAppData, &len, "LOCALAPPDATA");
		if (err == 0 && localAppData != nullptr)
			configPath = fs::path(localAppData);
#else
		const char* home = std::getenv("HOME");
		configPath = fs::path(home) / ".config";
#endif
		configPath /= "TerminalChat";
		if (!fs::exists(configPath))
			fs::create_directories(configPath);

		return configPath;
	}

	std::vector<std::string> TC_COMMON_API split(std::string_view text, char delimeter)
	{
		auto parts = text
			| std::views::split(delimeter)
			| std::views::filter([](auto part) { return !std::ranges::empty(part); });

		std::vector<std::string> res;
		for (auto part : parts)
			res.emplace_back(part.begin(), part.end());

		return res;
	}
}
