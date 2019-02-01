#pragma once

//
// Windows
// 
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <SetupAPI.h>
#include <newdev.h>

//
// OS Bluetooth APIs
// 
#include <bthsdpdef.h>
#include <bthdef.h>
#include <BlueToothApis.h>

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

namespace winapi
{
    BOOL AdjustProcessPrivileges();
};
