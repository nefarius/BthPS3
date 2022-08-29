#include "Devcon.h"

//
// WinAPI
// 
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <SetupAPI.h>
#include <tchar.h>
#include <devguid.h>
#include <newdev.h>
#include <Shlwapi.h>

//
// STL
// 
#include <strsafe.h>
#include <vector>

#include "LibraryHelper.hpp"

//
// Hooking
// 
#include <detours/detours.h>

static decltype(MessageBoxW)* real_MessageBoxW = MessageBoxW;

int DetourMessageBoxW(
	HWND    hWnd,
	LPCWSTR lpText,
	LPCWSTR lpCaption,
	UINT    uType
);

static BOOL g_MbCalled = FALSE;


// Helper function to build a multi-string from a vector<wstring>
inline std::vector<wchar_t> BuildMultiString(const std::vector<std::wstring>& data)
{
    // Special case of the empty multi-string
    if (data.empty())
    {
        // Build a vector containing just two NULs
        return std::vector<wchar_t>(2, L'\0');
    }

    // Get the total length in wchar_ts of the multi-string
    size_t totalLen = 0;
    for (const auto& s : data)
    {
        // Add one to current string's length for the terminating NUL
        totalLen += (s.length() + 1);
    }

    // Add one for the last NUL terminator (making the whole structure double-NUL terminated)
    totalLen++;

    // Allocate a buffer to store the multi-string
    std::vector<wchar_t> multiString;
    multiString.reserve(totalLen);

    // Copy the single strings into the multi-string
    for (const auto& s : data)
    {      
        multiString.insert(multiString.end(), s.begin(), s.end());
        
        // Don't forget to NUL-terminate the current string
        multiString.push_back(L'\0');
    }

    // Add the last NUL-terminator
    multiString.push_back(L'\0');

    return multiString;
}

