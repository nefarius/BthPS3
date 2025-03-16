// BthPS3Util.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include "BthPS3Util.h"

EXTERN_C IMAGE_DOS_HEADER __ImageBase;

using namespace colorwin;

//
// Enable Visual Styles for message box
// 
#pragma comment(linker,"\"/manifestdependency:type='win32' \
	name='Microsoft.Windows.Common-Controls' version='6.0.0.0' \
processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")


namespace
{
    bool enable_psm_patch(DWORD deviceIndex = 0)
    {
        DWORD bytesReturned = 0;

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
            return false;
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

        DWORD err = GetLastError();
        CloseHandle(hDevice);
        SetLastError(err);

        return ret > 0;
    }

    bool disable_psm_patch(DWORD deviceIndex = 0)
    {
        DWORD bytesReturned = 0;

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
            return false;
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

        DWORD err = GetLastError();
        CloseHandle(hDevice);
        SetLastError(err);

        return ret > 0;
    }

    bool get_psm_patch(PBTHPS3PSM_GET_PSM_PATCHING request, DWORD deviceIndex = 0)
    {
        DWORD bytesReturned = 0;

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
            return false;
        }

        request->DeviceIndex = deviceIndex;

        const auto ret = DeviceIoControl(
            hDevice,
            IOCTL_BTHPS3PSM_GET_PSM_PATCHING,
            request,
            sizeof(*request),
            request,
            sizeof(*request),
            &bytesReturned,
            nullptr
        );

        DWORD err = GetLastError();
        CloseHandle(hDevice);
        SetLastError(err);

        return ret > 0;
    }

    std::string GetVersionFromFile(std::string FilePath)
    {
        DWORD verHandle = 0;
        UINT size = 0;
        LPBYTE lpBuffer = nullptr;
        DWORD verSize = GetFileVersionInfoSizeA(FilePath.c_str(), &verHandle);
        std::stringstream versionString;

        if (verSize != NULL)
        {
            auto verData = new char[verSize];

            if (GetFileVersionInfoA(FilePath.c_str(), verHandle, verSize, verData))
            {
                if (VerQueryValueA(verData, "\\", (VOID FAR * FAR*)&lpBuffer, &size))
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

    std::string GetImageBasePath()
    {
        char myPath[MAX_PATH + 1] = {0};

        GetModuleFileNameA(
            reinterpret_cast<HINSTANCE>(&__ImageBase),
            myPath,
            MAX_PATH + 1
        );

        return std::string(myPath);
    }

    std::string GetLastErrorStdStr(DWORD errorCode = ERROR_SUCCESS)
    {
        DWORD error = (errorCode == ERROR_SUCCESS) ? GetLastError() : errorCode;
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
        return std::string("Unknown");
    }
}

int main(int, char* argv[])
{
    argh::parser cmdl;
    cmdl.add_params({
        "--device-index",
    });
    cmdl.parse(argv);
    ULONG deviceIndex = 0;

#pragma region Filter settings

    if (cmdl[{"--enable-psm-patch"}])
    {
        if (!(cmdl({"--device-index"}) >> deviceIndex))
        {
            std::cout << color(yellow) << "Device index missing, defaulting to 0" << std::endl;
        }

        if (!enable_psm_patch(deviceIndex))
        {
            std::cout << color(red) <<
                "Couldn't enable PSM patch, error: "
                << GetLastErrorStdStr() << std::endl;
            return GetLastError();
        }

        std::cout << color(green) << "PSM Patch enabled successfully" << std::endl;

        return EXIT_SUCCESS;
    }

    if (cmdl[{"--disable-psm-patch"}])
    {
        if (!(cmdl({"--device-index"}) >> deviceIndex))
        {
            std::cout << color(yellow) << "Device index missing, defaulting to 0" << std::endl;
        }

        if (!disable_psm_patch(deviceIndex))
        {
            std::cout << color(red) <<
                "Couldn't disable PSM patch, error: "
                << GetLastErrorStdStr() << std::endl;
            return GetLastError();
        }

        std::cout << color(green) << "PSM Patch disabled successfully" << std::endl;

        return EXIT_SUCCESS;
    }

    if (cmdl[{"--get-psm-patch"}])
    {
        if (!(cmdl({"--device-index"}) >> deviceIndex))
        {
            std::cout << color(yellow) << "Device index missing, defaulting to 0" << std::endl;
        }

        BTHPS3PSM_GET_PSM_PATCHING req;

        if (!get_psm_patch(&req))
        {
            std::cout << color(red) <<
                "Couldn't fetch PSM patch state, error: "
                << GetLastErrorStdStr() << std::endl;
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

    if (cmdl[{"-v", "--version"}])
    {
        std::cout << "BthPS3Util version " <<
            GetVersionFromFile(GetImageBasePath())
            << " (C) Nefarius Software Solutions e.U."
            << std::endl;
        return EXIT_SUCCESS;
    }

#pragma endregion

#pragma region Print usage

    std::cout << "usage: .\\BthPS3Util [options]" << std::endl << std::endl;
    std::cout << "  options:" << std::endl;
    std::cout << "    --enable-psm-patch        Instructs the filter to enable the PSM patch" << std::endl;
    std::cout << "      --device-index          Zero-based index of affected device (optional)" << std::endl;
    std::cout << "    --disable-psm-patch       Instructs the filter to disable the PSM patch" << std::endl;
    std::cout << "      --device-index          Zero-based index of affected device (optional)" << std::endl;
    std::cout << "    --get-psm-patch           Reports the current state of the PSM patch" << std::endl;
    std::cout << "      --device-index          Zero-based index of affected device (optional)" << std::endl;
    std::cout << "    -v, --version             Display version of this utility" << std::endl;
    std::cout << std::endl;

#pragma endregion

    return EXIT_FAILURE;
}
