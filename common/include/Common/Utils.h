#pragma once
#include "CommonApi.h"

namespace tc::utils
{
	bool TC_COMMON_API isIpValid(std::string_view ip);
	bool TC_COMMON_API isPortValid(uint32_t);
}

