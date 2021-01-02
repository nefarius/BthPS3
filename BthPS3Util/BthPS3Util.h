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
// Add some colors to console
// 
#include "colorwin.hpp"

//
// Setup helpers
// 
#include <Devcon.h>
#include <BthPS3Setup.h>
#include <UniUtil.h>
