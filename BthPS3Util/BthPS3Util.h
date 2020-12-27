#pragma once

//
// Windows
// 
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <shlwapi.h>
#include <SetupAPI.h>
#include <newdev.h>
#include <winioctl.h>
#include <rpc.h>

//
// OS Bluetooth APIs
// 
#include <bthsdpdef.h>
#include <bthdef.h>
#include <bluetoothapis.h>

//
// Device class interfaces
// 
#include <initguid.h>
#include <devguid.h>

//
// STL
// 
#include <iostream>
#include <algorithm>
#include <vector>
#include <string>

//
// Driver constants
// 
#include "BthPS3.h"

//
// CLI argument parser
// 
#include "argh.h"

//
// Registry manipulation wrapper
// 
#include "WinReg.hpp"

#include "colorwin.hpp"

#include "Devcon.h"

EXTERN_C IMAGE_DOS_HEADER __ImageBase;

namespace winapi
{
    BOOL AdjustProcessPrivileges();

    BOOL CreateDriverService(PCSTR ServiceName, PCSTR DisplayName, PCSTR BinaryPath);

    BOOL DeleteDriverService(PCSTR ServiceName);

    std::string GetLastErrorStdStr();

    std::string GetVersionFromFile(std::string FilePath);

    std::string GetImageBasePath();
};
