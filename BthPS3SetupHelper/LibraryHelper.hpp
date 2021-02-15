#pragma once

#include <bluetoothapis.h>
#include <newdev.h>

#include <type_traits>

class ProcPtr {
public:
    explicit ProcPtr(FARPROC ptr) : _ptr(ptr) {}

    template <typename T, typename = std::enable_if_t<std::is_function_v<T>>>
    operator T* () const {
        return reinterpret_cast<T*>(_ptr);
    }

private:
    FARPROC _ptr;
};

class DllHelper {
public:
    explicit DllHelper(LPCTSTR filename) : _module(LoadLibrary(filename)) {}

    ~DllHelper() { FreeLibrary(_module); }

    ProcPtr operator[](LPCSTR proc_name) const {
        return ProcPtr(GetProcAddress(_module, proc_name));
    }

    static HMODULE _parent_module;

private:
    HMODULE _module;
};


class Bthprops {
    DllHelper _dll{ L"BluetoothApis.dll" };

public:
    decltype(BluetoothFindFirstRadio)* pBluetoothFindFirstRadio = _dll["BluetoothFindFirstRadio"];
    decltype(BluetoothFindRadioClose)* pBluetoothFindRadioClose = _dll["BluetoothFindRadioClose"];
    decltype(BluetoothSetLocalServiceInfo)* pBluetoothSetLocalServiceInfo = _dll["BluetoothSetLocalServiceInfo"];
};

class Newdev {
    DllHelper _dll{ L"Newdev.dll" };

public:
    decltype(DiUninstallDriverW)* pDiUninstallDriverW = _dll["DiUninstallDriverW"];
    decltype(DiInstallDriverW)* pDiInstallDriverW = _dll["DiInstallDriverW"];
    decltype(DiUninstallDevice)* pDiUninstallDevice = _dll["DiUninstallDevice"];
};

