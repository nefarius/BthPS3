#include "BthPS3Setup.h"

BOOL winapi::AdjustProcessPrivileges()
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
		goto exit;
	}

	bRetVal = LookupPrivilegeValue(nullptr, SE_LOAD_DRIVER_NAME, &luid);
	if (!bRetVal)
	{
		err = GetLastError();
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
		goto exit1;
	}

exit1:
	CloseHandle(procToken);
exit:
	return bRetVal;
}

BOOL winapi::CreateDriverService(PCSTR ServiceName, PCSTR DisplayName, PCSTR BinaryPath)
{
	SC_HANDLE hSCManager;
	SC_HANDLE hService;

	hSCManager = OpenSCManagerA(
		nullptr,
		nullptr,
		SC_MANAGER_CREATE_SERVICE
	);

	if (!hSCManager) {
		return FALSE;
	}

	hService = CreateServiceA(
		hSCManager,
		ServiceName,
		DisplayName,
		SERVICE_START | DELETE | SERVICE_STOP,
		SERVICE_KERNEL_DRIVER,
		SERVICE_DEMAND_START,
		SERVICE_ERROR_IGNORE,
		BinaryPath,
		nullptr,
		nullptr,
		nullptr,
		nullptr,
		nullptr
	);

	if (!hService)
	{
		CloseServiceHandle(hSCManager);
		return FALSE;
	}

	CloseServiceHandle(hService);
	CloseServiceHandle(hSCManager);

	return TRUE;
}

BOOL winapi::DeleteDriverService(PCSTR ServiceName)
{
	SC_HANDLE hSCManager;
	SC_HANDLE hService;
	BOOL ret;

	hSCManager = OpenSCManagerA(
		nullptr,
		nullptr,
		SC_MANAGER_CREATE_SERVICE
	);

	if (!hSCManager) {
		return FALSE;
	}

	hService = OpenServiceA(
		hSCManager,
		ServiceName,
		SERVICE_START | DELETE | SERVICE_STOP
	);

	if (!hService) {
		CloseServiceHandle(hSCManager);
		return FALSE;
	}

	ret = DeleteService(hService);

	CloseServiceHandle(hService);
	CloseServiceHandle(hSCManager);

	return ret;
}

std::string winapi::GetLastErrorStdStr(DWORD errorCode)
{
	DWORD error = (errorCode == ERROR_SUCCESS) ? GetLastError() : errorCode;
	if (error)
	{
		LPVOID lpMsgBuf;
		DWORD bufLen = FormatMessageA(
			FORMAT_MESSAGE_ALLOCATE_BUFFER |
			FORMAT_MESSAGE_FROM_SYSTEM |
			FORMAT_MESSAGE_IGNORE_INSERTS,
			nullptr,
			error,
			MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
			(LPSTR)&lpMsgBuf,
			0, nullptr);
		if (bufLen)
		{
			auto lpMsgStr = (LPCSTR)lpMsgBuf;
			std::string result(lpMsgStr, lpMsgStr + bufLen);

			LocalFree(lpMsgBuf);

			return result;
		}
	}
	return std::string("LEER");
}

std::string winapi::GetVersionFromFile(std::string FilePath)
{
	DWORD  verHandle = 0;
	UINT   size = 0;
	LPBYTE lpBuffer = nullptr;
	DWORD  verSize = GetFileVersionInfoSizeA(FilePath.c_str(), &verHandle);
	std::stringstream versionString;

	if (verSize != NULL)
	{
		auto verData = new char[verSize];

		if (GetFileVersionInfoA(FilePath.c_str(), verHandle, verSize, verData))
		{
			if (VerQueryValueA(verData, "\\", (VOID FAR * FAR*) & lpBuffer, &size))
			{
				if (size)
				{
					auto* verInfo = (VS_FIXEDFILEINFO*)lpBuffer;
					if (verInfo->dwSignature == 0xfeef04bd)
					{
						versionString
							<< static_cast<ULONG>(HIWORD(verInfo->dwProductVersionMS)) << "."
							<< static_cast<ULONG>(LOWORD(verInfo->dwProductVersionMS)) << "."
							<< static_cast<ULONG>(HIWORD(verInfo->dwProductVersionLS)) << "."
							<< static_cast<ULONG>(LOWORD(verInfo->dwProductVersionLS));
					}
				}
			}
		}
		delete[] verData;
	}

	return versionString.str();
}

std::string winapi::GetImageBasePath()
{
	char myPath[MAX_PATH + 1] = { 0 };

	GetModuleFileNameA(
		reinterpret_cast<HINSTANCE>(&__ImageBase),
		myPath,
		MAX_PATH + 1
	);

	return std::string(myPath);
}

