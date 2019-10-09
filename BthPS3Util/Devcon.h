#pragma once

#include <guiddef.h>
#include <string>

namespace devcon
{
    bool create(std::wstring className, const GUID *classGuid, std::wstring hardwareId);

	bool enable_disable_by_compatible_id(std::wstring compatibleId, bool state);

    bool enable_disable_bth_usb_device(bool state);
};
