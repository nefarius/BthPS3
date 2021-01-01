#include "Devcon.h"

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <SetupAPI.h>
#include <tchar.h>
#include <devguid.h>

bool devcon::create(std::wstring className, const GUID* classGuid, std::wstring hardwareId)
{
    auto deviceInfoSet = SetupDiCreateDeviceInfoList(classGuid, nullptr);

    if (INVALID_HANDLE_VALUE == deviceInfoSet)
        return false;

    SP_DEVINFO_DATA deviceInfoData;
    deviceInfoData.cbSize = sizeof(deviceInfoData);

    auto cdiRet = SetupDiCreateDeviceInfoW(
        deviceInfoSet,
        className.c_str(),
        classGuid,
        nullptr,
        nullptr,
        DICD_GENERATE_ID,
        &deviceInfoData
    );

    if (!cdiRet)
    {
        SetupDiDestroyDeviceInfoList(deviceInfoSet);
        return false;
    }

    auto sdrpRet = SetupDiSetDeviceRegistryPropertyW(
        deviceInfoSet,
        &deviceInfoData,
        SPDRP_HARDWAREID,
        (const PBYTE)hardwareId.c_str(),
        (DWORD)(hardwareId.size() * sizeof(WCHAR))
    );

    if (!sdrpRet)
    {
        SetupDiDestroyDeviceInfoList(deviceInfoSet);
        return false;
    }

    auto cciRet = SetupDiCallClassInstaller(
        DIF_REGISTERDEVICE,
        deviceInfoSet,
        &deviceInfoData
    );

    if (!cciRet)
    {
        SetupDiDestroyDeviceInfoList(deviceInfoSet);
        return false;
    }

    SetupDiDestroyDeviceInfoList(deviceInfoSet);

    return true;
}

bool devcon::create(std::string className, const GUID* classGuid, std::string hardwareId)
{
    auto deviceInfoSet = SetupDiCreateDeviceInfoList(classGuid, nullptr);

    if (INVALID_HANDLE_VALUE == deviceInfoSet)
        return false;

    SP_DEVINFO_DATA deviceInfoData;
    deviceInfoData.cbSize = sizeof(deviceInfoData);

    auto cdiRet = SetupDiCreateDeviceInfoA(
        deviceInfoSet,
        className.c_str(),
        classGuid,
        nullptr,
        nullptr,
        DICD_GENERATE_ID,
        &deviceInfoData
    );

    if (!cdiRet)
    {
        SetupDiDestroyDeviceInfoList(deviceInfoSet);
        return false;
    }

    auto sdrpRet = SetupDiSetDeviceRegistryPropertyA(
        deviceInfoSet,
        &deviceInfoData,
        SPDRP_HARDWAREID,
        (const PBYTE)hardwareId.c_str(),
        (DWORD)(hardwareId.size() * sizeof(WCHAR))
    );

    if (!sdrpRet)
    {
        SetupDiDestroyDeviceInfoList(deviceInfoSet);
        return false;
    }

    auto cciRet = SetupDiCallClassInstaller(
        DIF_REGISTERDEVICE,
        deviceInfoSet,
        &deviceInfoData
    );

    if (!cciRet)
    {
        SetupDiDestroyDeviceInfoList(deviceInfoSet);
        return false;
    }

    SetupDiDestroyDeviceInfoList(deviceInfoSet);

    return true;
}

bool devcon::restart_bth_usb_device()
{
    DWORD i, err;
    bool found = false, succeeded = false;

    HDEVINFO hDevInfo;
    SP_DEVINFO_DATA spDevInfoData;

    hDevInfo = SetupDiGetClassDevs(
        &GUID_DEVCLASS_BLUETOOTH,
        nullptr, 
        nullptr, 
        DIGCF_PRESENT
    );
    if (hDevInfo == INVALID_HANDLE_VALUE)
    {
        return succeeded;
    }

    spDevInfoData.cbSize = sizeof(SP_DEVINFO_DATA);
    for (i = 0; SetupDiEnumDeviceInfo(hDevInfo, i, &spDevInfoData); i++)
    {
        DWORD DataT;
        LPTSTR p, buffer = nullptr;
        DWORD buffersize = 0;

        // get all devices info
        while (!SetupDiGetDeviceRegistryProperty(hDevInfo,
            &spDevInfoData,
            SPDRP_ENUMERATOR_NAME,
            &DataT,
            (PBYTE)buffer,
            buffersize,
            &buffersize))
        {
            if (GetLastError() == ERROR_INVALID_DATA)
            {
                break;
            }
            if (GetLastError() == ERROR_INSUFFICIENT_BUFFER)
            {
                if (buffer)
                    LocalFree(buffer);
                buffer = (wchar_t*)LocalAlloc(LPTR, buffersize);
            }
            else
            {
                goto cleanup_DeviceInfo;
            }
        }

        if (GetLastError() == ERROR_INVALID_DATA)
            continue;

        //find device with enumerator name "USB"
        for (p = buffer; *p && (p < &buffer[buffersize]); p += lstrlen(p) + sizeof(TCHAR))
        {
            if (!_tcscmp(TEXT("USB"), p))
            {
                found = true;
                break;
            }
        }

        if (buffer)
            LocalFree(buffer);

        // if device found restart
        if (found)
        {
            if(!SetupDiRestartDevices(hDevInfo, &spDevInfoData))
            {
	            err = GetLastError();
            	break;
            }

            succeeded = true;
        	
        	break;
        }
    }

cleanup_DeviceInfo:
    err = GetLastError();
    SetupDiDestroyDeviceInfoList(hDevInfo);
    SetLastError(err);

    return succeeded;
}
