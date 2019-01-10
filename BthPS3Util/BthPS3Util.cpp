// BthPS3Util.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

//
// Windows
// 
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <SetupAPI.h>
#include <strsafe.h>

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
#include "Public.h"

//
// CLI argument parser
// 
#include "argh.h"

//
// Registry manipulation wrapper
// 
#include "WinReg.hpp"

using namespace winreg;


#define LOWER_FILTERS L"LowerFilters"

BOOL
AdjustProcessPrivileges(
);

int main(int, char* argv[])
{
    argh::parser cmdl(argv);

    if (FALSE == AdjustProcessPrivileges())
    {
        return ERROR_ACCESS_DENIED;
    }

    DWORD err = ERROR_SUCCESS;
    BLUETOOTH_LOCAL_SERVICE_INFO SvcInfo = { 0 };

    if (FAILED(StringCbCopyW(SvcInfo.szName, sizeof(SvcInfo.szName), BthPS3ServiceName)))
    {
        std::cout << "Couldn't set service name" << std::endl;
        return GetLastError();
    }

    if (cmdl[{ "--enable-service" }])
    {
        SvcInfo.Enabled = TRUE;

        if (ERROR_SUCCESS != (err = BluetoothSetLocalServiceInfo(
            nullptr, //callee would select the first found radio
            &BTHPS3_SERVICE_GUID,
            0,
            &SvcInfo
        )))
        {
            std::cout << "BluetoothSetLocalServiceInfo failed, err " << err << std::endl;
            return GetLastError();
        }

        std::cout << "Service enabled successfully" << std::endl;

        return ERROR_SUCCESS;
    }

    if (cmdl[{ "--disable-service" }])
    {
        SvcInfo.Enabled = FALSE;

        if (ERROR_SUCCESS != (err = BluetoothSetLocalServiceInfo(
            nullptr, //callee would select the first found radio
            &BTHPS3_SERVICE_GUID,
            0,
            &SvcInfo
        )))
        {
            std::cout << "BluetoothSetLocalServiceInfo failed, err " << err << std::endl;
            return GetLastError();
        }

        std::cout << "Service disabled successfully" << std::endl;

        return ERROR_SUCCESS;
    }

    if (cmdl[{ "--enable-filter" }])
    {
        auto key = SetupDiOpenClassRegKey(&GUID_DEVCLASS_BLUETOOTH, KEY_ALL_ACCESS);

        if (INVALID_HANDLE_VALUE == key)
        {
            std::cout << "Couldn't open Class key, error " << std::hex << GetLastError() << std::endl;
            return ERROR_ACCESS_DENIED;
        }

        // wrap it up
        RegKey classKey(key);

        //
        // Check if "LowerFilters" value exists
        // 
        auto values = classKey.EnumValues();
        auto ret = std::find_if(values.begin(), values.end(), [&](std::pair<std::wstring, DWORD> const & ref) {
            return ref.first == LOWER_FILTERS;
        });

        //
        // Value doesn't exist, create with filter driver name
        // 
        if (ret == values.end())
        {
            classKey.SetMultiStringValue(LOWER_FILTERS, { BthPS3FilterName });
            classKey.Close();
            return ERROR_SUCCESS;
        }

        auto lowerFilters = classKey.GetMultiStringValue(LOWER_FILTERS);

        if (std::find(lowerFilters.begin(), lowerFilters.end(), BthPS3FilterName) == lowerFilters.end())
        {
            lowerFilters.emplace_back(BthPS3FilterName);
            classKey.SetMultiStringValue(LOWER_FILTERS, lowerFilters);
            classKey.Close();
            return ERROR_SUCCESS;
        }
    }

    if (cmdl[{ "--disable-filter" }])
    {
    }

    return ERROR_INVALID_PARAMETER;
}

BOOL
AdjustProcessPrivileges(
)
{
    HANDLE procToken;
    LUID luid;
    TOKEN_PRIVILEGES tp;
    BOOL bRetVal;
    DWORD err;

    bRetVal = OpenProcessToken(
        GetCurrentProcess(),
        TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY,
        &procToken
    );

    if (!bRetVal)
    {
        err = GetLastError();
        std::cout << "OpenProcessToken failed, err = " << err << std::endl;
        goto exit;
    }

    bRetVal = LookupPrivilegeValue(nullptr, SE_LOAD_DRIVER_NAME, &luid);
    if (!bRetVal)
    {
        err = GetLastError();
        std::cout << "LookupPrivilegeValue failed, err " << err << std::endl;
        goto exit1;
    }

    tp.PrivilegeCount = 1;
    tp.Privileges[0].Luid = luid;
    tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;

    //
    // AdjustTokenPrivileges can succeed even when privileges are not adjusted.
    // In such case GetLastError returns ERROR_NOT_ALL_ASSIGNED.
    //
    // Hence we check for GetLastError in both success and failure case.
    //

    (void)AdjustTokenPrivileges(
        procToken,
        FALSE,
        &tp,
        sizeof(TOKEN_PRIVILEGES),
        (PTOKEN_PRIVILEGES)nullptr,
        (PDWORD)nullptr
    );
    err = GetLastError();

    if (err != ERROR_SUCCESS)
    {
        bRetVal = FALSE;
        std::cout << "AdjustTokenPrivileges failed, err " << err << std::endl;
        goto exit1;
    }

exit1:
    CloseHandle(procToken);
exit:
    return bRetVal;
}

