using System;
using System.Diagnostics.CodeAnalysis;
using System.Runtime.ConstrainedExecution;
using System.Runtime.InteropServices;
using System.Security;

namespace BthPS3CfgUI;

public static class BluetoothHelper
{
    public static bool IsBluetoothRadioAvailable
    {
        get
        {
            BLUETOOTH_FIND_RADIO_PARAMS radioParams = new();
            radioParams.Init();

            IntPtr findHandle = BluetoothFindFirstRadio(ref radioParams, out IntPtr radioHandle);

            if (findHandle == IntPtr.Zero)
            {
                return false;
            }

            BluetoothFindRadioClose(findHandle);
            CloseHandle(radioHandle);

            return true;
        }
    }

    [DllImport("BluetoothApis.dll", SetLastError = true)]
    private static extern IntPtr
        BluetoothFindFirstRadio(ref BLUETOOTH_FIND_RADIO_PARAMS pbtfrp, out IntPtr phRadio);

    [DllImport("BluetoothApis.dll", SetLastError = true)]
    private static extern bool BluetoothFindRadioClose(IntPtr hFind);

    [DllImport("kernel32.dll", SetLastError = true)]
    [ReliabilityContract(Consistency.WillNotCorruptState, Cer.Success)]
    [SuppressUnmanagedCodeSecurity]
    [return: MarshalAs(UnmanagedType.Bool)]
    private static extern bool CloseHandle(IntPtr hObject);

    [StructLayout(LayoutKind.Sequential)]
    [SuppressMessage("ReSharper", "InconsistentNaming")]
    [SuppressMessage("ReSharper", "MemberCanBePrivate.Local")]
    private struct BLUETOOTH_FIND_RADIO_PARAMS
    {
        public uint dwSize;

        public void Init()
        {
            dwSize = (uint)Marshal.SizeOf(typeof(BLUETOOTH_FIND_RADIO_PARAMS));
        }
    }
}