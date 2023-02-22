/**********************************************************************************
 *                                                                                *
 * BthPS3 - Windows kernel-mode Bluetooth profile and bus driver                  *
 *                                                                                *
 * BSD 3-Clause License                                                           *
 *                                                                                *
 * Copyright (c) 2018-2023, Nefarius Software Solutions e.U.                      *
 * All rights reserved.                                                           *
 *                                                                                *
 * Redistribution and use in source and binary forms, with or without             *
 * modification, are permitted provided that the following conditions are met:    *
 *                                                                                *
 * 1. Redistributions of source code must retain the above copyright notice, this *
 *    list of conditions and the following disclaimer.                            *
 *                                                                                *
 * 2. Redistributions in binary form must reproduce the above copyright notice,   *
 *    this list of conditions and the following disclaimer in the documentation   *
 *    and/or other materials provided with the distribution.                      *
 *                                                                                *
 * 3. Neither the name of the copyright holder nor the names of its               *
 *    contributors may be used to endorse or promote products derived from        *
 *    this software without specific prior written permission.                    *
 *                                                                                *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"    *
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE      *
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE *
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE   *
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL     *
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR     *
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER     *
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,  *
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE  *
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.           *
 *                                                                                *
 **********************************************************************************/


#include "Driver.h"
#include "Bluetooth.tmh"
#include <bthguid.h>
#include "BthPS3.h"
#include "BthPS3ETW.h"


 //
 // Requests information about underlying radio
 // 
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
BthPS3_RetrieveLocalInfo(
	_In_ PBTHPS3_DEVICE_CONTEXT_HEADER DevCtxHdr
)
{
	NTSTATUS status = STATUS_SUCCESS;
	struct _BRB_GET_LOCAL_BD_ADDR* brb = NULL;
	UCHAR hciVersion;

	FuncEntry(TRACE_BTH);

	do
	{
		brb = (struct _BRB_GET_LOCAL_BD_ADDR*)
			DevCtxHdr->ProfileDrvInterface.BthAllocateBrb(
				BRB_HCI_GET_LOCAL_BD_ADDR,
				POOLTAG_BTHPS3
			);

		if (brb == NULL)
		{
			status = STATUS_INSUFFICIENT_RESOURCES;

			TraceError(
				TRACE_BTH,
				"Failed to allocate brb BRB_HCI_GET_LOCAL_BD_ADDR with status %!STATUS!",
				status
			);

			break;
		}

		if (!NT_SUCCESS(status = BthPS3_SendBrbSynchronously(
			DevCtxHdr->IoTarget,
			DevCtxHdr->HostInitRequest,
			(PBRB)brb,
			sizeof(*brb)
		)))
		{
			TraceError(
				TRACE_BTH,
				"Retrieving local bth address failed with status %!STATUS!",
				status
			);
		}
		else
		{
			DevCtxHdr->LocalBthAddr = brb->BtAddress;
		}

		DevCtxHdr->ProfileDrvInterface.BthFreeBrb((PBRB)brb);

		//
		// Verify HCI major version is high enough
		// 
		if (!NT_SUCCESS(status = BthPS3_GetHciVersion(
			DevCtxHdr->IoTarget,
			&hciVersion,
			NULL
		)))
		{
			TraceEvents(TRACE_LEVEL_WARNING, TRACE_BTH,
				"Retrieving HCI major version failed, status %!STATUS!",
				status
			);
			//
			// Can still operate without this information
			// 
			status = STATUS_SUCCESS;
		}
		else
		{
			TraceInformation(
				TRACE_BTH,
				"Host radio HCI major version %d",
				hciVersion
			);

			EventWriteHciVersion(NULL, hciVersion);

			if (hciVersion < BTHPS3_MIN_SUPPORTED_HCI_MAJOR_VERSION)
			{
				TraceError(
					TRACE_BTH,
					"Host radio HCI major version %d too low, can't continue",
					hciVersion
				);

				EventWriteHciVersionTooLow(NULL, hciVersion);

				//
				// Fail initialization
				// 
				status = STATUS_NOT_SUPPORTED;
			}
		}
	} while (FALSE);

	FuncExit(TRACE_BTH, "status=%!STATUS!", status);

	return status;
}

