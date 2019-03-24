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


#pragma once

static
PVOID
USBPcapURBGetBufferPointer(
    ULONG length,
    PVOID buffer,
    PMDL  bufferMDL
)
{
    ASSERT((length == 0) ||
        ((length != 0) && (buffer != NULL || bufferMDL != NULL)));

    if (length == 0)
    {
        return NULL;
    }
    else if (buffer != NULL)
    {
        return buffer;
    }
    else if (bufferMDL != NULL)
    {
        PVOID address = MmGetSystemAddressForMdlSafe(bufferMDL,
            NormalPagePriority);
        return address;
    }
    else
    {
        //DkDbgStr("Invalid buffer!");
        return NULL;
    }
}

//
// Helper to query the underlying bus about Compatible IDs
// 
static
BOOLEAN
IsCompatibleDevice(
    WDFDEVICE Device
)
{
    NTSTATUS            status;
    PDEVICE_OBJECT      pdo;
    KEVENT              ke;
    IO_STATUS_BLOCK     iosb;
    PIRP                irp;
    PIO_STACK_LOCATION  stack;
    BOOLEAN             ret = FALSE;
    PCWSTR              szIter = NULL;

    pdo = WdfDeviceWdmGetPhysicalDevice(Device);
    KeInitializeEvent(&ke, NotificationEvent, FALSE);

    RtlZeroMemory(&iosb, sizeof(IO_STATUS_BLOCK));
    irp = IoBuildSynchronousFsdRequest(
        IRP_MJ_PNP, pdo,
        NULL, 0, NULL,
        &ke, &iosb
    );
    irp->IoStatus.Status = STATUS_NOT_SUPPORTED; // required initialize
    stack = IoGetNextIrpStackLocation(irp);
    stack->MinorFunction = IRP_MN_QUERY_ID;
    stack->Parameters.QueryId.IdType = BusQueryCompatibleIDs;

    status = IoCallDriver(pdo, irp);

    if (status == STATUS_PENDING)
    {
        // 
        // Normally, we will not hit this, because QueryId should not be an expensive operation
        // 
        KeWaitForSingleObject(&ke, Executive, KernelMode, FALSE, NULL);
    }

    if (NT_SUCCESS(status))
    {
        WCHAR *retval = (WCHAR*)iosb.Information;

        //
        // Walk through devices Compatible IDs
        // 
        for (szIter = retval; *szIter; szIter += wcslen(szIter) + 1)
        {
            //
            // Match against Wireless Controller (E0h)
            // 
            if (0 == wcscmp(szIter, BTHPS3PSM_FILTER_COMPATIBLE_ID))
            {
                ret = TRUE;
                break;
            }
        }

        ExFreePool(retval); // IRP_MN_QUERY_ID requires this
    }

    return ret;
}

static
BOOLEAN
IsDummyDevice(
    WDFDEVICE Device
)
{
    NTSTATUS                status;
    WDFMEMORY               propertyHWIDMemory;
    WDF_OBJECT_ATTRIBUTES   attributes;
    BOOLEAN                 ret = FALSE;

    DECLARE_UNICODE_STRING_SIZE(currentHwId, 200);
    DECLARE_CONST_UNICODE_STRING(hardwareId, BTHPS3PSM_FILTER_HARDWARE_ID);

    WDF_OBJECT_ATTRIBUTES_INIT(&attributes);
    attributes.ParentObject = Device;

    status = WdfDeviceAllocAndQueryProperty(
        Device,
        DevicePropertyHardwareID,
        NonPagedPoolNx,
        &attributes,
        &propertyHWIDMemory
    );

    if (NT_SUCCESS(status))
    {
        RtlInitUnicodeString(&currentHwId, WdfMemoryGetBuffer(propertyHWIDMemory, NULL));

        ret = (0 == RtlCompareUnicodeString(&currentHwId, &hardwareId, TRUE));

        WdfObjectDelete(propertyHWIDMemory);
    }

    return ret;
}
