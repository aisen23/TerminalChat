#include "pch.h"
#include "Common/Log.h"
#include "Common/Utils.h"
#include "Common/Queue.h"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <unistd.h>
#include <limits.h>
#endif

#include <chrono>
#include <ctime>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <print>
#include <thread>

namespace tc::log
{
	static Level g_logLevel = Level::Info;
	static std::ofstream g_logFile;
	static size_t g_maxFileSizeBytes = 10 * 1024 * 1024;
	static size_t g_maxFiles = 3;
	static std::filesystem::path g_logFilePath;
	static std::string g_binName;
	static std::filesystem::path g_logsDir;

	struct LogState {
		tc::Queue<std::string> queue;
		std::jthread thread;

		~LogState() {
			queue.shutdown();
			if (thread.joinable())
				thread.join();
		}
	};
	static LogState g_logState;

	void init(std::string_view instanceId)
	{
		std::filesystem::path exePath;
#ifdef _WIN32
		char path[MAX_PATH];
		if (GetModuleFileNameA(NULL, path, MAX_PATH))
			exePath = path;
#else
		char path[PATH_MAX];
		ssize_t count = readlink("/proc/self/exe", path, PATH_MAX);
		if (count != -1)
			exePath = std::string(path, count);
#endif
		if (exePath.empty())
			throw std::runtime_error("Failed to get executable path.");

		auto binDir = exePath.parent_path();
		g_binName = exePath.filename().replace_extension("").string();

		auto configPath = binDir / (g_binName + ".LogConfig.txt");

		g_logsDir = tc::utils::writableConfigPath() / "logs";
		if (!std::filesystem::exists(g_logsDir))
			std::filesystem::create_directories(g_logsDir);

		if (std::filesystem::exists(configPath))
		{
			std::ifstream file(configPath);
			std::string line;
			while (std::getline(file, line))
			{
				if (line.starts_with("LogLevel=")) {
					if (line.find("Debug") != std::string::npos) g_logLevel = Level::Debug;
					else if (line.find("Info") != std::string::npos) g_logLevel = Level::Info;
					else if (line.find("Warn") != std::string::npos) g_logLevel = Level::Warn;
					else if (line.find("Error") != std::string::npos) g_logLevel = Level::Error;
					else if (line.find("Fatal") != std::string::npos) g_logLevel = Level::Fatal;
				} else if (line.starts_with("MaxFileSize=")) {
					try {
						auto val = std::stoull(line.substr(12));
						if (val > 0) g_maxFileSizeBytes = val * 1024 * 1024;
					} catch(...) {}
				} else if (line.starts_with("MaxFiles=")) {
					try {
						auto val = std::stoull(line.substr(9));
						if (val > 0) g_maxFiles = val;
					} catch(...) {}
				}
			}
		}
		else
		{
			std::ofstream file(configPath);
			file << "LogLevel=Info\nMaxFileSize=10\nMaxFiles=3\n";
			g_logLevel = Level::Info;
		}

		std::string logFile = instanceId.empty() ? g_binName + ".log" : std::format(g_binName + ".{}.log", instanceId);
		g_logFilePath = g_logsDir / logFile;
		g_logFile.open(g_logFilePath, std::ios::app);

		if (!g_logState.thread.joinable())
		{
			g_logState.thread = std::jthread([] {
				for (;;) {
					g_logState.queue.wait();
					auto optMsg = g_logState.queue.popFront();
					if (!optMsg) {
						if (g_logState.queue.stopped())
							break;
						continue;
					}

					if (g_logFile.is_open())
					{
						std::error_code ec;
						if (std::filesystem::file_size(g_logFilePath, ec) > g_maxFileSizeBytes)
						{
							g_logFile.close();

							for (size_t i = g_maxFiles - 1; i > 0; --i)
							{
								auto oldPath = g_logsDir / std::format("{}({}).log", g_binName, i);
								auto newPath = g_logsDir / std::format("{}({}).log", g_binName, i + 1);
								if (std::filesystem::exists(oldPath))
									std::filesystem::rename(oldPath, newPath, ec);
							}
							auto firstBackup = g_logsDir / std::format("{}(1).log", g_binName);
							if (std::filesystem::exists(g_logFilePath))
								std::filesystem::rename(g_logFilePath, firstBackup, ec);

							g_logFile.open(g_logFilePath, std::ios::app);
						}

						g_logFile << *optMsg << std::endl;
					}
				}
			});
		}
	}

