#include "stdafx.h"

#include <PathCch.h>
#include <devguid.h>

#include <string>
#include <Devcon.h>
#include <BthPS3Setup.h>

const std::wstring g_nullInf = L"BthPS3_PDO_NULL_Device.inf";
const std::wstring g_profileInf = L"BthPS3.inf";
const std::wstring g_filterInf = L"BthPS3PSM.inf";


UINT __stdcall CheckHostRadioPresence(
	MSIHANDLE hInstall
	)
{
	HRESULT hr = S_OK;
	UINT er = ERROR_SUCCESS;

	hr = WcaInitialize(hInstall, "CheckHostRadioPresence");
	ExitOnFailure(hr, "Failed to initialize");

	WcaLog(LOGMSG_STANDARD, "Initialized.");

	HANDLE hRadio = INVALID_HANDLE_VALUE;
	BLUETOOTH_FIND_RADIO_PARAMS radioParams;
	ZeroMemory(&radioParams, sizeof(BLUETOOTH_FIND_RADIO_PARAMS));
	radioParams.dwSize = sizeof(BLUETOOTH_FIND_RADIO_PARAMS);

	auto* const ret = BluetoothFindFirstRadio(&radioParams, &hRadio);

	if (ret)
	{
		BluetoothFindRadioClose(ret);
		MsiSetProperty(hInstall, L"RADIOFOUND", L"1");
		WcaLog(LOGMSG_STANDARD, "Host radio found.");
	}
	else
	{
		WcaLog(LOGMSG_STANDARD, "Host radio not found.");
	}
		
LExit:
	er = SUCCEEDED(hr) ? ERROR_SUCCESS : ERROR_INSTALL_FAILURE;
	return WcaFinalize(er);
}

