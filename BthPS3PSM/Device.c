/*
* BthPS3PSM - Windows kernel-mode BTHUSB lower filter driver
* Copyright (C) 2018-2019  Nefarius Software Solutions e.U. and Contributors
*
* This program is free software: you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation, either version 3 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/


#include "driver.h"
#include "device.tmh"
#include <usb.h>
#include <usbdi.h>
#include <usbdlib.h>
#include <wdfusb.h>

#ifdef ALLOC_PRAGMA
#pragma alloc_text (PAGE, BthPS3PSMCreateDevice)
#pragma alloc_text (PAGE, BthPS3PSM_EvtDevicePrepareHardware)
#endif

NTSTATUS
BthPS3PSMCreateDevice(
    _Inout_ PWDFDEVICE_INIT DeviceInit
)
/*++

Routine Description:

    Worker routine called to create a device and its software resources.

Arguments:

    DeviceInit - Pointer to an opaque init structure. Memory for this
                    structure will be freed by the framework when the WdfDeviceCreate
                    succeeds. So don't access the structure after that point.

Return Value:

    NTSTATUS

--*/
{
    WDF_OBJECT_ATTRIBUTES           deviceAttributes;
    WDFDEVICE                       device;
    NTSTATUS                        status;
    WDF_PNPPOWER_EVENT_CALLBACKS    pnpPowerCallbacks;


    PAGED_CODE();

    WdfFdoInitSetFilter(DeviceInit);

    WDF_PNPPOWER_EVENT_CALLBACKS_INIT(&pnpPowerCallbacks);
    pnpPowerCallbacks.EvtDevicePrepareHardware = BthPS3PSM_EvtDevicePrepareHardware;
    WdfDeviceInitSetPnpPowerEventCallbacks(DeviceInit, &pnpPowerCallbacks);

    WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&deviceAttributes, DEVICE_CONTEXT);

    status = WdfDeviceCreate(&DeviceInit, &deviceAttributes, &device);

    if (NT_SUCCESS(status))
    {
        //
        // Query for Compatible IDs and opt-out on unsupported devices
        // 
        if (!IsCompatibleDevice(device))
        {
            TraceEvents(TRACE_LEVEL_WARNING,
                TRACE_QUEUE,
                "It appears we're not loaded within a USB stack, aborting initialization"
            );

            return STATUS_INVALID_DEVICE_REQUEST;
        }

        //
        // Initialize the I/O Package and any Queues
        //
        status = BthPS3PSMQueueInitialize(device);
    }

    return status;
}

NTSTATUS
BthPS3PSM_EvtDevicePrepareHardware(
    WDFDEVICE Device,
    WDFCMRESLIST ResourcesRaw,
    WDFCMRESLIST ResourcesTranslated
)
{
    WDF_USB_DEVICE_CREATE_CONFIG    usbConfig;
    NTSTATUS                        status;
    PDEVICE_CONTEXT                 deviceContext;

    UNREFERENCED_PARAMETER(ResourcesRaw);
    UNREFERENCED_PARAMETER(ResourcesTranslated);

    PAGED_CODE();

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DEVICE, "%!FUNC! Entry");

    deviceContext = DeviceGetContext(Device);

    //
    // Initialize the USB config context.
    //
    WDF_USB_DEVICE_CREATE_CONFIG_INIT(
        &usbConfig,
        USBD_CLIENT_CONTRACT_VERSION_602
    );

    //
    // Allocate framework USB device object
    // 
    // Since we're a filter we _must not_ blindly
    // call WdfUsb* functions but "abuse" the URBs
    // coming from the upper function driver.
    // 
    status = WdfUsbTargetDeviceCreateWithParameters(
        Device,
        &usbConfig,
        WDF_NO_OBJECT_ATTRIBUTES,
        &deviceContext->UsbDevice
    );

    if (!NT_SUCCESS(status)) {

        TraceEvents(TRACE_LEVEL_ERROR,
            TRACE_QUEUE,
            "WdfUsbTargetDeviceCreateWithParameters failed with status %!STATUS!",
            status);
    }

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DEVICE, "%!FUNC! Exit");

    return status;
}