//
// Grabs driver-to-driver interface
// 
#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
BthPS3_QueryInterfaces(
	_In_ PBTHPS3_SERVER_CONTEXT DevCtx
)
{
	NTSTATUS status;

	PAGED_CODE();

	if (!NT_SUCCESS(status = WdfFdoQueryForInterface(
		DevCtx->Header.Device,
		&GUID_BTHDDI_PROFILE_DRIVER_INTERFACE,
		(PINTERFACE)(&DevCtx->Header.ProfileDrvInterface),
		sizeof(DevCtx->Header.ProfileDrvInterface),
		BTHDDI_PROFILE_DRIVER_INTERFACE_VERSION_FOR_QI,
		NULL
	)))
	{
		TraceError(
			TRACE_BTH,
			"QueryInterface failed for Interface profile driver interface, version %d, Status code %!STATUS!",
			BTHDDI_PROFILE_DRIVER_INTERFACE_VERSION_FOR_QI,
			status
		);
	}

	return status;
}
#pragma code_seg()

//
// Initialize Bluetooth driver-to-driver interface
// 
#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
BthPS3_Initialize(
	_In_ PBTHPS3_SERVER_CONTEXT DevCtx
)
{
	PAGED_CODE();

	return BthPS3_QueryInterfaces(DevCtx);
}
#pragma code_seg()

_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
BthPS3_GetDeviceName(
	WDFIOTARGET IoTarget,
	BTH_ADDR RemoteAddress,
	PCHAR Name
)
{
	FuncEntry(TRACE_BTH);

	NTSTATUS status = STATUS_INVALID_BUFFER_SIZE;
	ULONG index = 0;
	WDF_MEMORY_DESCRIPTOR MemoryDescriptor;
	WDFMEMORY MemoryHandle = NULL;
	PBTH_DEVICE_INFO_LIST pDeviceInfoList = NULL;
	ULONG maxDevices = BTH_DEVICE_INFO_MAX_COUNT;
	ULONG retryCount = 0;

	//
	// Retry increasing the buffer a few times if _a lot_ of devices
	// are cached and the allocated memory can't store them all.
	// 
	for (retryCount = 0; (retryCount <= BTH_DEVICE_INFO_MAX_RETRIES
		&& status == STATUS_INVALID_BUFFER_SIZE); retryCount++)
	{
		if (MemoryHandle != NULL)
		{
			WdfObjectDelete(MemoryHandle);
		}

		if (!NT_SUCCESS(status = WdfMemoryCreate(NULL,
			NonPagedPoolNx,
			POOLTAG_BTHPS3,
			sizeof(BTH_DEVICE_INFO_LIST) + (sizeof(BTH_DEVICE_INFO) * maxDevices),
			&MemoryHandle,
			NULL)))
		{
			return status;
		}

		WDF_MEMORY_DESCRIPTOR_INIT_HANDLE(
			&MemoryDescriptor,
			MemoryHandle,
			NULL
		);

		status = WdfIoTargetSendIoctlSynchronously(
			IoTarget,
			NULL,
			IOCTL_BTH_GET_DEVICE_INFO,
			&MemoryDescriptor,
			&MemoryDescriptor,
			NULL,
			NULL
		);

		//
		// Increase memory to allocate
		// 
		maxDevices += BTH_DEVICE_INFO_MAX_COUNT;
	}

	if (!NT_SUCCESS(status)) {
		WdfObjectDelete(MemoryHandle);
		return status;
	}

	pDeviceInfoList = WdfMemoryGetBuffer(MemoryHandle, NULL);
	status = STATUS_NOT_FOUND;

	for (index = 0; index < pDeviceInfoList->numOfDevices; index++)
	{
		const PBTH_DEVICE_INFO pDeviceInfo = &pDeviceInfoList->deviceList[index];

		if (pDeviceInfo->address == RemoteAddress)
		{
			if (strlen(pDeviceInfo->name) == 0)
			{
				status = STATUS_INVALID_PARAMETER;
				break;
			}

			strcpy_s(Name, BTH_MAX_NAME_SIZE, pDeviceInfo->name);
			status = STATUS_SUCCESS;
			break;
		}
	}

	WdfObjectDelete(MemoryHandle);

	FuncExit(TRACE_BTH, "status=%!STATUS!", status);

	return status;
}