bool bthps3::bluetooth::enable_service()
{
	DWORD err;
	BLUETOOTH_LOCAL_SERVICE_INFO SvcInfo = {0};
	wcscpy_s(SvcInfo.szName, sizeof(SvcInfo.szName) / sizeof(WCHAR), BthPS3ServiceName);

	if (FALSE == winapi::AdjustProcessPrivileges())
		return false;

	SvcInfo.Enabled = TRUE;

	if (ERROR_SUCCESS != (err = BluetoothSetLocalServiceInfo(
		nullptr, //callee would select the first found radio
		&BTHPS3_SERVICE_GUID,
		0,
		&SvcInfo
	)))
	{
		SetLastError(err);
		return false;
	}

	SetLastError(err);
	return true;
}

bool bthps3::bluetooth::disable_service()
{
	DWORD err;
	BLUETOOTH_LOCAL_SERVICE_INFO SvcInfo = {0};
	wcscpy_s(SvcInfo.szName, sizeof(SvcInfo.szName) / sizeof(WCHAR), BthPS3ServiceName);

	if (FALSE == winapi::AdjustProcessPrivileges())
		return false;

	SvcInfo.Enabled = FALSE;

	if (ERROR_SUCCESS != (err = BluetoothSetLocalServiceInfo(
		nullptr, //callee would select the first found radio
		&BTHPS3_SERVICE_GUID,
		0,
		&SvcInfo
	)))
	{
		SetLastError(err);
		return false;
	}

	SetLastError(err);
	return true;
}

bool bthps3::filter::enable_psm_patch(DWORD deviceIndex)
{
	DWORD bytesReturned = 0;

	const auto hDevice = CreateFile(
		BTHPS3PSM_CONTROL_DEVICE_PATH,
		GENERIC_READ | GENERIC_WRITE,
		FILE_SHARE_READ | FILE_SHARE_WRITE,
		nullptr,
		OPEN_EXISTING,
		FILE_ATTRIBUTE_NORMAL,
		nullptr
	);

	if (hDevice == INVALID_HANDLE_VALUE)
	{
		return false;
	}

	BTHPS3PSM_ENABLE_PSM_PATCHING req;
	req.DeviceIndex = deviceIndex;

	const auto ret = DeviceIoControl(
		hDevice,
		IOCTL_BTHPS3PSM_ENABLE_PSM_PATCHING,
		&req,
		sizeof(BTHPS3PSM_ENABLE_PSM_PATCHING),
		nullptr,
		0,
		&bytesReturned,
		nullptr
	);

	DWORD err = GetLastError();
	CloseHandle(hDevice);
	SetLastError(err);

	return ret > 0;
}

bool bthps3::filter::disable_psm_patch(DWORD deviceIndex)
{
	DWORD bytesReturned = 0;

	const auto hDevice = CreateFile(
		BTHPS3PSM_CONTROL_DEVICE_PATH,
		GENERIC_READ | GENERIC_WRITE,
		FILE_SHARE_READ | FILE_SHARE_WRITE,
		nullptr,
		OPEN_EXISTING,
		FILE_ATTRIBUTE_NORMAL,
		nullptr
	);

	if (hDevice == INVALID_HANDLE_VALUE)
	{
		return false;
	}

	BTHPS3PSM_DISABLE_PSM_PATCHING req;
	req.DeviceIndex = deviceIndex;

	const auto ret = DeviceIoControl(
		hDevice,
		IOCTL_BTHPS3PSM_DISABLE_PSM_PATCHING,
		&req,
		sizeof(BTHPS3PSM_DISABLE_PSM_PATCHING),
		nullptr,
		0,
		&bytesReturned,
		nullptr
	);

	DWORD err = GetLastError();
	CloseHandle(hDevice);
	SetLastError(err);

	return ret > 0;
}

bool bthps3::filter::get_psm_patch(PBTHPS3PSM_GET_PSM_PATCHING request, DWORD deviceIndex)
{
	DWORD bytesReturned = 0;

	const auto hDevice = CreateFile(
		BTHPS3PSM_CONTROL_DEVICE_PATH,
		GENERIC_READ | GENERIC_WRITE,
		FILE_SHARE_READ | FILE_SHARE_WRITE,
		nullptr,
		OPEN_EXISTING,
		FILE_ATTRIBUTE_NORMAL,
		nullptr
	);

	if (hDevice == INVALID_HANDLE_VALUE)
	{
		return false;
	}

	request->DeviceIndex = deviceIndex;

	const auto ret = DeviceIoControl(
		hDevice,
		IOCTL_BTHPS3PSM_GET_PSM_PATCHING,
		request,
		sizeof(*request),
		request,
		sizeof(*request),
		&bytesReturned,
		nullptr
	);

	DWORD err = GetLastError();
	CloseHandle(hDevice);
	SetLastError(err);

	return ret > 0;
}
