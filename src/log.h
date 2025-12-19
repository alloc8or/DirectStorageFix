#pragma once

#include <spdlog/spdlog.h>
#include <format>

#define LOG_INFO(fmt, ...)	Log::Get().Write(spdlog::level::info,  fmt, ##__VA_ARGS__)
#define LOG_ERROR(fmt, ...) Log::Get().Write(spdlog::level::err,   fmt, ##__VA_ARGS__)
#define LOG_WARN(fmt, ...)  Log::Get().Write(spdlog::level::warn,  fmt, ##__VA_ARGS__)

#ifdef DEBUG
#define LOG_DEBUG(fmt, ...)	Log::Get().Write(__FUNCTION__, spdlog::level::debug, fmt, ##__VA_ARGS__)
#else
#define LOG_DEBUG(fmt, ...)
#endif

class Log
{
public:
	Log(const Log&) = delete;
	void operator=(const Log&) = delete;

	static Log& Get()
	{
		static Log s_Instance;
		return s_Instance;
	}

	void Write(spdlog::level::level_enum logLevel, std::string_view msg)
	{
		if (!m_Initialized) return;
		m_FileLogger->log(logLevel, "{}", msg);
	}

	void Write(const char* functionName, spdlog::level::level_enum logLevel, std::string_view msg)
	{
		if (!m_Initialized) return;
		m_FileLogger->log(logLevel, "[{}] {}", functionName, msg);
	}

	template<typename... Args>
	void Write(spdlog::level::level_enum logLevel, spdlog::format_string_t<Args...> fmt, Args&&... args)
	{
		if (!m_Initialized) return;
		m_FileLogger->log(logLevel, fmt, std::forward<Args>(args)...);
	}

	template<typename... Args>
	void Write(const char* functionName, spdlog::level::level_enum logLevel, std::string_view fmt, Args&&... args)
	{
		if (!m_Initialized) return;
		m_FileLogger->log(logLevel, "[{}] {}", functionName, std::vformat(fmt, std::make_format_args(args...)));
	}

	bool Init();
	void ForceFlush();

private:
	Log() = default;

	bool m_Initialized = false;
	std::shared_ptr<spdlog::logger> m_FileLogger;
};