/**********************************************************************************
 *                                                                                *
 * BthPS3 - Windows kernel-mode Bluetooth profile and bus driver                  *
 *                                                                                *
 * BSD 3-Clause License                                                           *
 *                                                                                *
 * Copyright (c) 2018-2022, Nefarius Software Solutions e.U.                      *
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
#include "Device.tmh"

#ifdef ALLOC_PRAGMA
#pragma alloc_text (PAGE, BthPS3_CreateDevice)
#pragma alloc_text (PAGE, BthPS3_EvtWdfDeviceSelfManagedIoCleanup)
#pragma alloc_text (PAGE, BthPS3_OpenFilterIoTarget)
#endif


 //
 // Framework device creation entry point
 // 
NTSTATUS
BthPS3_CreateDevice(
	_Inout_ PWDFDEVICE_INIT DeviceInit
)
{
	WDF_OBJECT_ATTRIBUTES attributes;
	WDFDEVICE device = NULL;
	NTSTATUS status;
	WDF_PNPPOWER_EVENT_CALLBACKS pnpPowerCallbacks;
	PBTHPS3_SERVER_CONTEXT pSrvCtx = NULL;
	PDMFDEVICE_INIT dmfDeviceInit = NULL;
	DMF_EVENT_CALLBACKS dmfEventCallbacks;

	PAGED_CODE();


	FuncEntry(TRACE_DEVICE);

	WdfDeviceInitSetDeviceType(DeviceInit, FILE_DEVICE_BUS_EXTENDER);

	dmfDeviceInit = DMF_DmfDeviceInitAllocate(DeviceInit);

	if (dmfDeviceInit == NULL)
	{
		TraceEvents(
			TRACE_LEVEL_ERROR,
			TRACE_DEVICE,
			"DMF_DmfDeviceInitAllocate failed"
		);

		status = STATUS_INSUFFICIENT_RESOURCES;

		goto exitFailure;
	}

	DMF_DmfDeviceInitHookFileObjectConfig(dmfDeviceInit, NULL);
	DMF_DmfDeviceInitHookPowerPolicyEventCallbacks(dmfDeviceInit, NULL);

	//
	// Configure PNP/power callbacks
	//
	WDF_PNPPOWER_EVENT_CALLBACKS_INIT(&pnpPowerCallbacks);
	pnpPowerCallbacks.EvtDeviceSelfManagedIoInit = BthPS3_EvtWdfDeviceSelfManagedIoInit;
	pnpPowerCallbacks.EvtDeviceSelfManagedIoCleanup = BthPS3_EvtWdfDeviceSelfManagedIoCleanup;

	DMF_DmfDeviceInitHookPnpPowerEventCallbacks(dmfDeviceInit, &pnpPowerCallbacks);

	WdfDeviceInitSetPnpPowerEventCallbacks(
		DeviceInit,
		&pnpPowerCallbacks
	);

	WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&attributes, BTHPS3_SERVER_CONTEXT);

	do
	{
		if (!NT_SUCCESS(status = WdfDeviceCreate(&DeviceInit, &attributes, &device)))
		{
			TraceError(
				TRACE_DEVICE,
				"WdfDeviceCreate failed with status %!STATUS!",
				status
			);
			break;
		}

		pSrvCtx = GetServerDeviceContext(device);

		if (!NT_SUCCESS(status = BthPS3_ServerContextInit(pSrvCtx, device)))
		{
			TraceError(
				TRACE_DEVICE,
				"Initialization of context failed with status %!STATUS!",
				status
			);
			break;
		}

		//
		// Query for interfaces and pre-allocate BRBs
		//

		if (!NT_SUCCESS(status = BthPS3_Initialize(GetServerDeviceContext(device))))
		{
			break;
		}

		//
		// Search and open filter remote I/O target
		// 

		if (!NT_SUCCESS(status = BthPS3_OpenFilterIoTarget(device)))
		{
			break;
		}

		//
		// Allocate request object for async filter communication
		// 

		WDF_OBJECT_ATTRIBUTES_INIT(&attributes);
		attributes.ParentObject = pSrvCtx->PsmFilter.IoTarget;

		if (!NT_SUCCESS(status = WdfRequestCreate(
			&attributes,
			pSrvCtx->PsmFilter.IoTarget,
			&pSrvCtx->PsmFilter.AsyncRequest
		)))
		{
			TraceError(
				TRACE_DEVICE,
				"WdfRequestCreate failed with status %!STATUS!",
				status
			);
			break;
		}

		//
		// DMF Module initialization
		// 
		DMF_EVENT_CALLBACKS_INIT(&dmfEventCallbacks);
		dmfEventCallbacks.EvtDmfDeviceModulesAdd = DmfDeviceModulesAdd;
		DMF_DmfDeviceInitSetEventCallbacks(
			dmfDeviceInit,
			&dmfEventCallbacks
		);

		status = DMF_ModulesCreate(device, &dmfDeviceInit);

		if (!NT_SUCCESS(status))
		{
			TraceEvents(
				TRACE_LEVEL_ERROR,
				TRACE_DEVICE,
				"DMF_ModulesCreate failed with status %!STATUS!",
				status
			);
			break;
		}

	} while (FALSE);

exitFailure:

	if (dmfDeviceInit != NULL)
	{
		DMF_DmfDeviceInitFree(&dmfDeviceInit);
	}

	if (!NT_SUCCESS(status) && device != NULL)
	{
		WdfObjectDelete(device);
	}

	FuncExit(TRACE_DEVICE, "status=%!STATUS!", status);

	return status;
}

//
// Locate and attempt to open instance of BTHPS3PSM filter device
// 
NTSTATUS BthPS3_OpenFilterIoTarget(
	_In_ WDFDEVICE Device
)
{
	NTSTATUS                    status = STATUS_UNSUCCESSFUL;
	WDF_OBJECT_ATTRIBUTES       ioTargetAttrib;
	PBTHPS3_SERVER_CONTEXT      pCtx;
	WDF_IO_TARGET_OPEN_PARAMS   openParams;

	DECLARE_CONST_UNICODE_STRING(symbolicLink, BTHPS3PSM_SYMBOLIC_NAME_STRING);

	PAGED_CODE();

	FuncEntry(TRACE_DEVICE);

	pCtx = GetServerDeviceContext(Device);

	WDF_OBJECT_ATTRIBUTES_INIT(&ioTargetAttrib);

	do
	{
		if (!NT_SUCCESS(status = WdfIoTargetCreate(
			Device,
			&ioTargetAttrib,
			&pCtx->PsmFilter.IoTarget
		)))
		{
			TraceError(
				TRACE_DEVICE,
				"WdfIoTargetCreate failed with status %!STATUS!",
				status
			);
			break;
		}

		WDF_IO_TARGET_OPEN_PARAMS_INIT_OPEN_BY_NAME(
			&openParams,
			&symbolicLink,
			STANDARD_RIGHTS_ALL
		);

		if (!NT_SUCCESS(status = WdfIoTargetOpen(
			pCtx->PsmFilter.IoTarget,
			&openParams
		)))
		{
			TraceError(
				TRACE_DEVICE,
				"WdfIoTargetOpen failed with status %!STATUS!",
				status
			);
			WdfObjectDelete(pCtx->PsmFilter.IoTarget);
			break;
		}

	} while (FALSE);

	FuncExit(TRACE_DEVICE, "status=%!STATUS!", status);

	return status;
}

//
// Timed auto-reset of filter driver
// 
void BthPS3_EnablePatchEvtWdfTimer(
	WDFTIMER Timer
)
{
	NTSTATUS status;
	PBTHPS3_SERVER_CONTEXT devCtx = GetServerDeviceContext(WdfTimerGetParentObject(Timer));

	TraceVerbose(TRACE_DEVICE,
		"Requesting filter to enable patch"
	);

	if (!NT_SUCCESS(status = BthPS3PSM_EnablePatchAsync(
		devCtx->PsmFilter.IoTarget,
		0 // TODO: read from registry?
	)))
	{
		TraceVerbose(TRACE_DEVICE,
			"BthPS3PSM_EnablePatchAsync failed with status %!STATUS!",
			status
		);
	}
}

//
// Gets invoked on device power-up
// 
_Use_decl_annotations_
NTSTATUS
BthPS3_EvtWdfDeviceSelfManagedIoInit(
	WDFDEVICE Device
)
{
	NTSTATUS status;
	PBTHPS3_SERVER_CONTEXT devCtx = GetServerDeviceContext(Device);

	FuncEntry(TRACE_DEVICE);

	do
	{
		if (!NT_SUCCESS(status = BthPS3_RetrieveLocalInfo(&devCtx->Header)))
		{
			break;
		}

		if (!NT_SUCCESS(status = BthPS3_RegisterPSM(devCtx)))
		{
			break;
		}

		if (!NT_SUCCESS(status = BthPS3_RegisterL2CAPServer(devCtx)))
		{
			break;
		}

		//
		// Attempt to enable, but ignore failure
		//
		if (devCtx->Settings.AutoEnableFilter)
		{
			(void)BthPS3PSM_EnablePatchSync(
				devCtx->PsmFilter.IoTarget,
				0
			);
		}

	} while (FALSE);

	FuncExit(TRACE_DEVICE, "status=%!STATUS!", status);

	return status;
}

//
// Gets invoked on device shutdown
// 
_Use_decl_annotations_
VOID
BthPS3_EvtWdfDeviceSelfManagedIoCleanup(
	WDFDEVICE Device
)
{
	PBTHPS3_SERVER_CONTEXT devCtx = GetServerDeviceContext(Device);
	WDFOBJECT currentItem;
	PBTHPS3_CLIENT_CONNECTION connection = NULL;

	PAGED_CODE();

	FuncEntry(TRACE_DEVICE);

	if (devCtx->PsmFilter.IoTarget != NULL)
	{
		WdfIoTargetClose(devCtx->PsmFilter.IoTarget);
		WdfObjectDelete(devCtx->PsmFilter.IoTarget);
	}

	if (NULL != devCtx->L2CAPServerHandle)
	{
		BthPS3_UnregisterL2CAPServer(devCtx);
	}

	if (0 != devCtx->PsmHidControl)
	{
		BthPS3_UnregisterPSM(devCtx);
	}

	//
	// Drop children
	// 
	// At this stage nobody is updating the connection list so no locking required
	// 
	while ((currentItem = WdfCollectionGetFirstItem(devCtx->Header.ClientConnections)) != NULL)
	{
		WdfCollectionRemoveItem(devCtx->Header.ClientConnections, 0);
		connection = GetClientConnection(currentItem);

		//
		// Disconnect HID Interrupt Channel first
		// 
		L2CAP_PS3_RemoteDisconnect(
			&devCtx->Header,
			connection->RemoteAddress,
			&connection->HidInterruptChannel
		);

		//
		// Wait until BTHPORT.SYS has completely dropped the connection
		// 
		KeWaitForSingleObject(
			&connection->HidInterruptChannel.DisconnectEvent,
			Executive,
			KernelMode,
			FALSE,
			NULL
		);

		//
		// Disconnect HID Control Channel last
		// 
		L2CAP_PS3_RemoteDisconnect(
			&devCtx->Header,
			connection->RemoteAddress,
			&connection->HidControlChannel
		);

		//
		// Wait until BTHPORT.SYS has completely dropped the connection
		// 
		KeWaitForSingleObject(
			&connection->HidControlChannel.DisconnectEvent,
			Executive,
			KernelMode,
			FALSE,
			NULL
		);

		//
		// Invokes freeing memory
		// 
		WdfObjectDelete(currentItem);
	}

	FuncExitNoReturn(TRACE_DEVICE);
}

//
// Initializes DMF modules
// 
_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
DmfDeviceModulesAdd(
	_In_ WDFDEVICE Device,
	_In_ PDMFMODULE_INIT DmfModuleInit
)
{
	FuncEntry(TRACE_DEVICE);

	DMF_MODULE_ATTRIBUTES moduleAttributes;
	DMF_CONFIG_Pdo moduleConfigPdo;

	const PBTHPS3_SERVER_CONTEXT pSrvCtx = GetServerDeviceContext(Device);

	//
	// PDO module
	// 

	DMF_CONFIG_Pdo_AND_ATTRIBUTES_INIT(
		&moduleConfigPdo,
		&moduleAttributes
	);

	moduleConfigPdo.DeviceLocation = L"Nefarius Bluetooth PS Enumerator";
	moduleConfigPdo.InstanceIdFormatString = L"BTHPS3_DEVICE_%02d";
	// Do not create any PDOs during Module create.
	// PDOs will be created dynamically through Module Method.
	//
	moduleConfigPdo.PdoRecordCount = 0;
	moduleConfigPdo.PdoRecords = NULL;
	moduleConfigPdo.EvtPdoPnpCapabilities = NULL;
	moduleConfigPdo.EvtPdoPowerCapabilities = NULL;

	DMF_DmfModuleAdd(
		DmfModuleInit,
		&moduleAttributes,
		WDF_NO_OBJECT_ATTRIBUTES,
		&pSrvCtx->Header.DmfModulePdo
	);

	FuncExitNoReturn(TRACE_DEVICE);
}