auto devcon::create(const std::wstring& className, const GUID* classGuid, const std::wstring& hardwareId) -> bool
{
	const auto deviceInfoSet = SetupDiCreateDeviceInfoList(classGuid, nullptr);

    if (INVALID_HANDLE_VALUE == deviceInfoSet)
        return false;

    SP_DEVINFO_DATA deviceInfoData;
    deviceInfoData.cbSize = sizeof(deviceInfoData);

	const auto cdiRet = SetupDiCreateDeviceInfoW(
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

	const auto sdrpRet = SetupDiSetDeviceRegistryPropertyW(
        deviceInfoSet,
        &deviceInfoData,
        SPDRP_HARDWAREID,
        (const PBYTE)hardwareId.c_str(),
        static_cast<DWORD>(hardwareId.size() * sizeof(WCHAR))
    );

    if (!sdrpRet)
    {
        SetupDiDestroyDeviceInfoList(deviceInfoSet);
        return false;
    }

	const auto cciRet = SetupDiCallClassInstaller(
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

bool devcon::enable_disable_bth_usb_device(bool state)
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

        // if device found change it's state
        if (found)
        {
            SP_PROPCHANGE_PARAMS params;

            params.ClassInstallHeader.cbSize = sizeof(SP_CLASSINSTALL_HEADER);
            params.ClassInstallHeader.InstallFunction = DIF_PROPERTYCHANGE;
            params.Scope = DICS_FLAG_GLOBAL;
            params.StateChange = (state) ? DICS_ENABLE : DICS_DISABLE;

            // setup proper parameters            
            if (!SetupDiSetClassInstallParams(hDevInfo, &spDevInfoData, &params.ClassInstallHeader, sizeof(params)))
            {
                err = GetLastError();
            }
            else
            {
                // use parameters
                if (!SetupDiCallClassInstaller(DIF_PROPERTYCHANGE, hDevInfo, &spDevInfoData))
                {
                    err = GetLastError(); // error here  
                }
                else { succeeded = true; }
            }

            break;
        }
    }

cleanup_DeviceInfo:
    err = GetLastError();
    SetupDiDestroyDeviceInfoList(hDevInfo);
    SetLastError(err);

    return succeeded;
}

bool devcon::install_driver(const std::wstring& fullInfPath, bool* rebootRequired)
{
	Newdev newdev;
	BOOL reboot;

	if (!newdev.pDiInstallDriverW)
	{
		SetLastError(ERROR_INVALID_FUNCTION);
		return false;
	}
	
	const auto ret = newdev.pDiInstallDriverW(
		nullptr,
		fullInfPath.c_str(),
		DIIRFLAG_FORCE_INF,
		&reboot
	);

	if (rebootRequired)
		*rebootRequired = reboot > 1;

	return ret > 0;
}

bool devcon::add_device_class_lower_filter(const GUID* classGuid, const std::wstring& filterName)
{
	auto key = SetupDiOpenClassRegKey(classGuid, KEY_ALL_ACCESS);

	if (INVALID_HANDLE_VALUE == key)
	{
		return false;
	}

	LPCWSTR lowerFilterValue = L"LowerFilters";
    DWORD type, size;
	std::vector<std::wstring> filters;
	
	auto status = RegQueryValueExW(
		key,
		lowerFilterValue,
		nullptr,
		&type,
		nullptr,
		&size
	);

	//
	// Value exists already, read it with returned buffer size
	// 
	if (status == ERROR_SUCCESS)
	{
		std::vector<wchar_t> temp(size / sizeof(wchar_t));

		status = RegQueryValueExW(
			key,
			lowerFilterValue,
			nullptr,
			&type,
			reinterpret_cast<LPBYTE>(&temp[0]),
			&size
		);

		if (status != ERROR_SUCCESS)
		{
			RegCloseKey(key);
			SetLastError(status);
			return false;
		}

		size_t index = 0;
		size_t len = wcslen(&temp[0]);
		while (len > 0)
		{
			filters.emplace_back(&temp[index]);
			index += len + 1;
			len = wcslen(&temp[index]);
		}

		//
		// Filter not there yet, add
		// 
		if (std::find(filters.begin(), filters.end(), filterName) == filters.end())
		{
			filters.emplace_back(filterName);
		}

		const std::vector<wchar_t> multiString = BuildMultiString(filters);

		const DWORD dataSize = static_cast<DWORD>(multiString.size() * sizeof(wchar_t));

		status = RegSetValueExW(
			key,
			lowerFilterValue,
			0, // reserved
			REG_MULTI_SZ,
			reinterpret_cast<const BYTE*>(&multiString[0]),
			dataSize
		);

		if (status != ERROR_SUCCESS)
		{
			RegCloseKey(key);
			SetLastError(status);
			return false;
		}

		RegCloseKey(key);
		return true;
	}
	//
	// Value doesn't exist, create and populate
	// 
	else if (status == ERROR_FILE_NOT_FOUND)
	{
		filters.emplace_back(filterName);

		const std::vector<wchar_t> multiString = BuildMultiString(filters);

		const DWORD dataSize = static_cast<DWORD>(multiString.size() * sizeof(wchar_t));

		status = RegSetValueExW(
			key,
			lowerFilterValue,
			0, // reserved
			REG_MULTI_SZ,
			reinterpret_cast<const BYTE*>(&multiString[0]),
			dataSize
		);

		if (status != ERROR_SUCCESS)
		{
			RegCloseKey(key);
			SetLastError(status);
			return false;
		}

		RegCloseKey(key);
		return true;
	}

	RegCloseKey(key);
	return false;
}

bool devcon::remove_device_class_lower_filter(const GUID* classGuid, const std::wstring& filterName)
{
	auto key = SetupDiOpenClassRegKey(classGuid, KEY_ALL_ACCESS);

	if (INVALID_HANDLE_VALUE == key)
	{
		return false;
	}

	LPCWSTR lowerFilterValue = L"LowerFilters";
	DWORD type, size;
	std::vector<std::wstring> filters;

	auto status = RegQueryValueExW(
		key,
		lowerFilterValue,
		nullptr,
		&type,
		nullptr,
		&size
	);

	//
	// Value exists already, read it with returned buffer size
	// 
	if (status == ERROR_SUCCESS)
	{
		std::vector<wchar_t> temp(size / sizeof(wchar_t));

		status = RegQueryValueExW(
			key,
			lowerFilterValue,
			nullptr,
			&type,
			reinterpret_cast<LPBYTE>(&temp[0]),
			&size
		);

		if (status != ERROR_SUCCESS)
		{
			RegCloseKey(key);
			SetLastError(status);
			return false;
		}

		//
		// Remove value, if found
		//
		size_t index = 0;
		size_t len = wcslen(&temp[0]);
		while (len > 0)
		{
			if (filterName != &temp[index])
			{
				filters.emplace_back(&temp[index]);
			}
			index += len + 1;
			len = wcslen(&temp[index]);
		}

		const std::vector<wchar_t> multiString = BuildMultiString(filters);

		const DWORD dataSize = static_cast<DWORD>(multiString.size() * sizeof(wchar_t));

		status = RegSetValueExW(
			key,
			lowerFilterValue,
			0, // reserved
			REG_MULTI_SZ,
			reinterpret_cast<const BYTE*>(&multiString[0]),
			dataSize
		);

		if (status != ERROR_SUCCESS)
		{
			RegCloseKey(key);
			SetLastError(status);
			return false;
		}

		RegCloseKey(key);
		return true;
	}
	//
	// Value doesn't exist, return
	// 
	if (status == ERROR_FILE_NOT_FOUND)
	{
		RegCloseKey(key);
		return true;
	}

	RegCloseKey(key);
	return false;
}

inline bool uninstall_device_and_driver(HDEVINFO hDevInfo, PSP_DEVINFO_DATA spDevInfoData, bool* rebootRequired)
{
	BOOL drvNeedsReboot = FALSE, devNeedsReboot = FALSE;
	DWORD requiredBufferSize = 0;
	DWORD err = ERROR_SUCCESS;
	bool ret = false;

	Newdev newdev;

	if (!newdev.pDiUninstallDevice || !newdev.pDiUninstallDriverW)
	{
		SetLastError(ERROR_INVALID_FUNCTION);
		return false;
	}
	
	SP_DRVINFO_DATA_W drvInfoData;
	drvInfoData.cbSize = sizeof(drvInfoData);

	PSP_DRVINFO_DETAIL_DATA_W pDrvInfoDetailData = nullptr;

	do
	{
		//
		// Start building driver info
		// 
		if (!SetupDiBuildDriverInfoList(
			hDevInfo,
			spDevInfoData,
			SPDIT_COMPATDRIVER
		))
		{
			err = GetLastError();
			break;
		}

		if (!SetupDiEnumDriverInfo(
			hDevInfo,
			spDevInfoData,
			SPDIT_COMPATDRIVER,
			0, // One result expected
			&drvInfoData
		))
		{
			err = GetLastError();
			break;
		}

		//
		// Details will contain the INF path to driver store copy
		// 
		SP_DRVINFO_DETAIL_DATA_W drvInfoDetailData;
		drvInfoDetailData.cbSize = sizeof(drvInfoDetailData);

		//
		// Request required buffer size
		// 
		(void)SetupDiGetDriverInfoDetail(
			hDevInfo,
			spDevInfoData,
			&drvInfoData,
			&drvInfoDetailData,
			drvInfoDetailData.cbSize,
			&requiredBufferSize
		);

		if (requiredBufferSize == 0)
		{
			err = GetLastError();
			break;
		}

		//
		// Allocate required amount
		// 
		pDrvInfoDetailData = static_cast<PSP_DRVINFO_DETAIL_DATA_W>(malloc(requiredBufferSize));

		if (pDrvInfoDetailData == nullptr)
		{
			err = ERROR_INSUFFICIENT_BUFFER;
			break;
		}

		pDrvInfoDetailData->cbSize = sizeof(SP_DRVINFO_DETAIL_DATA_W);

		//
		// Query full driver details
		// 
		if (!SetupDiGetDriverInfoDetail(
			hDevInfo,
			spDevInfoData,
			&drvInfoData,
			pDrvInfoDetailData,
			requiredBufferSize,
			nullptr
		))
		{
			err = GetLastError();
			break;
		}

		//
		// Remove device
		// 
		if (!newdev.pDiUninstallDevice(
			nullptr,
			hDevInfo,
			spDevInfoData,
			0,
			&devNeedsReboot
		))
		{
			err = GetLastError();
			break;
		}

		//
		// Uninstall from driver store
		// 
		if (!newdev.pDiUninstallDriverW(
			nullptr,
			pDrvInfoDetailData->InfFileName,
			0,
			&drvNeedsReboot
		))
		{
			err = GetLastError();
			break;
		}

		*rebootRequired = (drvNeedsReboot > 0) || (devNeedsReboot > 0);
	}
	while (FALSE);

	if (pDrvInfoDetailData)
		free(pDrvInfoDetailData);

	(void)SetupDiDestroyDriverInfoList(
		hDevInfo,
		spDevInfoData,
		SPDIT_COMPATDRIVER
	);

	SetLastError(err);

	return ret;
}

static PWSTR wstristr(PCWSTR haystack, PCWSTR needle) {
	do {
		PCWSTR h = haystack;
		PCWSTR n = needle;
		while (towlower(*h) == towlower(*n) && *n) {
			h++;
			n++;
		}
		if (*n == 0) {
			return (PWSTR)haystack;
		}
	} while (*haystack++);
	return NULL;
}

bool devcon::uninstall_device_and_driver(const GUID* classGuid, const std::wstring& hardwareId, bool* rebootRequired)
{
    DWORD err = ERROR_SUCCESS;
    bool succeeded = false;

    HDEVINFO hDevInfo;
    SP_DEVINFO_DATA spDevInfoData;

    hDevInfo = SetupDiGetClassDevs(
        classGuid,
        nullptr, 
        nullptr, 
        DIGCF_PRESENT
    );
    if (hDevInfo == INVALID_HANDLE_VALUE)
    {
        return succeeded;
    }

    spDevInfoData.cbSize = sizeof(SP_DEVINFO_DATA);
	
    for (DWORD i = 0; SetupDiEnumDeviceInfo(hDevInfo, i, &spDevInfoData); i++)
    {
        DWORD DataT;
        LPWSTR p, buffer = nullptr;
        DWORD buffersize = 0;

        // get all devices info
        while (!SetupDiGetDeviceRegistryProperty(hDevInfo,
            &spDevInfoData,
            SPDRP_HARDWAREID,
            &DataT,
            reinterpret_cast<PBYTE>(buffer),
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
                buffer = static_cast<wchar_t*>(LocalAlloc(LPTR, buffersize));
            }
            else
            {
                goto cleanup_DeviceInfo;
            }
        }

        if (GetLastError() == ERROR_INVALID_DATA)
            continue;

        //
        // find device matching hardware ID
        // 
        for (p = buffer; p && *p && (p < &buffer[buffersize]); p += lstrlenW(p) + sizeof(TCHAR))
        {
	        if (wstristr(p, hardwareId.c_str()))
	        {
		        succeeded = ::uninstall_device_and_driver(hDevInfo, &spDevInfoData, rebootRequired);
		        err = GetLastError();
		        break;
	        }
        }

        if (buffer)
	        LocalFree(buffer);
    }

cleanup_DeviceInfo:
    err = GetLastError();
    SetupDiDestroyDeviceInfoList(hDevInfo);
    SetLastError(err);

    return succeeded;
}

static int DetourMessageBoxW(
	HWND    hWnd,
	LPCWSTR lpText,
	LPCWSTR lpCaption,
	UINT    uType
)
{
	hWnd; lpText; lpCaption; uType;

	g_MbCalled = TRUE;

	return IDOK;
};

bool devcon::inf_default_install(const std::wstring& fullInfPath, bool* rebootRequired)
{
	uint32_t errCode = ERROR_SUCCESS;
	SYSTEM_INFO sysInfo;
	HINF hInf = INVALID_HANDLE_VALUE;
	WCHAR InfSectionWithExt[255];
	WCHAR pszDest[280];
	BOOLEAN defaultSection = FALSE;

	GetNativeSystemInfo(&sysInfo);

	hInf = SetupOpenInfFileW(fullInfPath.c_str(), nullptr, INF_STYLE_WIN4, nullptr);

	do
	{
		if (hInf == INVALID_HANDLE_VALUE)
		{
			errCode = GetLastError();
			break;
		}

		if (SetupDiGetActualSectionToInstallW(
			hInf,
			L"DefaultInstall",
			InfSectionWithExt,
			0xFFu,
			reinterpret_cast<PDWORD>(&sysInfo.lpMinimumApplicationAddress),
			nullptr)
			&& SetupFindFirstLineW(hInf, InfSectionWithExt, nullptr, reinterpret_cast<PINFCONTEXT>(&sysInfo.lpMaximumApplicationAddress)))
		{
			defaultSection = TRUE;

			if (StringCchPrintfW(pszDest, 280ui64, L"DefaultInstall 132 %ws", fullInfPath.c_str()) < 0)
			{
				errCode = GetLastError();
				break;
			}
			
			DetourTransactionBegin();
			DetourUpdateThread(GetCurrentThread());
			DetourAttach((void**)&real_MessageBoxW, DetourMessageBoxW);
			DetourTransactionCommit();

			InstallHinfSectionW(nullptr, nullptr, pszDest, 0);

			DetourTransactionBegin();
			DetourUpdateThread(GetCurrentThread());
			DetourDetach((void**)&real_MessageBoxW, DetourMessageBoxW);
			DetourTransactionCommit();

			//
			// If a message box call was intercepted, we encountered an error
			// 
			if (g_MbCalled)
			{
				g_MbCalled = FALSE;
				errCode = ERROR_PNP_REBOOT_REQUIRED;
				break;
			}
		}

		if (!SetupFindFirstLineW(hInf, L"Manufacturer", nullptr, reinterpret_cast<PINFCONTEXT>(&sysInfo.lpMaximumApplicationAddress)))
		{
			if (!defaultSection)
			{
				errCode = ERROR_SECTION_NOT_FOUND;
			}
			break;
		}

		Newdev newdev;
		BOOL reboot;

		if (!newdev.pDiInstallDriverW)
		{
			SetLastError(ERROR_INVALID_FUNCTION);
			return false;
		}

		const auto ret = newdev.pDiInstallDriverW(
			nullptr,
			fullInfPath.c_str(),
			0,
			&reboot
		);

		if (rebootRequired)
			*rebootRequired = reboot > 1;

	} while (FALSE);

	if (hInf != INVALID_HANDLE_VALUE)
	{
		SetupCloseInfFile(hInf);
	}

	SetLastError(errCode);
	return errCode == ERROR_SUCCESS;
}
