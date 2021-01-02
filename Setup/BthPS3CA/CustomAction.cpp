#include "stdafx.h"

#include <pathcch.h>

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

	do
	{
		WcaLog(LOGMSG_STANDARD, "Installing PDO NULL driver.");
		if (!devcon::install_driver(nullInfPath, &rebootRequired))
		{
			WcaLog(LOGMSG_STANDARD, "Failed to install PDO NULL driver, error: %s",
			       winapi::GetLastErrorStdStr().c_str());
		}
	}
	while (FALSE);
	
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
