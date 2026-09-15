using System;
using System.Diagnostics;
using System.Runtime.InteropServices;

namespace Nefarius.BthPS3.Setup;

/// <summary>
///     Processor architecture helpers for picking packaged nefcon binaries.
/// </summary>
internal static class ArchitectureInfo
{
    /// <summary>
    ///     Folder name under nefcon: <c>arm64</c> on ARM64-native hosts, otherwise
    ///     <see cref="RuntimeInformation.OSArchitecture" /> (e.g. <c>X64</c>).
    /// </summary>
    public static string PlatformShortName =>
        IsArm64
            ? "arm64"
            : RuntimeInformation.OSArchitecture.ToString();

    private static bool IsArm64
    {
        get
        {
            IntPtr handle = Process.GetCurrentProcess().Handle;
            IsWow64Process2(handle, out _, out ushort nativeMachine);
            return nativeMachine == 0xaa64;
        }
    }

    [DllImport("kernel32.dll", SetLastError = true)]
    private static extern bool IsWow64Process2(
        IntPtr process,
        out ushort processMachine,
        out ushort nativeMachine);
}
