// BthPS3Util.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <strsafe.h>
#include <bthsdpdef.h>
#include <bthdef.h>
#include <BlueToothApis.h>
#include <iostream>
#include <initguid.h>
#include "Public.h"
#include "argh.h"


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

    if (cmdl[{ "-i", "--install" }])
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

        std::cout << "Service installed successfully" << std::endl;

        return ERROR_SUCCESS;
    }

    if (cmdl[{ "-u", "--uninstall" }])
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

        std::cout << "Service uninstalled successfully" << std::endl;

        return ERROR_SUCCESS;
    }

    std::cout << "usage: .\\BthPS3Util -i|-u" << std::endl;

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
        std::cout << "LookupPrivilegeValue failed, err " <<  err << std::endl;
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

