#include "stdafx.h"

#include <devguid.h>

#include <string>
#include <Devcon.h>
#include <BthPS3Setup.h>

const std::wstring g_nullInf = L"BthPS3_PDO_NULL_Device.inf";
const std::wstring g_profileInf = L"BthPS3.inf";
const std::wstring g_filterInf = L"BthPS3PSM.inf";


//
// Test if Bluetooth host radio is present, set property "RADIOFOUND" if so
// 
UINT __stdcall CheckHostRadioPresence(
	MSIHANDLE hInstall
	)
{
	HRESULT hr = S_OK;
	UINT er = ERROR_SUCCESS;
	Bthprops bth;
	
	hr = WcaInitialize(hInstall, "CheckHostRadioPresence");
	ExitOnFailure(hr, "Failed to initialize");

	WcaLog(LOGMSG_STANDARD, "Initialized.");

	
	HANDLE hRadio = INVALID_HANDLE_VALUE;
	BLUETOOTH_FIND_RADIO_PARAMS radioParams;
	ZeroMemory(&radioParams, sizeof(BLUETOOTH_FIND_RADIO_PARAMS));
	radioParams.dwSize = sizeof(BLUETOOTH_FIND_RADIO_PARAMS);

	ExitOnNull(bth.pBluetoothFindFirstRadio, hr, E_INVALIDARG, "BluetoothFindFirstRadio not found");
	ExitOnNull(bth.pBluetoothFindRadioClose, hr, E_INVALIDARG, "BluetoothFindRadioClose not found");
	
	auto* const ret = bth.pBluetoothFindFirstRadio(&radioParams, &hRadio);

	if (ret)
	{
		bth.pBluetoothFindRadioClose(ret);
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

//
// Driver installation logic
// 
UINT __stdcall InstallDrivers(
	MSIHANDLE hInstall
	)
{
	HRESULT hr = S_OK;
	UINT er = ERROR_SUCCESS;

	std::wstring nullInfPath;
	std::wstring profileInfPath;
	std::wstring filterInfPath;
	
	hr = WcaInitialize(hInstall, "InstallDrivers");
	ExitOnFailure(hr, "Failed to initialize");

	WcaLog(LOGMSG_STANDARD, "Initialized.");

	bool rr1 = false, rr2 = false, rr3 = false;
	WCHAR targetPath[MAX_PATH * sizeof(WCHAR)];
	DWORD length = ARRAYSIZE(targetPath);

	(void)MsiGetProperty(hInstall, L"CustomActionData", targetPath, &length);
	
	nullInfPath =  std::wstring(targetPath) + L"\\"  + g_nullInf;
	profileInfPath = std::wstring(targetPath) + L"\\" + g_profileInf;
	filterInfPath = std::wstring(targetPath) + L"\\" + g_filterInf;

	
	WcaLog(LOGMSG_STANDARD, "Installing BthPS3PSM filter driver in driver store.");
	if (!devcon::install_driver(filterInfPath, &rr1))
	{
		ExitOnLastError(hr, "Failed to install BthPS3PSM filter driver in driver store, error: %s",
		                winapi::GetLastErrorStdStr(Dutil_er).c_str());
	}
	WcaLog(LOGMSG_STANDARD, "BthPS3PSM filter driver installed in driver store.");

	WcaLog(LOGMSG_STANDARD, "Installing BthPS3PSM filter driver on virtual hardware node.");
	if (!devcon::install_driver(filterInfPath, &rr2))
	{
		ExitOnLastError(hr, "Failed to install BthPS3 filter driver on virtual hardware node, error: %s",
		                winapi::GetLastErrorStdStr(Dutil_er).c_str());
	}
	WcaLog(LOGMSG_STANDARD, "BthPS3 filter driver installed on virtual hardware node.");

	WcaLog(LOGMSG_STANDARD, "Adding BthPS3PSM as Bluetooth class filters.");
	if (!devcon::add_device_class_lower_filter(&GUID_DEVCLASS_BLUETOOTH, BthPS3FilterName))
	{
		ExitOnLastError(hr, "Failed to add BthPS3PSM to class filters, error: %s",
		                winapi::GetLastErrorStdStr(Dutil_er).c_str());
	}
	WcaLog(LOGMSG_STANDARD, "BthPS3PSM added to Bluetooth class filters.");
	
	WcaLog(LOGMSG_STANDARD, "Installing BthPS3 driver in driver store.");
	if (!devcon::install_driver(profileInfPath, &rr3))
	{
		ExitOnLastError(hr, "Failed to install BthPS3 driver in driver store, error: %s",
		                winapi::GetLastErrorStdStr(Dutil_er).c_str());
	}
	WcaLog(LOGMSG_STANDARD, "BthPS3 driver installed in driver store.");

	WcaLog(LOGMSG_STANDARD, "Installing PDO NULL driver.");
	if (!devcon::install_driver(nullInfPath, nullptr))
	{
		ExitOnLastError(hr, "Failed to install PDO NULL driver, error: %s",
		                winapi::GetLastErrorStdStr(Dutil_er).c_str());
	}
	WcaLog(LOGMSG_STANDARD, "PDO NULL driver installed.");
	
		
	WcaLog(LOGMSG_STANDARD, "Enabling profile service for BTHENUM.");
	if (!bthps3::bluetooth::enable_service())
	{
		ExitOnLastError(hr, "Failed to enable profile service for BTHENUM, error: %s",
		                winapi::GetLastErrorStdStr(Dutil_er).c_str());
	}
	WcaLog(LOGMSG_STANDARD, "Profile service for BTHENUM enabled.");


	if (!bthps3::filter::enable_psm_patch()
		|| rr1 || rr2 || rr3)
	{
		WcaLog(LOGMSG_STANDARD, "A system reboot is required.");
		//
		// This will fail as deferred custom actions have no access to MSI properties.
		// Always schedule reboot in installer instead.
		// 
		(void)MsiSetMode(hInstall, MSIRUNMODE_REBOOTATEND, TRUE);		
	}
	
LExit:

	if (FAILED(hr))
	{
		(void)bthps3::bluetooth::disable_service();
		(void)devcon::remove_device_class_lower_filter(&GUID_DEVCLASS_BLUETOOTH, BthPS3FilterName);
	}
	
	er = SUCCEEDED(hr) ? ERROR_SUCCESS : ERROR_INSTALL_FAILURE;
	return WcaFinalize(er);
}

//
// Driver removal logic
// 
UINT __stdcall UninstallDrivers(
	MSIHANDLE hInstall
	)
{
	HRESULT hr = S_OK;
	UINT er = ERROR_SUCCESS;

	hr = WcaInitialize(hInstall, "UninstallDrivers");
	ExitOnFailure(hr, "Failed to initialize");

	WcaLog(LOGMSG_STANDARD, "Initialized.");

	bool rr1 = false, rr2 = false;
	WCHAR targetPath[MAX_PATH * sizeof(WCHAR)];
	DWORD length = ARRAYSIZE(targetPath);

	(void)MsiGetProperty(hInstall, L"CustomActionData", targetPath, &length);


	WcaLog(LOGMSG_STANDARD, "Removing BthPS3PSM from Bluetooth class filters.");
	(void)devcon::remove_device_class_lower_filter(&GUID_DEVCLASS_BLUETOOTH, BthPS3FilterName);
	WcaLog(LOGMSG_STANDARD, "BthPS3PSM removed from Bluetooth class filters.");
	

	WcaLog(LOGMSG_STANDARD, "Uninstalling BthPS3 driver.");
	(void)devcon::uninstall_device_and_driver(
		&GUID_DEVCLASS_BLUETOOTH,
		L"BTHENUM\\{1cb831ea-79cd-4508-b0fc-85f7c85ae8e0}",
		&rr2
	);
	WcaLog(LOGMSG_STANDARD, "BthPS3 driver removed.");
	
	
	WcaLog(LOGMSG_STANDARD, "Disabling profile service for BTHENUM.");
	(void)bthps3::bluetooth::disable_service();
	WcaLog(LOGMSG_STANDARD, "Profile service for BTHENUM disabled.");


	if (rr1 || rr2)
	{
		WcaLog(LOGMSG_STANDARD, "A system reboot is required.");
		MsiSetProperty(hInstall, L"REBOOTREQUIRED", L"1");
	}
	
LExit:
	er = SUCCEEDED(hr) ? ERROR_SUCCESS : ERROR_INSTALL_FAILURE;
	return WcaFinalize(er);
}

UINT __stdcall CheckOldDriverPresence(
	MSIHANDLE hInstall
	)
{
	HRESULT hr = S_OK;
	UINT er = ERROR_SUCCESS;

	hr = WcaInitialize(hInstall, "CheckOldDriverPresence");
	ExitOnFailure(hr, "Failed to initialize");

	WcaLog(LOGMSG_STANDARD, "Initialized.");
	
	if (bthps3::filter::is_present())
	{
		WcaLog(LOGMSG_STANDARD, "Existing driver found.");
	}
	else
	{
		WcaLog(LOGMSG_STANDARD, "No existing driver found.");
		MsiSetProperty(hInstall, L"FILTERNOTFOUND", L"1");
	}
		
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
