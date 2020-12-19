#pragma once

#include <guiddef.h>
#include <string>

namespace devcon
{
    bool create(std::wstring className, const GUID *classGuid, std::wstring hardwareId);

	bool create(std::string className, const GUID *classGuid, std::string hardwareId);

    bool enable_disable_bth_usb_device(bool state);
};
