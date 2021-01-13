#pragma once

#include "framework.h"

EXTERN_C IMAGE_DOS_HEADER __ImageBase;

namespace winapi
{
    BOOL AdjustProcessPrivileges();

    BOOL CreateDriverService(PCSTR ServiceName, PCSTR DisplayName, PCSTR BinaryPath);

    BOOL DeleteDriverService(PCSTR ServiceName);

    std::string GetLastErrorStdStr(DWORD errorCode = ERROR_SUCCESS);

    std::string GetVersionFromFile(std::string FilePath);

    std::string GetImageBasePath();
};

namespace bthps3
{
	namespace bluetooth
	{
		bool enable_service();

		bool disable_service();
	}

	namespace filter
	{
		bool enable_psm_patch(DWORD deviceIndex = 0);

		bool disable_psm_patch(DWORD deviceIndex = 0);

		bool get_psm_patch(PBTHPS3PSM_GET_PSM_PATCHING request, DWORD deviceIndex = 0);

		bool is_present();
	}
}
