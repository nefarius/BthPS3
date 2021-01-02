#pragma once

#include <guiddef.h>
#include <string>

namespace devcon
{
    bool create(std::wstring className, const GUID *classGuid, std::wstring hardwareId);

    bool restart_bth_usb_device();

	bool install_driver(const std::wstring& fullInfPath, bool* rebootRequired);

	bool add_device_class_lower_filter(const GUID* classGuid, const std::wstring& filterName);

	bool remove_device_class_lower_filter(const GUID* classGuid, const std::wstring& filterName);
};