UINT __stdcall InstallDrivers(
	MSIHANDLE hInstall
	)
{
	HRESULT hr = S_OK;
	UINT er = ERROR_SUCCESS;

	std::wstring nullInfPath(PATHCCH_MAX_CCH, L'\0');
	std::wstring profileInfPath(PATHCCH_MAX_CCH, L'\0');
	std::wstring filterInfPath(PATHCCH_MAX_CCH, L'\0');
	
	hr = WcaInitialize(hInstall, "InstallDrivers");
	ExitOnFailure(hr, "Failed to initialize");

	WcaLog(LOGMSG_STANDARD, "Initialized.");

	bool rebootRequired = false;
	WCHAR targetPath[PATHCCH_MAX_CCH * sizeof(WCHAR)];
	DWORD length = ARRAYSIZE(targetPath);

	(void)MsiGetProperty(hInstall, L"CustomActionData", targetPath, &length);
	
	PathCchCombine(&nullInfPath[0], nullInfPath.size(), targetPath, g_nullInf.c_str());
	PathCchCombine(&profileInfPath[0], profileInfPath.size(), targetPath, g_profileInf.c_str());
	PathCchCombine(&filterInfPath[0], filterInfPath.size(), targetPath, g_filterInf.c_str());

	
	WcaLog(LOGMSG_STANDARD, "Installing PDO NULL driver.");
	if (!devcon::install_driver(nullInfPath, &rebootRequired))
	{
		ExitOnLastError(hr, "Failed to install PDO NULL driver, error: %s",
		                winapi::GetLastErrorStdStr(Dutil_er).c_str());
	}
	WcaLog(LOGMSG_STANDARD, "PDO NULL driver installed.");

	WcaLog(LOGMSG_STANDARD, "Installing BthPS3 driver in driver store.");
	if (!devcon::install_driver(profileInfPath, &rebootRequired))
	{
		ExitOnLastError(hr, "Failed to install BthPS3 driver in driver store, error: %s",
		                winapi::GetLastErrorStdStr().c_str());
	}
	WcaLog(LOGMSG_STANDARD, "BthPS3 driver installed in driver store.");

	WcaLog(LOGMSG_STANDARD, "Enabling profile service for BTHENUM.");
	if (!bthps3::bluetooth::enable_service())
	{
		ExitOnLastError(hr, "Failed to enable profile service for BTHENUM, error: %s",
		                winapi::GetLastErrorStdStr().c_str());
	}
	WcaLog(LOGMSG_STANDARD, "Profile service for BTHENUM enabled.");

	WcaLog(LOGMSG_STANDARD, "Installing BthPS3 driver on BTHENUM PDO.");
	if (!devcon::install_driver(profileInfPath, &rebootRequired))
	{
		ExitOnLastError(hr, "Failed to install BthPS3 driver on BTHENUM PDO, error: %s",
		                winapi::GetLastErrorStdStr().c_str());
	}
	WcaLog(LOGMSG_STANDARD, "BthPS3 driver installed on BTHENUM PDO.");

	WcaLog(LOGMSG_STANDARD, "Installing BthPS3PSM filter driver in driver store.");
	if (!devcon::install_driver(filterInfPath, &rebootRequired))
	{
		ExitOnLastError(hr, "Failed to install BthPS3PSM filter driver in driver store, error: %s",
		                winapi::GetLastErrorStdStr().c_str());
	}
	WcaLog(LOGMSG_STANDARD, "BthPS3PSM filter driver installed in driver store.");
	
	WcaLog(LOGMSG_STANDARD, "Creating virtual hardware node for BthPS3PSM filter driver.");
	if (!devcon::create(L"System", &GUID_DEVCLASS_SYSTEM, BTHPS3PSM_FILTER_HARDWARE_ID))
	{
		ExitOnLastError(hr, "Failed to create virtual hardware node for BthPS3PSM filter driver, error: %s",
		                winapi::GetLastErrorStdStr().c_str());
	}
	WcaLog(LOGMSG_STANDARD, "Virtual hardware node for BthPS3PSM filter driver created.");

	WcaLog(LOGMSG_STANDARD, "Installing BthPS3PSM filter driver on virtual hardware node.");
	if (!devcon::install_driver(filterInfPath, &rebootRequired))
	{
		ExitOnLastError(hr, "Failed to install BthPS3 filter driver on virtual hardware node, error: %s",
		                winapi::GetLastErrorStdStr().c_str());
	}
	WcaLog(LOGMSG_STANDARD, "BthPS3 filter driver installed on virtual hardware node.");
	
LExit:
	er = SUCCEEDED(hr) ? ERROR_SUCCESS : ERROR_INSTALL_FAILURE;
	return WcaFinalize(er);
}

UINT __stdcall UninstallDrivers(
	MSIHANDLE hInstall
	)
{
	HRESULT hr = S_OK;
	UINT er = ERROR_SUCCESS;

	hr = WcaInitialize(hInstall, "UninstallDrivers");
	ExitOnFailure(hr, "Failed to initialize");

	WcaLog(LOGMSG_STANDARD, "Initialized.");

	WCHAR targetPath[MAX_PATH * sizeof(WCHAR)];
	DWORD length = ARRAYSIZE(targetPath);

	(void)MsiGetProperty(hInstall, L"CustomActionData", targetPath, &length);

	MessageBox(NULL, targetPath, L"Path", MB_OK);
	
LExit:
	er = SUCCEEDED(hr) ? ERROR_SUCCESS : ERROR_INSTALL_FAILURE;
	return WcaFinalize(er);
}


// DllMain - Initialize and cleanup WiX custom action utils.
extern "C" BOOL WINAPI DllMain(
	__in HINSTANCE hInst,
	__in ULONG ulReason,
	__in LPVOID
	)
{
	switch(ulReason)
	{
	case DLL_PROCESS_ATTACH:
		WcaGlobalInitialize(hInst);
		break;

	case DLL_PROCESS_DETACH:
		WcaGlobalFinalize();
		break;
	}

	return TRUE;
}
