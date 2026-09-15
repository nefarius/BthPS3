using System;
using System.Linq;

using Nefarius.Utilities.Bluetooth;
using Nefarius.Utilities.DeviceManagement.PnP;

namespace Nefarius.BthPS3.Setup;

/// <summary>
///     The Bluetooth host radio transport, as classified by <see cref="RadioTransport" />.
/// </summary>
/// <remarks>
///     Mirrors the transport classification implemented by the <c>BthPS3PSM_QueryTransportType</c> kernel-mode
///     routine so setup can decide how (and whether) to reload the host radio device stack.
/// </remarks>
public enum RadioTransportType
{
    /// <summary>
    ///     The transport could not be determined, or is neither USB nor BTHX.
    /// </summary>
    Unsupported,

    /// <summary>
    ///     The radio is attached via USB (<c>bthusb.sys</c> or a vendor equivalent).
    /// </summary>
    Usb,

    /// <summary>
    ///     The radio is attached via the Bluetooth Extensibility Transport (BTHX), bound to Microsoft's inbox
    ///     <c>BthMini.sys</c> function driver (e.g. PCIe or UART-attached controllers).
    /// </summary>
    Bthx
}

/// <summary>
///     Detects the transport a Bluetooth host radio is attached through, so setup can pick a compatible
///     device-stack reload strategy for both USB and BTHX (BthMini) radios.
/// </summary>
internal static class RadioTransport
{
    private const string UsbEnumeratorName = "USB";
    private const string BthxCompatibleId = "MS_BTHX_BTHMINI";
    private const string BthMiniServiceName = "BthMini";

    /// <summary>
    ///     Attempts to locate the currently present Bluetooth host radio device node.
    /// </summary>
    /// <param name="device">The located host radio <see cref="PnPDevice" />, if found.</param>
    /// <returns>True if a host radio device node was found, false otherwise.</returns>
    public static bool TryGetHostRadioDevice(out PnPDevice device)
    {
        return Devcon.FindByInterfaceGuid(HostRadio.DeviceInterface, out device);
    }

    /// <summary>
    ///     Classifies the transport a given Bluetooth host radio device node is attached through.
    /// </summary>
    /// <param name="device">The host radio device node to classify.</param>
    /// <returns>The detected <see cref="RadioTransportType" />.</returns>
    /// <remarks>
    ///     USB is identified by the <c>USB</c> enumerator name. BTHX (non-USB) radios are identified via the
    ///     documented <c>MS_BTHX_BTHMINI</c> compatible ID or, as a fallback, the <c>BthMini</c> service name -
    ///     the same signals the <c>BthPS3PSM</c> filter driver uses to decide whether to attach.
    /// </remarks>
    public static RadioTransportType GetTransportType(PnPDevice device)
    {
        string? enumeratorName = device.GetProperty<string>(DevicePropertyKey.Device_EnumeratorName);

        if (string.Equals(enumeratorName, UsbEnumeratorName, StringComparison.OrdinalIgnoreCase))
        {
            return RadioTransportType.Usb;
        }

        if (device.CompatibleIds?.Any(id => string.Equals(id, BthxCompatibleId, StringComparison.OrdinalIgnoreCase))
            == true)
        {
            return RadioTransportType.Bthx;
        }

        string? serviceName = device.GetProperty<string>(DevicePropertyKey.Device_Service);

        if (string.Equals(serviceName, BthMiniServiceName, StringComparison.OrdinalIgnoreCase))
        {
            return RadioTransportType.Bthx;
        }

        return RadioTransportType.Unsupported;
    }
}
