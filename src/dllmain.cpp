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
		assert(false);

	char modName[MAX_PATH];
	if (!GetModuleFileNameA(GetModuleHandleA(nullptr), modName, std::size(modName)))
	{
		LOG_ERROR("Failed to get module name");
		return;
	}

	const char* fileName = strrchr(modName, '\\');
	if (!fileName || strcmp(fileName + 1, "GTA5_Enhanced.exe"))
	{
		LOG_WARN("Module name is not GTA5_Enhanced.exe. Stopping.");
		return;
	}
	
	if (!IsDirectStorageEnabled())
	{
		LOG_WARN("DirectStorage not enabled. Stopping.");
		return;
	}

	if (MH_OK != MH_Initialize())
	{
		LOG_ERROR("Failed to initialize MinHook");
		return;
	}
	
	MODULEINFO info{};
	if (!GetModuleInformation(GetCurrentProcess(), GetModuleHandle(nullptr), &info, sizeof(info)))
	{
		LOG_ERROR("Failed to retrieve module info");
		return;
	}

	const mem::region region{info.lpBaseOfDll, info.SizeOfImage};
	auto addr = mem::scan(mem::pattern("E8 ? ? ? ? 48 8B 06 48 89 F1 ? 89 ? 4D 89 F8 FF"), region);
	if (!addr)
	{
		LOG_ERROR("Failed to find fiDeviceDirectStorage::RegisterFile");
		return;
	}

	addr += addr.at<int>(1) + 5;
	if (MH_OK != MH_CreateHook(addr.as<void*>(), (void*)&HK_fiDeviceDirectStorage__RegisterFile, (void**)&OG_fiDeviceDirectStorage__RegisterFile))
	{
		LOG_ERROR("Failed to create hook");
		return;
	}

	if (MH_OK != MH_EnableHook(MH_ALL_HOOKS))
	{
		LOG_ERROR("Failed to enable hooks");
		return;
	}
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