_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
BthPS3_GetHciVersion(
	_In_ WDFIOTARGET IoTarget,
	_Out_ PUCHAR HciMajorVersion,
	_Out_opt_ PUSHORT HciRevision
)
{
	FuncEntry(TRACE_BTH);

	NTSTATUS status;
	WDF_MEMORY_DESCRIPTOR MemoryDescriptor;
	WDFMEMORY MemoryHandle = NULL;
	PBTH_LOCAL_RADIO_INFO pLocalInfo = NULL;
#ifdef _M_IX86
	ULONG bytesReturned;
#else
	ULONGLONG bytesReturned;
#endif

	if (!NT_SUCCESS(status = WdfMemoryCreate(NULL,
		NonPagedPoolNx,
		POOLTAG_BTHPS3,
		sizeof(BTH_LOCAL_RADIO_INFO),
		&MemoryHandle,
		NULL)))
	{
		return status;
	}

	WDF_MEMORY_DESCRIPTOR_INIT_HANDLE(
		&MemoryDescriptor,
		MemoryHandle,
		NULL
	);

	status = WdfIoTargetSendIoctlSynchronously(
		IoTarget,
		NULL,
		IOCTL_BTH_GET_LOCAL_INFO,
		&MemoryDescriptor,
		&MemoryDescriptor,
		NULL,
		&bytesReturned
	);

	if (!NT_SUCCESS(status) || bytesReturned < sizeof(BTH_LOCAL_RADIO_INFO)) {
		WdfObjectDelete(MemoryHandle);
		return status;
	}

	pLocalInfo = WdfMemoryGetBuffer(MemoryHandle, NULL);

	if (HciMajorVersion)
		*HciMajorVersion = pLocalInfo->hciVersion;

	if (HciRevision)
		*HciRevision = pLocalInfo->hciRevision;

	WdfObjectDelete(MemoryHandle);

	FuncExit(TRACE_BTH, "status=%!STATUS!", status);

	return status;
}

_IRQL_requires_max_(PASSIVE_LEVEL)
_IRQL_requires_same_
ScheduledTask_Result_Type
BthPS3_EvtQueuedWorkItemHandler(
	_In_ DMFMODULE DmfModule,
	_In_ VOID* ClientBuffer,
	_In_ VOID* ClientBufferContext
)
{
	UNREFERENCED_PARAMETER(DmfModule);
	UNREFERENCED_PARAMETER(ClientBufferContext);

	FuncEntry(TRACE_BTH);

	const PBTHPS3_QWI_CONTEXT pCtx = ClientBuffer;

	switch (pCtx->IndicationCode)
	{
	case IndicationRemoteConnect:

		(void)L2CAP_PS3_HandleRemoteConnect(
			pCtx->Context.Server,
			&pCtx->IndicationParameters
		);

		break;
	case IndicationRemoteDisconnect:

		(void)L2CAP_PS3_HandleRemoteDisconnect(
			pCtx->Context.Pdo,
			&pCtx->IndicationParameters
		);

		break;
	}

	FuncExitNoReturn(TRACE_BTH);

	return ScheduledTask_WorkResult_Success;
}
