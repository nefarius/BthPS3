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
// Helper to query the underlying bus about a specific PDO property.
// 
static 
NTSTATUS 
BusQueryId(
    WDFDEVICE Device,
    BUS_QUERY_ID_TYPE IdType,
    PWCHAR Buffer,
    ULONG BufferLength
)
{
    NTSTATUS            status;
    PDEVICE_OBJECT      pdo;
    KEVENT              ke;
    IO_STATUS_BLOCK     iosb;
    PIRP                irp;
    PIO_STACK_LOCATION  stack;

    pdo = WdfDeviceWdmGetPhysicalDevice(Device);
    KeInitializeEvent(&ke, NotificationEvent, FALSE);

    RtlZeroMemory(&iosb, sizeof(IO_STATUS_BLOCK));
    irp = IoBuildSynchronousFsdRequest(IRP_MJ_PNP, pdo,
        NULL, 0, NULL,
        &ke, &iosb
    );
    irp->IoStatus.Status = STATUS_NOT_SUPPORTED; // required initialize
    stack = IoGetNextIrpStackLocation(irp);
    stack->MinorFunction = IRP_MN_QUERY_ID;
    stack->Parameters.QueryId.IdType = IdType;

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

        if (wcslen(retval) > BufferLength)
        {
            ExFreePool(retval); // IRP_MN_QUERY_ID requires this
            return STATUS_BUFFER_TOO_SMALL;
        }

        wcscpy(Buffer, retval);
        ExFreePool(retval); // IRP_MN_QUERY_ID requires this
    }

    return status;
}

