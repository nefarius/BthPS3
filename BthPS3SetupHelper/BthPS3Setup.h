#pragma once

#include "framework.h"

EXTERN_C IMAGE_DOS_HEADER __ImageBase;

namespace winapi
{
    BOOL AdjustProcessPrivileges();

    BOOL CreateDriverService(PCSTR ServiceName, PCSTR DisplayName, PCSTR BinaryPath);

    BOOL DeleteDriverService(PCSTR ServiceName);

    std::string GetLastErrorStdStr();

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
}
