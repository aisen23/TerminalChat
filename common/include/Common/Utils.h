#pragma once
#include "CommonApi.h"

#include <filesystem>
#include <string>
#include <string_view>

namespace tc::utils
{
	bool TC_COMMON_API isIpValid(std::string_view ip);
	bool TC_COMMON_API isPortValid(uint32_t);
	std::filesystem::path TC_COMMON_API writableConfigPath();
	std::vector<std::string> TC_COMMON_API split(std::string_view text, char delimeter);
}

