#pragma once

#include <guiddef.h>
#include <string>

namespace devcon
{
    bool create(const std::wstring& className, const GUID *classGuid, const std::wstring& hardwareId);

    bool restart_bth_usb_device();

	bool enable_disable_bth_usb_device(bool state);
	
	bool install_driver(const std::wstring& fullInfPath, bool* rebootRequired);

	bool add_device_class_lower_filter(const GUID* classGuid, const std::wstring& filterName);

	bool remove_device_class_lower_filter(const GUID* classGuid, const std::wstring& filterName);

	bool uninstall_device_and_driver(const GUID *classGuid, const std::wstring& hardwareId, bool* rebootRequired);
};
