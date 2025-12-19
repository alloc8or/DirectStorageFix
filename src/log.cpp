#include "log.h"

#include <spdlog/sinks/basic_file_sink.h>
#include <Windows.h>
#include <Shlobj.h>
#include <filesystem>

using namespace std::chrono_literals;

#define LOG_FILE_NAME "DirectStorageFix.log"

bool Log::Init()
{
	if (m_Initialized)
		return true;

	std::error_code ec;
	std::filesystem::remove(LOG_FILE_NAME, ec);

	bool root = true;

	try { m_FileLogger = spdlog::basic_logger_mt("m_FileLogger", LOG_FILE_NAME); }
	catch (...) { root = false; }

	if (!root)
	{
		// Try AppData instead...
		std::filesystem::path logFilePath;
		PWSTR path = nullptr;
		if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_RoamingAppData, 0, nullptr, &path)))
		{
			logFilePath = path;
			logFilePath /= LOG_FILE_NAME;

			std::error_code ec;
			std::filesystem::remove(logFilePath, ec);

			CoTaskMemFree(path);
		}
		else
		{
			CoTaskMemFree(path);
			return false;
		}	

		try { m_FileLogger = spdlog::basic_logger_mt("m_FileLogger", logFilePath.string()); }
		catch (...) { return false; }
	}

	m_FileLogger->set_pattern("[%H:%M:%S.%e] [%l] %v");
	m_FileLogger->set_level(spdlog::level::trace);
	m_FileLogger->flush_on(spdlog::level::err);
	spdlog::flush_every(5s);

	m_Initialized = true;
	return true;
}

void Log::ForceFlush()
{
	if (!m_Initialized) return;
	m_FileLogger->flush();
}