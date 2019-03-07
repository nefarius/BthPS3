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
    PDEVICE_CONTEXT                 deviceContext;
    WDFDEVICE                       device;
    NTSTATUS                        status;
    WDF_USB_DEVICE_CREATE_CONFIG    usbConfig;


    PAGED_CODE();

    WdfFdoInitSetFilter(DeviceInit);

    WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&deviceAttributes, DEVICE_CONTEXT);

    status = WdfDeviceCreate(&DeviceInit, &deviceAttributes, &device);

    if (NT_SUCCESS(status)) 
    {
        deviceContext = DeviceGetContext(device);

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
            device,
            &usbConfig,
            WDF_NO_OBJECT_ATTRIBUTES,
            &deviceContext->UsbDevice
        );

        //
        // This will typically fail with either
        // - STATUS_INVALID_DEVICE_REQUEST
        // - STATUS_NO_SUCH_DEVICE
        // if getting attached to a non-USB device
        // which will then conveniently cause 
        // unloading the filter automatically.
        // 
        if (!NT_SUCCESS(status)) {

            TraceEvents(TRACE_LEVEL_WARNING,
                TRACE_QUEUE,
                "WdfUsbTargetDeviceCreateWithParameters failed with status %!STATUS!",
                status);

            return status;
        }

        //
        // Initialize the I/O Package and any Queues
        //
        status = BthPS3PSMQueueInitialize(device);
    }

    return status;
}
