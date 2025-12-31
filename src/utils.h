#pragma once

#include <cstdint>
#include <array>

enum class DStorageStatus
{
	None,

	Enabled,
	Enabled_ForceDS,
	
	Disabled_ForceWin32,
	Disabled_NotOnWindows11,
	Disabled_NotOnNVMe,

	COUNT
};

constexpr const char* DStorageStatus_ToFriendlyString(DStorageStatus status)
{
	constexpr std::array DStorageStatusStrings = {
		"None",
		"Enabled",
		"-forceds",
		"-forcewin32",
		"Not on Windows 11",
		"Not on NVMe drive",
	};
	static_assert(DStorageStatusStrings.size() == static_cast<size_t>(DStorageStatus::COUNT));

	size_t index = static_cast<size_t>(status);
	if (index >= 0 && index < DStorageStatusStrings.size())
	{
		return DStorageStatusStrings[index];
	}

	return "Invalid";
}

int  GetWindowsNtBuild();
bool IsRunningOnNVMe();
bool IsDirectStorageEnabled(DStorageStatus* outStatus = nullptr);
bool GetGameBuild(uint16_t& major, uint16_t& minor);