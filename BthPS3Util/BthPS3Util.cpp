// BthPS3Util.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include "BthPS3Util.h"


using namespace colorwin;


//
// Enable Visual Styles for message box
// 
#pragma comment(linker,"\"/manifestdependency:type='win32' \
	name='Microsoft.Windows.Common-Controls' version='6.0.0.0' \
processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")



int main(int, char* argv[])
{
	argh::parser cmdl;
	cmdl.add_params({
		"--inf-path",
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

	if (cmdl[{"--enable-filter"}])
	{
		auto ret = devcon::add_device_class_lower_filter(&GUID_DEVCLASS_BLUETOOTH, BthPS3FilterName);

		if (ret)
		{
			std::cout << color(yellow) <<
				"Filter enabled. Reconnect affected devices or reboot system to apply changes!"
				<< std::endl;

			return EXIT_SUCCESS;
		}

		std::cout << color(red) <<
			"Failed to modify filter value, error: "
			<< winapi::GetLastErrorStdStr() << std::endl;
		return GetLastError();
	}

	if (cmdl[{"--disable-filter"}])
	{
		auto ret = devcon::remove_device_class_lower_filter(&GUID_DEVCLASS_BLUETOOTH, BthPS3FilterName);

		if (ret)
		{
			std::cout << color(yellow) <<
				"Filter disabled. Reconnect affected devices or reboot system to apply changes!"
				<< std::endl;

			return EXIT_SUCCESS;
		}

		std::cout << color(red) <<
			"Failed to modify filter value, error: "
			<< winapi::GetLastErrorStdStr() << std::endl;
		return GetLastError();
	}

#pragma endregion

#pragma region Generic driver installer

	if (cmdl[{ "--install-driver" }])
	{
		if (!(cmdl({ "--inf-path" }) >> infPath)) {
			std::cout << color(red) << "INF path missing" << std::endl;
			return EXIT_FAILURE;
		}

		bool rebootRequired;

		if (!devcon::install_driver(to_wstring(infPath), &rebootRequired))
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
			std::cout << color(red) << "Device Class GUID format invalid, expected format (no brackets): xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx" << std::endl;
			return EXIT_FAILURE;
		}

		auto ret = devcon::create(to_wstring(className), &clID, to_wstring(hwId));

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

		if (!bthps3::filter::enable_psm_patch(deviceIndex))
		{
			std::cout << color(red) <<
				"Couldn't enable PSM patch, error: "
				<< winapi::GetLastErrorStdStr() << std::endl;
			return GetLastError();
		}

		std::cout << color(green) << "PSM Patch enabled successfully" << std::endl;

		return EXIT_SUCCESS;
	}

	if (cmdl[{ "--disable-psm-patch" }])
	{
		if (!(cmdl({ "--device-index" }) >> deviceIndex)) {
			std::cout << color(yellow) << "Device index missing, defaulting to 0" << std::endl;
		}
		
		if (!bthps3::filter::disable_psm_patch(deviceIndex))
		{
			std::cout << color(red) <<
				"Couldn't disable PSM patch, error: "
				<< winapi::GetLastErrorStdStr() << std::endl;
			return GetLastError();
		}

		std::cout << color(green) << "PSM Patch disabled successfully" << std::endl;

		return EXIT_SUCCESS;
	}

	if (cmdl[{ "--get-psm-patch" }])
	{
		if (!(cmdl({ "--device-index" }) >> deviceIndex)) {
			std::cout << color(yellow) << "Device index missing, defaulting to 0" << std::endl;
		}

		BTHPS3PSM_GET_PSM_PATCHING req;
		
		if (!bthps3::filter::get_psm_patch(&req))
		{
			std::cout << color(red) <<
				"Couldn't fetch PSM patch state, error: "
				<< winapi::GetLastErrorStdStr() << std::endl;
			return GetLastError();
		}

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
		if (!devcon::restart_bth_usb_device())
		{
			std::cout << color(red) <<
				"Failed to restart Bluetooth host device, error: "
				<< winapi::GetLastErrorStdStr() << std::endl;
			return GetLastError();
		}
		
		std::cout << color(green) << "Bluetooth host device restarted successfully" << std::endl;

		return EXIT_SUCCESS;
	}

	if (cmdl[{ "--check-host-radio" }])
	{
		HANDLE hRadio = INVALID_HANDLE_VALUE;
		BLUETOOTH_FIND_RADIO_PARAMS radioParams;
		ZeroMemory(&radioParams, sizeof(BLUETOOTH_FIND_RADIO_PARAMS));
		radioParams.dwSize = sizeof(BLUETOOTH_FIND_RADIO_PARAMS);

		auto* const ret = BluetoothFindFirstRadio(&radioParams, &hRadio);

		if (ret == nullptr)
		{
			if (cmdl[{"--show-dialog"}])
			{
				MessageBox(nullptr,
				           L"Bluetooth Host Radio couldn't be detected. "\
							"Please make sure your USB dongle/Laptop card is plugged in/turned on and "\
							"running with the manufacturer/Windows stock drivers.",
				           L"Bluetooth Host Radio not found",
				           MB_ICONERROR | MB_OK
				);
			}

			std::cout << color(red) <<
				"Bluetooth Host Radio not found, error: "
				<< winapi::GetLastErrorStdStr() << std::endl;
			
			return GetLastError();
		}

		BluetoothFindRadioClose(ret);

		std::cout << color(green) << "Bluetooth host radio detected successfully" << std::endl;
		
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
	std::cout << "    --check-host-radio        Check if Bluetooth Host Radio is currently present" << std::endl;
	std::cout << "      --show-dialog           Present a modal dialog box to the user (optional)" << std::endl;
	std::cout << "    -v, --version             Display version of this utility" << std::endl;
	std::cout << std::endl;

#pragma endregion

	return EXIT_FAILURE;
}
