#pragma once

namespace tc
{
	enum class MsgTypes
	{
		Ping,
		ChatText,
		ChangeNickname,
	};

	constexpr int MIN_PORT = 40000;
	constexpr int MAX_PORT = 65535;

	template <typename T, typename... Args>
	bool isIn(T value, Args... args)
	{
		return ((value == args) || ...);
	}
}

