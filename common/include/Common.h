#pragma once

namespace tc
{
	template <typename T, typename... Args>
	bool TC_COMMON_API isIn(T value, Args... args)
	{
		return ((value == args) || ...);
	}
}

