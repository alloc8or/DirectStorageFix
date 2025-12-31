#include "log.h"
#include "utils.h"

#include <MinHook.h>
#include <mem/pattern.h>
#include <Psapi.h>

// Fix for vcpkg link issue when using clang-cl
#ifdef __clang__
#ifdef DEBUG
#pragma comment(lib, "fmtd.lib")
#pragma comment(lib, "minhook.x64d.lib")
#else // DEBUG
#pragma comment(lib, "fmt.lib")
#pragma comment(lib, "minhook.x64.lib")
#endif // DEBUG
#endif // __clang__

namespace rage
{
	struct fiFindData;
}

void(*OG_fiDeviceDirectStorage__RegisterFile)(void* _this, const char* fileName, const rage::fiFindData& findData);
void HK_fiDeviceDirectStorage__RegisterFile(void* _this, const char* fileName, const rage::fiFindData& findData)
{
	const char* ext = strrchr(fileName, '.');
	if (ext && (!strcmp(ext, ".rpf") || !strcmp(ext, ".cache")))
	{
		LOG_INFO("[fiDeviceDirectStorage::RegisterFile] {}", fileName);
		OG_fiDeviceDirectStorage__RegisterFile(_this, fileName, findData);
	}
}

static void Init()
{
	if (!Log::Get().Init())
		assert(false && "Log init failed");

	LOG_INFO("Init started");

	char modName[MAX_PATH];
	if (!GetModuleFileNameA(GetModuleHandleA(nullptr), modName, std::size(modName)))
	{
		LOG_ERROR("Failed to get module name (error {})", GetLastError());
		return;
	}

	const char* fileName = strrchr(modName, '\\');
	if (!fileName || strcmp(fileName + 1, "GTA5_Enhanced.exe"))
	{
		LOG_WARN("Module name is not GTA5_Enhanced.exe. Stopping.");
		return;
	}

	LOG_INFO("Windows NT build: {}", GetWindowsNtBuild());

	uint16_t major = 0, minor = 0;
	if (GetGameBuild(major, minor))
	{
		LOG_INFO("Game build: {}.{}", major, minor);

		if (major < 1013)
		{
			LOG_WARN("Game build < 1013. Stopping.");
			return;
		}
	}
	else
	{
		LOG_WARN("Unknown game build");
	}
	
	DStorageStatus status{};
	if (!IsDirectStorageEnabled(&status))
	{
		LOG_WARN("DirectStorage not enabled ({}). Stopping.", DStorageStatus_ToFriendlyString(status));
		return;
	}

	if (auto status = MH_Initialize(); status != MH_OK)
	{
		LOG_ERROR("Failed to initialize MinHook ({})", MH_StatusToString(status));
		return;
	}
	
	MODULEINFO info{};
	if (!GetModuleInformation(GetCurrentProcess(), GetModuleHandle(nullptr), &info, sizeof(info)))
	{
		LOG_ERROR("Failed to retrieve module info (error {})", GetLastError());
		return;
	}

	const mem::region region{info.lpBaseOfDll, info.SizeOfImage};
	LOG_INFO("Base: 0x{:X}, Size: 0x{:X}", region.start.as<uintptr_t>(), region.size);

	auto addr = mem::scan(mem::pattern("E8 ? ? ? ? 48 8B 06 48 89 F1 ? 89 ? 4D 89 F8 FF"), region);
	if (!addr)
	{
		LOG_ERROR("Failed to find fiDeviceDirectStorage::RegisterFile");
		return;
	}

	LOG_DEBUG("Found fiDeviceDirectStorage::RegisterFile call at 0x{:X}", addr.as<uintptr_t>());

	addr = addr.add(1).rip(4);
	if (auto status = MH_CreateHook(addr.as<void*>(),
		(void*)&HK_fiDeviceDirectStorage__RegisterFile,
		(void**)&OG_fiDeviceDirectStorage__RegisterFile);
		status != MH_OK)
	{
		LOG_ERROR("Failed to create hook at 0x{:X} ({})", addr.as<uintptr_t>(), MH_StatusToString(status));
		return;
	}

	if (auto status = MH_EnableHook(MH_ALL_HOOKS); status != MH_OK)
	{
		LOG_ERROR("Failed to enable hooks ({})", MH_StatusToString(status));
		return;
	}

	LOG_INFO("Init finished");
}

static void Shutdown()
{
	Log::Get().ForceFlush();
	spdlog::shutdown();
	MH_Uninitialize();
}

BOOL WINAPI DllMain(HMODULE hModule, DWORD dwReason, LPVOID lpReserved)
{
	switch (dwReason)
	{
	case DLL_PROCESS_ATTACH:
		Init();
		break;
	case DLL_PROCESS_DETACH:
		Shutdown();
		break;
	default:
		break;
	}

	return TRUE;
}
