// BthPS3Util.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include "BthPS3Util.h"

using namespace winreg;
using namespace colorwin;


#define LOWER_FILTERS L"LowerFilters"
#define BUFSIZE 4096


int main(int, char* argv[])
{
	argh::parser cmdl;
	cmdl.add_params({
		"--inf-path",
		"--bin-path",
		"--device-index",
		"--hardware-id",
		"--class-name",
		"--class-guid"
	});
	cmdl.parse(argv);
	std::string infPath, binPath, hwId, className, classGuid;
	ULONG deviceIndex = 0;

	DWORD bytesReturned = 0;
	DWORD err = ERROR_SUCCESS;
	BLUETOOTH_LOCAL_SERVICE_INFO SvcInfo = { 0 };
	wcscpy_s(SvcInfo.szName, sizeof(SvcInfo.szName) / sizeof(WCHAR), BthPS3ServiceName);

#pragma region BTHENUM Profile Driver actions

	if (cmdl[{ "--enable-service" }])
	{
		if (FALSE == winapi::AdjustProcessPrivileges())
		{
			std::cout << color(red) <<
				"Failed to gain required privileges, error: "
				<< winapi::GetLastErrorStdStr() << std::endl;
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
			std::cout << color(red) <<
				"BluetoothSetLocalServiceInfo failed, error: "
				<< winapi::GetLastErrorStdStr() << std::endl;
			return GetLastError();
		}

		std::cout << color(green) << "Service enabled successfully" << std::endl;

		return EXIT_SUCCESS;
	}

	if (cmdl[{ "--disable-service" }])
	{
		if (FALSE == winapi::AdjustProcessPrivileges())
		{
			std::cout << color(red) <<
				"Failed to gain required privileges, error: "
				<< winapi::GetLastErrorStdStr() << std::endl;
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
			std::cout << color(red) <<
				"BluetoothSetLocalServiceInfo failed, error: "
				<< winapi::GetLastErrorStdStr() << std::endl;
			return GetLastError();
		}

		std::cout << color(green) << "Service disabled successfully" << std::endl;

		return EXIT_SUCCESS;
	}

#pragma endregion

#pragma region BthPS3PSM Lower Filter Driver actions

	if (cmdl[{ "--create-filter-service" }])
	{
		if (!(cmdl({ "--bin-path" }) >> binPath)) {
			std::cout << color(red) << "SYS path missing" << std::endl;
			return EXIT_FAILURE;
		}

		if (PathIsRelativeA(binPath.c_str()))
		{
			CHAR  buffer[BUFSIZE] = ("");
			CHAR** lppPart = { NULL };

			if (0 == GetFullPathNameA(
				binPath.c_str(),
				BUFSIZ,
				buffer,
				lppPart
			))
			{
				std::cout << color(red) <<
					"GetFullPathNameA failed, error: "
					<< winapi::GetLastErrorStdStr() << std::endl;
				return GetLastError();
			}

			binPath = buffer;
		}

		auto ret = winapi::CreateDriverService(
			BthPS3FilterServiceName,
			"PlayStation(R) 3 Bluetooth Filter Service",
			binPath.c_str()
		);

		if (!ret) {
			std::cout << color(red) <<
				"Service creation failed, error: "
				<< winapi::GetLastErrorStdStr() << std::endl;
			return GetLastError();
		}

		std::cout << color(green) << "Service created successfully" << std::endl;
		return EXIT_SUCCESS;
	}

	if (cmdl[{ "--delete-filter-service" }])
	{
		auto ret = winapi::DeleteDriverService(
			BthPS3FilterServiceName
		);

		if (!ret) {
			std::cout << color(red) <<
				"Service deletion failed, error: "
				<< winapi::GetLastErrorStdStr() << std::endl;
			return GetLastError();
		}

		std::cout << color(green) << "Service deleted successfully" << std::endl;
		return EXIT_SUCCESS;
	}

	if (cmdl[{ "--enable-filter" }])
	{
		auto key = SetupDiOpenClassRegKey(&GUID_DEVCLASS_BLUETOOTH, KEY_ALL_ACCESS);

		if (INVALID_HANDLE_VALUE == key)
		{
			std::cout << color(red) <<
				"Couldn't open Class key, error: "
				<< winapi::GetLastErrorStdStr() << std::endl;
			return GetLastError();
		}

		// wrap it up
		RegKey classKey(key);

		//
		// Check if "LowerFilters" value exists
		// 
		auto values = classKey.EnumValues();
		auto ret = std::find_if(values.begin(), values.end(), [&](std::pair<std::wstring, DWORD> const& ref) {
			return ref.first == LOWER_FILTERS;
			});

		//
		// Value doesn't exist, create with filter driver name
		// 
		if (ret == values.end())
		{
			classKey.SetMultiStringValue(LOWER_FILTERS, { BthPS3FilterName });
			classKey.Close();

			std::cout << color(yellow) <<
				"Filter enabled. Reconnect affected devices or reboot system to apply changes!"
				<< std::endl;

			return EXIT_SUCCESS;
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

			std::cout << color(yellow) <<
				"Filter enabled. Reconnect affected devices or reboot system to apply changes!"
				<< std::endl;

			return EXIT_SUCCESS;
		}

		classKey.Close();

		std::cout << color(green) <<
			"Filter already enabled. No changes were made"
			<< std::endl;

		return ERROR_SUCCESS;
	}

	if (cmdl[{ "--disable-filter" }])
	{
		auto key = SetupDiOpenClassRegKey(&GUID_DEVCLASS_BLUETOOTH, KEY_ALL_ACCESS);

		if (INVALID_HANDLE_VALUE == key)
		{
			std::cout << color(red) <<
				"Couldn't open Class key, error: "
				<< winapi::GetLastErrorStdStr() << std::endl;
			return GetLastError();
		}

		// wrap it up
		RegKey classKey(key);

		//
		// Check if "LowerFilters" value exists
		// 
		auto values = classKey.EnumValues();
		auto ret = std::find_if(values.begin(), values.end(), [&](std::pair<std::wstring, DWORD> const& ref) {
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

			std::cout << color(yellow) <<
				"Filter disabled. Reconnect affected devices or reboot system to apply changes!"
				<< std::endl;

			return EXIT_SUCCESS;
		}

		classKey.Close();

		std::cout << color(green) <<
			"Filter already disabled. No changes were made"
			<< std::endl;

		return EXIT_SUCCESS;
	}

#pragma endregion

#pragma region Generic driver installer

	if (cmdl[{ "--install-driver" }])
	{
		if (!(cmdl({ "--inf-path" }) >> infPath)) {
			std::cout << color(red) << "INF path missing" << std::endl;
			return EXIT_FAILURE;
		}

		BOOL rebootRequired;
		auto ret = DiInstallDriverA(
			nullptr,
			infPath.c_str(),
			(cmdl[{ "--force" }]) ? DIIRFLAG_FORCE_INF : 0,
			& rebootRequired
		);

		if (!ret)
		{
			std::cout << color(red) <<
				"Failed to install driver, error: "
				<< winapi::GetLastErrorStdStr() << std::endl;
			return GetLastError();
		}

		std::cout << color(green) << "Driver installed successfully" << std::endl;

		return (rebootRequired) ? ERROR_RESTART_APPLICATION : EXIT_SUCCESS;
	}

	if (cmdl[{ "--create-device-node" }])
	{
		if (!(cmdl({ "--hardware-id" }) >> hwId)) {
			std::cout << color(red) << "Hardware ID missing" << std::endl;
			return EXIT_FAILURE;
		}

		if (!(cmdl({ "--class-name" }) >> className)) {
			std::cout << color(red) << "Device Class Name missing" << std::endl;
			return EXIT_FAILURE;
		}

		if (!(cmdl({ "--class-guid" }) >> classGuid)) {
			std::cout << color(red) << "Device Class GUID missing" << std::endl;
			return EXIT_FAILURE;
		}

		GUID clID;

		if (UuidFromStringA(reinterpret_cast<RPC_CSTR>(&classGuid[0]), &clID) == RPC_S_INVALID_STRING_UUID)
		{
			std::cout << color(red) << "Device Class GUID format invalid" << std::endl;
			return EXIT_FAILURE;
		}

		auto ret = devcon::create(className, &clID, hwId);

		if (!ret)
		{
			std::cout << color(red) <<
				"Failed to create device node, error: "
				<< winapi::GetLastErrorStdStr() << std::endl;
			return GetLastError();
		}

		std::cout << color(green) << "Device node created successfully" << std::endl;

		return EXIT_SUCCESS;
	}

#pragma endregion

#pragma region Filter settings

	if (cmdl[{ "--enable-psm-patch" }])
	{
		if (!(cmdl({ "--device-index" }) >> deviceIndex)) {
			std::cout << color(yellow) << "Device index missing, defaulting to 0" << std::endl;
		}

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
			std::cout << color(red) <<
				"Couldn't open control device, error: "
				<< winapi::GetLastErrorStdStr() << std::endl;
			return GetLastError();
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

		if (!ret)
		{
			CloseHandle(hDevice);

			std::cout << color(red) <<
				"Couldn't enable PSM patch, error: "
				<< winapi::GetLastErrorStdStr() << std::endl;
			return GetLastError();
		}

		CloseHandle(hDevice);

		std::cout << color(green) << "PSM Patch enabled successfully" << std::endl;

		return EXIT_SUCCESS;
	}

	if (cmdl[{ "--disable-psm-patch" }])
	{
		if (!(cmdl({ "--device-index" }) >> deviceIndex)) {
			std::cout << color(yellow) << "Device index missing, defaulting to 0" << std::endl;
		}

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
			std::cout << color(red) <<
				"Couldn't open control device, error: "
				<< winapi::GetLastErrorStdStr() << std::endl;
			return GetLastError();
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

		if (!ret)
		{
			CloseHandle(hDevice);

			std::cout << color(red) <<
				"Couldn't disable PSM patch, error: "
				<< winapi::GetLastErrorStdStr() << std::endl;
			return GetLastError();
		}

		CloseHandle(hDevice);

		std::cout << color(green) << "PSM Patch disabled successfully" << std::endl;

		return EXIT_SUCCESS;
	}

	if (cmdl[{ "--get-psm-patch" }])
	{
		if (!(cmdl({ "--device-index" }) >> deviceIndex)) {
			std::cout << color(yellow) << "Device index missing, defaulting to 0" << std::endl;
		}

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
			std::cout << color(red) <<
				"Couldn't open control device, error: "
				<< winapi::GetLastErrorStdStr() << std::endl;
			return GetLastError();
		}

		BTHPS3PSM_GET_PSM_PATCHING req;
		req.DeviceIndex = deviceIndex;

		const auto ret = DeviceIoControl(
			hDevice,
			IOCTL_BTHPS3PSM_GET_PSM_PATCHING,
			&req,
			sizeof(req),
			&req,
			sizeof(req),
			&bytesReturned,
			nullptr
		);

		if (!ret)
		{
			CloseHandle(hDevice);

			std::cout << color(red) <<
				"Couldn't fetch PSM patch state, error: "
				<< winapi::GetLastErrorStdStr() << std::endl;
			return GetLastError();
		}

		CloseHandle(hDevice);

		if (req.IsEnabled)
		{
			std::cout << color(cyan) << "PSM Patching is "
				<< color(magenta) << "enabled"
				<< color(cyan) << " for device ";
			std::wcout << std::wstring(req.SymbolicLinkName) << std::endl;
		}
		else
		{
			std::cout << color(cyan) << "PSM Patching is "
				<< color(gray) << "disabled"
				<< color(cyan) << " for device ";
			std::wcout << std::wstring(req.SymbolicLinkName) << std::endl;
		}

		return EXIT_SUCCESS;
	}

#pragma endregion

#pragma region Misc. actions

	if (cmdl[{ "--restart-host-device" }])
	{
		if (!devcon::enable_disable_bth_usb_device(false))
		{
			std::cout << color(red) <<
				"Failed to disable Bluetooth host device, error: "
				<< winapi::GetLastErrorStdStr() << std::endl;
			return GetLastError();
		}

		if (!devcon::enable_disable_bth_usb_device(true))
		{
			std::cout << color(red) <<
				"Failed to enable Bluetooth host device, error: "
				<< winapi::GetLastErrorStdStr() << std::endl;
			return GetLastError();
		}

		std::cout << color(green) << "Bluetooth host device restarted successfully" << std::endl;

		return EXIT_SUCCESS;
	}

	if (cmdl[{ "-v", "--version" }])
	{
		std::cout << "BthPS3Util version " <<
			winapi::GetVersionFromFile(winapi::GetImageBasePath())
			<< " (C) Nefarius Software Solutions e.U."
			<< std::endl;
		return EXIT_SUCCESS;
	}

#pragma endregion

#pragma region Print usage

	std::cout << "usage: .\\BthPS3Util [options]" << std::endl << std::endl;
	std::cout << "  options:" << std::endl;
	std::cout << "    --enable-service          Register BthPS3 service on Bluetooth radio" << std::endl;
	std::cout << "    --disable-service         De-Register BthPS3 service on Bluetooth radio" << std::endl;
	std::cout << "    --create-filter-service   Create service for BthPS3PSM filter driver" << std::endl;
	std::cout << "      --bin-path              Path to the SYS file to install (required)" << std::endl;
	std::cout << "    --delete-filter-service   Delete service for BthPS3PSM filter driver" << std::endl;
	std::cout << "    --install-driver          Invoke the installation of a given PNP driver" << std::endl;
	std::cout << "      --inf-path              Path to the INF file to install (required)" << std::endl;
	std::cout << "      --force                 Force using this driver even if lower ranked (optional)" << std::endl;
	std::cout << "    --create-device-node      Create a new ROOT enumerated virtual device" << std::endl;
	std::cout << "      --hardware-id           Hardware ID of the new device (required)" << std::endl;
	std::cout << "      --class-name            Device Class Name of the new device (required)" << std::endl;
	std::cout << "      --class-guid            Device Class GUID of the new device (required)" << std::endl;
	std::cout << "    --enable-filter           Register BthPS3PSM as lower filter for Bluetooth Class" << std::endl;
	std::cout << "    --disable-filter          De-Register BthPS3PSM as lower filter for Bluetooth Class" << std::endl;
	std::cout << "    --enable-psm-patch        Instructs the filter to enable the PSM patch" << std::endl;
	std::cout << "      --device-index          Zero-based index of affected device (optional)" << std::endl;
	std::cout << "    --disable-psm-patch       Instructs the filter to disable the PSM patch" << std::endl;
	std::cout << "      --device-index          Zero-based index of affected device (optional)" << std::endl;
	std::cout << "    --get-psm-patch           Reports the current state of the PSM patch" << std::endl;
	std::cout << "      --device-index          Zero-based index of affected device (optional)" << std::endl;
	std::cout << "    --restart-host-device     Disable and re-enable Bluetooth host device" << std::endl;
	std::cout << "    -v, --version             Display version of this utility" << std::endl;
	std::cout << std::endl;

#pragma endregion

	return EXIT_FAILURE;
}

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

std::string winapi::GetLastErrorStdStr()
{
	DWORD error = GetLastError();
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
	return std::string();
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
