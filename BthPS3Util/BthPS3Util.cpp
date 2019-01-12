// BthPS3Util.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include "BthPS3Util.h"
using namespace winreg;


#define LOWER_FILTERS L"LowerFilters"

BOOL
AdjustProcessPrivileges(
);

int main(int, char* argv[])
{
    argh::parser cmdl(argv);

    DWORD err = ERROR_SUCCESS;
    BLUETOOTH_LOCAL_SERVICE_INFO SvcInfo = { 0 };
    wcscpy_s(SvcInfo.szName, sizeof(SvcInfo.szName) / sizeof(WCHAR), BthPS3ServiceName);

    if (cmdl[{ "--enable-service" }])
    {
        if (FALSE == AdjustProcessPrivileges())
        {
            std::cout << "Failed to gain required privileges, error " << std::hex << GetLastError() << std::endl;
            return GetLastError();
        }

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
        if (FALSE == AdjustProcessPrivileges())
        {
            std::cout << "Failed to gain required privileges, error " << std::hex << GetLastError() << std::endl;
            return GetLastError();
        }

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

    if (cmdl[{ "--create-filter-device" }])
    {
        auto ret = devcon::create(
            L"System",
            &GUID_DEVCLASS_SYSTEM,
            L"Nefarius\\{a3dc6d41-9e10-46d9-8be2-9b4a279841df}\0\0"
        );

        if (!ret)
        {
            std::cout << "Failed to create device, error " << std::hex << GetLastError() << std::endl;
            return GetLastError();
        }

        return ERROR_SUCCESS;
    }

    if (cmdl[{ "--enable-filter" }])
    {
        auto key = SetupDiOpenClassRegKey(&GUID_DEVCLASS_BLUETOOTH, KEY_ALL_ACCESS);

        if (INVALID_HANDLE_VALUE == key)
        {
            std::cout << "Couldn't open Class key, error " << std::hex << GetLastError() << std::endl;
            return GetLastError();
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

        //
        // Patch in our filter
        // 
        auto lowerFilters = classKey.GetMultiStringValue(LOWER_FILTERS);
        if (std::find(lowerFilters.begin(), lowerFilters.end(), BthPS3FilterName) == lowerFilters.end())
        {
            lowerFilters.emplace_back(BthPS3FilterName);
            classKey.SetMultiStringValue(LOWER_FILTERS, lowerFilters);
            classKey.Close();
            return ERROR_SUCCESS;
        }

        return ERROR_SUCCESS;
    }

    if (cmdl[{ "--disable-filter" }])
    {
        auto key = SetupDiOpenClassRegKey(&GUID_DEVCLASS_BLUETOOTH, KEY_ALL_ACCESS);

        if (INVALID_HANDLE_VALUE == key)
        {
            std::cout << "Couldn't open Class key, error " << std::hex << GetLastError() << std::endl;
            return GetLastError();
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
        // Value exists, patch out our name
        // 
        if (ret != values.end())
        {
            auto lowerFilters = classKey.GetMultiStringValue(LOWER_FILTERS);
            lowerFilters.erase(
                std::remove(
                    lowerFilters.begin(),
                    lowerFilters.end(),
                    BthPS3FilterName),
                lowerFilters.end());
            classKey.SetMultiStringValue(LOWER_FILTERS, lowerFilters);
            classKey.Close();
            return ERROR_SUCCESS;
        }

        classKey.Close();
        return ERROR_SUCCESS;
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

