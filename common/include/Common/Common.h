#pragma once

namespace tc
{
	enum class MsgTypes
	{
		Ping,
		ChatText,
		ChangeUuid,
	};


	template <typename T, typename... Args>
	bool isIn(T value, Args... args)
	{
		return ((value == args) || ...);
	}
}