	void shutdown()
	{
		g_logState.queue.shutdown();
		if (g_logState.thread.joinable())
			g_logState.thread.join();
		if (g_logFile.is_open())
			g_logFile.close();
	}

	void setLevel(Level level)
	{
		g_logLevel = level;
	}

	Level getLevel()
	{
		return g_logLevel;
	}

	void logMessage(Level level, const std::source_location& loc, std::string_view msg)
	{
		std::string_view levelStr;
		switch (level)
		{
		case Level::Debug: levelStr = "[DEBUG]"; break;
		case Level::Info: levelStr = "[INFO]"; break;
		case Level::Warn: levelStr = "[WARN]"; break;
		case Level::Error: levelStr = "[ERROR]"; break;
		case Level::Fatal: levelStr = "[FATAL]"; break;
		}

		auto now = std::chrono::system_clock::now();
		auto timeT = std::chrono::system_clock::to_time_t(now);
		auto sec = std::chrono::floor<std::chrono::seconds>(now);
		auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now - sec).count();

		std::tm tm{};
#ifdef _WIN32
		localtime_s(&tm, &timeT);
#else
		localtime_r(&timeT, &tm);
#endif

		std::string_view filepath = loc.file_name();

#ifdef SOLUTION_DIR
		std::string_view rootPath = SOLUTION_DIR;
		if (filepath.size() >= rootPath.size()) {
			bool match = true;
			for (size_t i = 0; i < rootPath.size(); ++i) {
				char c1 = filepath[i];
				char c2 = rootPath[i];
				// Handle both forward and backward slashes, and case-insensitivity on Windows
				if (std::tolower((unsigned char)c1) != std::tolower((unsigned char)c2) &&
					!((c1 == '\\' || c1 == '/') && (c2 == '\\' || c2 == '/'))) {
					match = false;
					break;
				}
			}
			if (match) {
				filepath.remove_prefix(rootPath.size());
			}
		}
#endif

		auto formattedMsg = std::format("{:02}/{:02}/{:04} {:02}:{:02}:{:02}.{:03} {} {} [{}:{}]",
			tm.tm_mon + 1, tm.tm_mday, tm.tm_year + 1900,
			tm.tm_hour, tm.tm_min, tm.tm_sec, ms,
			levelStr, msg, filepath, loc.line());

		g_logState.queue.pushBack(std::move(formattedMsg));
	}

	void printMessage(std::string_view name, std::string_view msg)
	{
		auto now = std::chrono::system_clock::now();
		auto timeT = std::chrono::system_clock::to_time_t(now);
		auto sec = std::chrono::floor<std::chrono::seconds>(now);
		auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now - sec).count();

		std::tm tm{};
#ifdef _WIN32
		localtime_s(&tm, &timeT);
#else
		localtime_r(&timeT, &tm);
#endif

		if (name.empty())
		{
			std::println("{:02}/{:02}/{:04} {:02}:{:02}:{:02}.{:03} {}",
				tm.tm_mon + 1, tm.tm_mday, tm.tm_year + 1900,
				tm.tm_hour, tm.tm_min, tm.tm_sec, ms,
				msg);
		}
		else
		{
			std::println("{:02}/{:02}/{:04} {:02}:{:02}:{:02}.{:03} [{}] {}",
				tm.tm_mon + 1, tm.tm_mday, tm.tm_year + 1900,
				tm.tm_hour, tm.tm_min, tm.tm_sec, ms,
				name, msg);
		}
	}
}
