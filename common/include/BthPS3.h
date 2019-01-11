#pragma once

DEFINE_GUID(BTHPS3_SERVICE_GUID,
    0x1cb831ea, 0x79cd, 0x4508, 0xb0, 0xfc, 0x85, 0xf7, 0xc8, 0x5a, 0xe8, 0xe0);
// {1cb831ea-79cd-4508-b0fc-85f7c85ae8e0}

DEFINE_GUID(GUID_DEVINTERFACE_BTHPS3PSM,
    0x1e1f8b68, 0xeaa2, 0x4d19, 0x8b, 0x02, 0xe8, 0xb0, 0x91, 0x6c, 0x77, 0xdb);
// {1e1f8b68-eaa2-4d19-8b02-e8b0916c77db}

extern __declspec(selectany) PCWSTR BthPS3FilterName = L"BthPS3PSM";
extern __declspec(selectany) PCWSTR BthPS3ServiceName = L"BthPS3Service";

// 0x11 -> 0x5053
#define PSM_DS3_HID_CONTROL     0x5053
// 0x13 -> 0x5055
#define PSM_DS3_HID_INTERRUPT   0x5055
