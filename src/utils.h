#pragma once

#include <cstdint>

bool GetIsWindows11OrGreater();
bool IsRunningOnNVMe();
bool IsDirectStorageEnabled();
bool GetGameBuild(uint16_t& major, uint16_t& minor);