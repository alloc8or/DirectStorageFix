#include "utils.h"
#include "log.h"

#include <Windows.h>
#include <winioctl.h>
#include <nvme.h>
#include <memory>
#include <fstream>
#include <filesystem>

bool GetIsWindows11OrGreater()
{
	HKEY hKey;
	LSTATUS status = RegOpenKeyExA(HKEY_LOCAL_MACHINE, R"(SOFTWARE\Microsoft\Windows NT\CurrentVersion)", 0, KEY_READ, &hKey);
	if (status != ERROR_SUCCESS)
	{
		LOG_DEBUG("RegOpenKeyExA failed (error {})", status);
		return false;
	}

	int winBuildNumber = 0;

	DWORD size = 0;
	status = RegQueryValueExA(hKey, "CurrentBuild", nullptr, nullptr, nullptr, &size);
	if ((status == ERROR_SUCCESS || status == ERROR_MORE_DATA) && size > 0)
	{
		auto buffer = std::make_unique_for_overwrite<char[]>(size);
		status = RegQueryValueExA(hKey, "CurrentBuild", nullptr, nullptr, (LPBYTE)buffer.get(), &size);
		if (status == ERROR_SUCCESS)
		{
			winBuildNumber = std::atoi(buffer.get());
			LOG_DEBUG("winBuildNumber: {}", winBuildNumber);
		}
		else
		{
			LOG_DEBUG("RegQueryValueExA failed (error {})", status);
		}
	}
	else
	{
		LOG_DEBUG("RegOpenKeyExA failed (error {})", status);
	}

	RegCloseKey(hKey);

	return winBuildNumber >= 22000;
}

