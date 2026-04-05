#pragma once
#include "CommonApi.h"

#include <string>
#include <string_view>
#include <format>
#include <print>
#include <source_location>
#include <type_traits>

namespace tc::log
{
	enum class Level { Debug, Info, Warn, Error, Fatal };

	TC_COMMON_API void init(const std::string& appName);
	TC_COMMON_API void setLevel(Level level);
	TC_COMMON_API Level getLevel();
	TC_COMMON_API void logMessage(Level level, const std::source_location& loc, std::string_view msg);

	template <class... Args>
	struct FormatLocation {
		std::format_string<Args...> fmt;
		std::source_location loc;

		template <class String>
			requires std::is_constructible_v<std::format_string<Args...>, const String&>
		consteval FormatLocation(const String& fmt_str, std::source_location loc = std::source_location::current())
			: fmt{fmt_str}, loc{loc} {}
	};

	template<typename... Args>
	void debug(FormatLocation<std::type_identity_t<Args>...> fmt, Args&&... args)
	{
		if (getLevel() <= Level::Debug)
			logMessage(Level::Debug, fmt.loc, std::format(fmt.fmt, std::forward<Args>(args)...));
	}

	template<typename... Args>
	void info(FormatLocation<std::type_identity_t<Args>...> fmt, Args&&... args)
	{
		if (getLevel() <= Level::Info)
			logMessage(Level::Info, fmt.loc, std::format(fmt.fmt, std::forward<Args>(args)...));
	}

	template<typename... Args>
	void warn(FormatLocation<std::type_identity_t<Args>...> fmt, Args&&... args)
	{
		if (getLevel() <= Level::Warn)
			logMessage(Level::Warn, fmt.loc, std::format(fmt.fmt, std::forward<Args>(args)...));
	}

	template<typename... Args>
	void error(FormatLocation<std::type_identity_t<Args>...> fmt, Args&&... args)
	{
		if (getLevel() <= Level::Error)
			logMessage(Level::Error, fmt.loc, std::format(fmt.fmt, std::forward<Args>(args)...));
	}

	template<typename... Args>
	void fatal(FormatLocation<std::type_identity_t<Args>...> fmt, Args&&... args)
	{
		if (getLevel() <= Level::Fatal)
			logMessage(Level::Fatal, fmt.loc, std::format(fmt.fmt, std::forward<Args>(args)...));
	}

	TC_COMMON_API void printMessage(std::string_view name, std::string_view msg);

	template<typename... Args>
	void print(std::format_string<std::type_identity_t<Args>...> fmt, Args&&... args)
	{
		printMessage("", std::format(fmt, std::forward<Args>(args)...));
	}

	template<typename... Args>
	void printAs(std::string_view name, std::format_string<std::type_identity_t<Args>...> fmt, Args&&... args)
	{
		printMessage(name, std::format(fmt, std::forward<Args>(args)...));
	}
}
