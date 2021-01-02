#pragma once

#include <guiddef.h>
#include <string>

namespace devcon
{
    bool create(std::wstring className, const GUID *classGuid, std::wstring hardwareId);

	bool create(std::string className, const GUID *classGuid, std::string hardwareId);

    bool restart_bth_usb_device();

	bool devcon::install_driver(const std::wstring& fullInfPath, bool* rebootRequired);
};