bool IsRunningOnNVMe()
{
	char modName[MAX_PATH];
	if (!GetModuleFileNameA(GetModuleHandleA(nullptr), modName, std::size(modName)))
	{
		LOG_DEBUG("GetModuleFileNameA failed (error {})", GetLastError());
		return false;
	}

	char volumePathName[MAX_PATH];
	if (!GetVolumePathNameA(modName, volumePathName, std::size(volumePathName)))
	{
		LOG_DEBUG("GetVolumePathNameA({}) failed (error {})", modName, GetLastError());
		return false;
	}

	char volumeName[50];
	if (!GetVolumeNameForVolumeMountPointA(volumePathName, volumeName, std::size(volumeName)))
	{
		LOG_DEBUG("GetVolumeNameForVolumeMountPointA({}) failed (error {})", volumePathName, GetLastError());
		return false;
	}

	size_t volumeNameLen = strlen(volumeName);
	if (volumeNameLen && volumeName[volumeNameLen - 1] == '\\')
		volumeName[volumeNameLen - 1] = '\0';

	HANDLE hDevice = CreateFileA(volumeName, 0, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr);
	if (hDevice == INVALID_HANDLE_VALUE)
	{
		LOG_DEBUG("CreateFileA({}) failed (error {})", volumeName, GetLastError());
		return false;
	}

	constexpr size_t bufferSize = FIELD_OFFSET(STORAGE_PROPERTY_QUERY, AdditionalParameters)
								+ sizeof(STORAGE_PROTOCOL_SPECIFIC_DATA)
								+ NVME_MAX_LOG_SIZE;
	auto buffer = std::make_unique_for_overwrite<uint8_t[]>(bufferSize);

	PSTORAGE_PROPERTY_QUERY query = (PSTORAGE_PROPERTY_QUERY)buffer.get();
	query->PropertyId = StorageAdapterProtocolSpecificProperty;
	query->QueryType = PropertyStandardQuery;

	PSTORAGE_PROTOCOL_SPECIFIC_DATA protocolData = (PSTORAGE_PROTOCOL_SPECIFIC_DATA)&query->AdditionalParameters;
	protocolData->ProtocolType = ProtocolTypeNvme;
	protocolData->DataType = NVMeDataTypeIdentify;
	protocolData->ProtocolDataRequestValue = NVME_IDENTIFY_CNS_CONTROLLER;
	protocolData->ProtocolDataRequestSubValue = 0;
	protocolData->ProtocolDataOffset = sizeof(STORAGE_PROTOCOL_SPECIFIC_DATA);
	protocolData->ProtocolDataLength = NVME_MAX_LOG_SIZE;	

	if (DeviceIoControl(hDevice, IOCTL_STORAGE_QUERY_PROPERTY, buffer.get(), bufferSize, buffer.get(), bufferSize, nullptr, nullptr))
	{
		CloseHandle(hDevice);

		PSTORAGE_PROTOCOL_DATA_DESCRIPTOR dataDesc = (PSTORAGE_PROTOCOL_DATA_DESCRIPTOR)buffer.get();

		if (dataDesc->Version != sizeof(STORAGE_PROTOCOL_DATA_DESCRIPTOR) ||
			dataDesc->Size != sizeof(STORAGE_PROTOCOL_DATA_DESCRIPTOR))
		{
			LOG_DEBUG("Version/Size mismatch (expected {}, got {}/{})",
				sizeof(STORAGE_PROTOCOL_DATA_DESCRIPTOR), dataDesc->Version, dataDesc->Size);
			return false;
		}

		if (dataDesc->ProtocolSpecificData.ProtocolDataOffset < sizeof(STORAGE_PROTOCOL_SPECIFIC_DATA) ||
			dataDesc->ProtocolSpecificData.ProtocolDataLength < NVME_MAX_LOG_SIZE)
		{
			LOG_DEBUG("Offset/Length mismatch (expected <{}/{}, got {}/{})",
				sizeof(STORAGE_PROTOCOL_SPECIFIC_DATA), NVME_MAX_LOG_SIZE,
				dataDesc->ProtocolSpecificData.ProtocolDataOffset, dataDesc->ProtocolSpecificData.ProtocolDataLength);
			return false;
		}

		PNVME_IDENTIFY_CONTROLLER_DATA identifyControllerData = (PNVME_IDENTIFY_CONTROLLER_DATA)(
			(PCHAR)&dataDesc->ProtocolSpecificData + dataDesc->ProtocolSpecificData.ProtocolDataOffset);

		LOG_DEBUG("VID {} NN {}", identifyControllerData->VID, identifyControllerData->NN);

		return identifyControllerData->VID != 0 && identifyControllerData->NN != 0;
	}
	else
	{
		LOG_DEBUG("DeviceIoControl failed (error {})", GetLastError());
	}

	CloseHandle(hDevice);
	return false;
}

// NOTE: This does not implement the BitLocker check that the game has.
// I don't think anyone has BitLocker enabled on a game drive anyway, as that would be kinda retarded.
bool IsDirectStorageEnabled()
{
	bool forceDS = false, forceWin32 = false;

	const char* cmdLine = GetCommandLineA();
	if (cmdLine)
	{
		forceDS    = strstr(cmdLine, "-forceds")    != nullptr;
		forceWin32 = strstr(cmdLine, "-forcewin32") != nullptr;
	}

	std::error_code ec;
	if (std::filesystem::exists("commandline.txt", ec))
	{
		std::ifstream file("commandline.txt");
		std::string line;
		while (std::getline(file, line))
		{
			if (line == "-forceds")
				forceDS |= true;
			else if (line == "-forcewin32")
				forceWin32 |= true;
		}
	}

	if (forceWin32)
	{
		LOG_DEBUG("Returning false due to -forcewin32");
		return false;
	}

	if (forceDS)
	{
		LOG_DEBUG("Returning true due to -forceds");
		return true;
	}

	if (!GetIsWindows11OrGreater())
	{
		LOG_DEBUG("Returning false due to not running on Windows 11");
		return false;
	}

	if (!IsRunningOnNVMe())
	{
		LOG_DEBUG("Returning false due to not running on an NVMe drive");
		return false;
	}

	return true;
}