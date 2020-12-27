#include "stdafx.h"


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
