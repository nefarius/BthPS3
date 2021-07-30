/**********************************************************************************
 *                                                                                *
 * BthPS3 - Windows kernel-mode Bluetooth profile and bus driver                  *
 *                                                                                *
 * BSD 3-Clause License                                                           *
 *                                                                                *
 * Copyright (c) 2018-2021, Nefarius Software Solutions e.U.                      *
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
#include <limits.h>
#include "buslogic.tmh"

#ifdef ALLOC_PRAGMA
#pragma alloc_text (PAGE, BthPS3_EvtWdfChildListCreateDevice)
#endif


 //
 // Spawn new child device (PDO)
 // 
_Use_decl_annotations_
NTSTATUS
BthPS3_EvtWdfChildListCreateDevice(
	WDFCHILDLIST ChildList,
	PWDF_CHILD_IDENTIFICATION_DESCRIPTION_HEADER IdentificationDescription,
	PWDFDEVICE_INIT ChildInit
)
{
	NTSTATUS                                status = STATUS_UNSUCCESSFUL;
	PPDO_IDENTIFICATION_DESCRIPTION         pDesc;
	UNICODE_STRING                          guidString;
	WDFDEVICE                               hChild = NULL;
	WDF_IO_QUEUE_CONFIG                     defaultQueueCfg;
	WDFQUEUE                                defaultQueue;
	WDF_OBJECT_ATTRIBUTES                   attributes;
	PBTHPS3_PDO_DEVICE_CONTEXT              pdoCtx = NULL;
	WDF_DEVICE_PNP_CAPABILITIES             pnpCaps;
	WDF_DEVICE_POWER_CAPABILITIES			powerCaps;
	WDF_DEVICE_POWER_POLICY_IDLE_SETTINGS   idleSettings;
	WDF_PNPPOWER_EVENT_CALLBACKS            pnpPowerCallbacks;
	WDFKEY                                  hKey = NULL;
	ULONG                                   rawPdo = 0;
	ULONG                                   hidePdo = 0;
	ULONG                                   adminOnlyPdo = 0;
	ULONG                                   exclusivePdo = 1;
	ULONG                                   idleTimeout = 10000; // 10 secs idle timeout

	DECLARE_UNICODE_STRING_SIZE(deviceId, MAX_DEVICE_ID_LEN);
	DECLARE_UNICODE_STRING_SIZE(hardwareId, MAX_DEVICE_ID_LEN);
	DECLARE_UNICODE_STRING_SIZE(instanceId, sizeof(INT32));

	DECLARE_CONST_UNICODE_STRING(rawPdoValue, BTHPS3_REG_VALUE_RAW_PDO);
	DECLARE_CONST_UNICODE_STRING(hidePdoValue, BTHPS3_REG_VALUE_HIDE_PDO);
	DECLARE_CONST_UNICODE_STRING(adminOnlyPdoValue, BTHPS3_REG_VALUE_ADMIN_ONLY_PDO);
	DECLARE_CONST_UNICODE_STRING(exclusivePdoValue, BTHPS3_REG_VALUE_EXCLUSIVE_PDO);
	DECLARE_CONST_UNICODE_STRING(idleTimeoutValue, BTHPS3_REG_VALUE_CHILD_IDLE_TIMEOUT);

	UNREFERENCED_PARAMETER(ChildList);

	PAGED_CODE();


	FuncEntry(TRACE_BUSLOGIC);

	pDesc = CONTAINING_RECORD(
		IdentificationDescription,
		PDO_IDENTIFICATION_DESCRIPTION,
		Header);
	
	//
	// Open
	//   HKEY_LOCAL_MACHINE\SYSTEM\CurrentControlSet\Services\BthPS3\Parameters
	// key
	// 
	status = WdfDriverOpenParametersRegistryKey(
		WdfGetDriver(),
		STANDARD_RIGHTS_ALL,
		WDF_NO_OBJECT_ATTRIBUTES,
		&hKey
	);

	//
	// On success, read configuration values
	// 
	if (NT_SUCCESS(status))
	{
		//
		// Don't care, if it fails, keep default value
		// 
		(void)WdfRegistryQueryULong(
			hKey,
			&rawPdoValue,
			&rawPdo
		);

		//
		// Don't care, if it fails, keep default value
		// 
		(void)WdfRegistryQueryULong(
			hKey,
			&hidePdoValue,
			&hidePdo
		);

		//
		// Don't care, if it fails, keep default value
		// 
		(void)WdfRegistryQueryULong(
			hKey,
			&adminOnlyPdoValue,
			&adminOnlyPdo
		);

		//
		// Don't care, if it fails, keep default value
		// 
		(void)WdfRegistryQueryULong(
			hKey,
			&exclusivePdoValue,
			&exclusivePdo
		);

		//
		// Don't care, if it fails, keep default value
		// 
		(void)WdfRegistryQueryULong(
			hKey,
			&idleTimeoutValue,
			&idleTimeout
		);

		WdfRegistryClose(hKey);
	}

	//
	// PDO features
	// 
	WdfDeviceInitSetDeviceType(ChildInit, FILE_DEVICE_BUS_EXTENDER);

	//
	// Only one instance (either function driver or user-land application)
	// may talk to this PDO at the same time to avoid splitting traffic.
	// 
	WdfDeviceInitSetExclusive(ChildInit, (BOOLEAN)exclusivePdo);

	//
	// Parent FDO will handle IRP_MJ_INTERNAL_DEVICE_CONTROL
	// 
	WdfPdoInitAllowForwardingRequestToParent(ChildInit);

	//
	// Adjust properties depending on device type
	// 
	switch (pDesc->ClientConnection->DeviceType)
	{
	case DS_DEVICE_TYPE_SIXAXIS:
		status = RtlStringFromGUID(&GUID_BUSENUM_BTHPS3_SIXAXIS,
			&guidString
		);
		break;
	case DS_DEVICE_TYPE_NAVIGATION:
		status = RtlStringFromGUID(&GUID_BUSENUM_BTHPS3_NAVIGATION,
			&guidString
		);
		break;
	case DS_DEVICE_TYPE_MOTION:
		status = RtlStringFromGUID(&GUID_BUSENUM_BTHPS3_MOTION,
			&guidString
		);
		break;
	case DS_DEVICE_TYPE_WIRELESS:
		status = RtlStringFromGUID(&GUID_BUSENUM_BTHPS3_WIRELESS,
			&guidString
		);
		break;
	default:
		// Doesn't happen
		return status;
	}

	if (!NT_SUCCESS(status))
	{
		TraceError(
			TRACE_BUSLOGIC,
			"RtlStringFromGUID failed with status %!STATUS!",
			status
		);
		return status;
	}

	do {

#pragma region Raw PDO properties

		if (rawPdo)
		{
			//
			// Assign RAW PDO Device Class GUID depending on device type
			// 
			// In raw device mode, user-land applications can talk to our
			// PDO as well with no function driver attached.
			// 
			switch (pDesc->ClientConnection->DeviceType)
			{
			case DS_DEVICE_TYPE_SIXAXIS:
				status = WdfPdoInitAssignRawDevice(ChildInit,
					&GUID_DEVCLASS_BTHPS3_SIXAXIS
				);
				break;
			case DS_DEVICE_TYPE_NAVIGATION:
				status = WdfPdoInitAssignRawDevice(ChildInit,
					&GUID_DEVCLASS_BTHPS3_NAVIGATION
				);
				break;
			case DS_DEVICE_TYPE_MOTION:
				status = WdfPdoInitAssignRawDevice(ChildInit,
					&GUID_DEVCLASS_BTHPS3_MOTION
				);
				break;
			case DS_DEVICE_TYPE_WIRELESS:
				status = WdfPdoInitAssignRawDevice(ChildInit,
					&GUID_DEVCLASS_BTHPS3_WIRELESS
				);
				break;
			default:
				// Doesn't happen
				break;
			}

			if (!NT_SUCCESS(status))
			{
				TraceError(
					TRACE_BUSLOGIC,
					"WdfPdoInitAssignRawDevice failed with status %!STATUS!",
					status
				);
				break;
			}

			//
			// Let the world talk to us
			// 
			status = WdfDeviceInitAssignSDDLString(ChildInit,
				(adminOnlyPdo) ? &SDDL_DEVOBJ_SYS_ALL_ADM_ALL : // only elevated allowed
				&SDDL_DEVOBJ_SYS_ALL_ADM_RWX_WORLD_RWX_RES_RWX  // everyone is allowed
			);

			if (!NT_SUCCESS(status))
			{
				TraceError(
					TRACE_BUSLOGIC,
					"WdfDeviceInitAssignSDDLString failed with status %!STATUS!",
					status
				);
				break;
			}
		}

#pragma endregion

#pragma region Build DeviceID

		//
		// Adjust properties depending on device type
		// 
		switch (pDesc->ClientConnection->DeviceType)
		{
		case DS_DEVICE_TYPE_SIXAXIS:
			status = RtlUnicodeStringPrintf(
				&deviceId,
				L"%ws\\%wZ&DEV&VID_%04X&PID_%04X&%012llX",
				BthPS3BusEnumeratorName,
				guidString,
				BTHPS3_SIXAXIS_VID,
				BTHPS3_SIXAXIS_PID,
				pDesc->ClientConnection->RemoteAddress
			);
			break;
		case DS_DEVICE_TYPE_NAVIGATION:
			status = RtlUnicodeStringPrintf(
				&deviceId,
				L"%ws\\%wZ&DEV&VID_%04X&PID_%04X&%012llX",
				BthPS3BusEnumeratorName,
				guidString,
				BTHPS3_NAVIGATION_VID,
				BTHPS3_NAVIGATION_PID,
				pDesc->ClientConnection->RemoteAddress
			);
			break;
		case DS_DEVICE_TYPE_MOTION:
			status = RtlUnicodeStringPrintf(
				&deviceId,
				L"%ws\\%wZ&DEV&VID_%04X&PID_%04X&%012llX",
				BthPS3BusEnumeratorName,
				guidString,
				BTHPS3_MOTION_VID,
				BTHPS3_MOTION_PID,
				pDesc->ClientConnection->RemoteAddress
			);
			break;
		case DS_DEVICE_TYPE_WIRELESS:
			status = RtlUnicodeStringPrintf(
				&deviceId,
				L"%ws\\%wZ&DEV&VID_%04X&PID_%04X&%012llX",
				BthPS3BusEnumeratorName,
				guidString,
				BTHPS3_WIRELESS_VID,
				BTHPS3_WIRELESS_PID,
				pDesc->ClientConnection->RemoteAddress
			);
			break;
		default:
			// Doesn't happen
			return status;
		}
				
		if (!NT_SUCCESS(status)) {
			TraceError(
				TRACE_BUSLOGIC,
				"RtlUnicodeStringPrintf failed for deviceId with status %!STATUS!",
				status
			);
			break;
		}

		status = WdfPdoInitAssignDeviceID(ChildInit, &deviceId);
		if (!NT_SUCCESS(status)) {
			TraceError(
				TRACE_BUSLOGIC,
				"WdfPdoInitAssignDeviceID failed with status %!STATUS!",
				status);
			break;
		}

#pragma endregion

#pragma region Build HardwareID

		//
		// Adjust properties depending on device type
		// 
		switch (pDesc->ClientConnection->DeviceType)
		{
		case DS_DEVICE_TYPE_SIXAXIS:
			status = RtlUnicodeStringPrintf(
				&hardwareId,
				L"%ws\\%wZ&Dev&VID_%04X&PID_%04X",
				BthPS3BusEnumeratorName,
				guidString,
				BTHPS3_SIXAXIS_VID,
				BTHPS3_SIXAXIS_PID
			);
			break;
		case DS_DEVICE_TYPE_NAVIGATION:
			status = RtlUnicodeStringPrintf(
				&hardwareId,
				L"%ws\\%wZ&Dev&VID_%04X&PID_%04X",
				BthPS3BusEnumeratorName,
				guidString,
				BTHPS3_NAVIGATION_VID,
				BTHPS3_NAVIGATION_PID
			);
			break;
		case DS_DEVICE_TYPE_MOTION:
			status = RtlUnicodeStringPrintf(
				&hardwareId,
				L"%ws\\%wZ&Dev&VID_%04X&PID_%04X",
				BthPS3BusEnumeratorName,
				guidString,
				BTHPS3_MOTION_VID,
				BTHPS3_MOTION_PID
			);
			break;
		case DS_DEVICE_TYPE_WIRELESS:
			status = RtlUnicodeStringPrintf(
				&hardwareId,
				L"%ws\\%wZ&Dev&VID_%04X&PID_%04X",
				BthPS3BusEnumeratorName,
				guidString,
				BTHPS3_WIRELESS_VID,
				BTHPS3_WIRELESS_PID
			);
			break;
		default:
			// Doesn't happen
			return status;
		}
		
		if (!NT_SUCCESS(status)) {
			TraceError(
				TRACE_BUSLOGIC,
				"RtlUnicodeStringPrintf failed for hardwareId with status %!STATUS!",
				status
			);
			break;
		}

		status = WdfPdoInitAddHardwareID(ChildInit, &hardwareId);
		if (!NT_SUCCESS(status)) {
			TraceError(
				TRACE_BUSLOGIC,
				"WdfPdoInitAddHardwareID failed with status %!STATUS!",
				status);
			break;
		}

#pragma endregion

#pragma region Build InstanceID
		
		status = RtlIntegerToUnicodeString(
			0,
			16,
			&instanceId
		);
		if (!NT_SUCCESS(status))
		{
			TraceError(
				TRACE_BUSLOGIC,
				"RtlIntegerToUnicodeString failed for instanceId with status %!STATUS!",
				status
			);
			break;
		}

		status = WdfPdoInitAssignInstanceID(ChildInit, &instanceId);
		if (!NT_SUCCESS(status)) {
			TraceError(
				TRACE_BUSLOGIC,
				"WdfPdoInitAssignInstanceID failed with status %!STATUS!",
				status);
			break;
		}

#pragma endregion

#pragma region PNP/Power Callbacks

		WDF_PNPPOWER_EVENT_CALLBACKS_INIT(&pnpPowerCallbacks);
		pnpPowerCallbacks.EvtDeviceD0Exit = BthPS3_PDO_EvtWdfDeviceD0Exit;
		pnpPowerCallbacks.EvtDeviceSelfManagedIoInit = BthPS3_PDO_EvtWdfDeviceSelfManagedIoInit;

		WdfDeviceInitSetPnpPowerEventCallbacks(ChildInit, &pnpPowerCallbacks);

#pragma endregion 

#pragma region Child device creation

		if (!rawPdo)
		{
			WdfDeviceInitSetPowerPolicyOwnership(ChildInit, TRUE);
		}

		WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(
			&attributes,
			BTHPS3_PDO_DEVICE_CONTEXT
		);
		attributes.EvtCleanupCallback = BthPS3_PDO_EvtDeviceContextCleanup;

		status = WdfDeviceCreate(
			&ChildInit,
			&attributes,
			&hChild
		);
		if (!NT_SUCCESS(status)) {
			TraceError(
				TRACE_BUSLOGIC,
				"WdfDeviceCreate failed with status %!STATUS!",
				status);
			break;
		}

#pragma endregion

#pragma region Expose device interface

		if (rawPdo)
		{
			switch (pDesc->ClientConnection->DeviceType)
			{
			case DS_DEVICE_TYPE_SIXAXIS:
				status = WdfDeviceCreateDeviceInterface(hChild,
					&GUID_DEVINTERFACE_BTHPS3_SIXAXIS,
					NULL
				);
				break;
			case DS_DEVICE_TYPE_NAVIGATION:
				status = WdfDeviceCreateDeviceInterface(hChild,
					&GUID_DEVINTERFACE_BTHPS3_NAVIGATION,
					NULL
				);
				break;
			case DS_DEVICE_TYPE_MOTION:
				status = WdfDeviceCreateDeviceInterface(hChild,
					&GUID_DEVINTERFACE_BTHPS3_MOTION,
					NULL
				);
				break;
			case DS_DEVICE_TYPE_WIRELESS:
				status = WdfDeviceCreateDeviceInterface(hChild,
					&GUID_DEVINTERFACE_BTHPS3_WIRELESS,
					NULL
				);
				break;
			default:
				// Doesn't happen
				break;
			}

			if (!NT_SUCCESS(status))
			{
				TraceError(
					TRACE_BUSLOGIC,
					"WdfDeviceCreateDeviceInterface failed with status %!STATUS!",
					status
				);
				break;
			}
		}

#pragma endregion
				
#pragma region Fill device context

		//
		// This info is used to attach it to incoming requests 
		// later on so the bus driver knows for which remote
		// device the request was made for.
		// 

		pdoCtx = GetPdoDeviceContext(hChild);

		pdoCtx->ClientConnection = pDesc->ClientConnection;

		//
		// PDO relies on the connection object context so 
		// we increase the reference count to protect from
		// it getting freed too soon. See BthPS3_PDO_EvtDeviceContextCleanup
		// 
		WdfObjectReference(WdfObjectContextGetObject(pDesc->ClientConnection));

#pragma endregion

#pragma region PNP/Power Caps

		//
		// PNP Capabilities
		// 

		WDF_DEVICE_PNP_CAPABILITIES_INIT(&pnpCaps);
		pnpCaps.Removable = WdfTrue;
		pnpCaps.SurpriseRemovalOK = WdfTrue;
		pnpCaps.NoDisplayInUI = (hidePdo) ? WdfTrue : WdfFalse;

		WdfDeviceSetPnpCapabilities(hChild, &pnpCaps);

		//
		// Power capabilities
		// 

		WDF_DEVICE_POWER_CAPABILITIES_INIT(&powerCaps);
		powerCaps.DeviceD1 = WdfFalse;
		powerCaps.DeviceD2 = WdfTrue;
		powerCaps.WakeFromD2 = WdfTrue;
		powerCaps.DeviceState[PowerSystemWorking] = PowerDeviceD0;
		powerCaps.DeviceState[PowerSystemSleeping1] = PowerDeviceD2;
		powerCaps.DeviceState[PowerSystemSleeping2] = PowerDeviceD2;
		powerCaps.DeviceState[PowerSystemSleeping3] = PowerDeviceD2;
		powerCaps.DeviceState[PowerSystemHibernate] = PowerDeviceD2;
		powerCaps.DeviceState[PowerSystemShutdown] = PowerDeviceD3;

		WdfDeviceSetPowerCapabilities(hChild, &powerCaps);
		
		//
		// Idle settings
		// 

		WDF_DEVICE_POWER_POLICY_IDLE_SETTINGS_INIT(&idleSettings, IdleCannotWakeFromS0);
		idleSettings.IdleTimeout = idleTimeout;
		status = WdfDeviceAssignS0IdleSettings(hChild, &idleSettings);

		//
		// Catch special case more precisely 
		// 
		if (status == STATUS_INVALID_DEVICE_REQUEST)
		{
			TraceError(
				TRACE_BUSLOGIC,
				"No function driver attached and not in RAW mode, can't continue"
			);
			break;
		}

		if (!NT_SUCCESS(status))
		{
			TraceError(
				TRACE_BUSLOGIC,
				"WdfDeviceAssignS0IdleSettings failed with status %!STATUS!",
				status
			);
			break;
		}

#pragma endregion

#pragma region Default I/O Queue creation

		//
		// All of the heavy lifting is done by a function driver
		// which exchanges data via IRP_MJ_DEVICE_CONTROL
		// 
		WDF_IO_QUEUE_CONFIG_INIT_DEFAULT_QUEUE(&defaultQueueCfg, WdfIoQueueDispatchParallel);

		defaultQueueCfg.EvtIoStop = BthPS3_EvtIoStop;
		defaultQueueCfg.EvtIoDeviceControl = BthPS3_PDO_EvtWdfIoQueueIoDeviceControl;

		status = WdfIoQueueCreate(
			hChild,
			&defaultQueueCfg,
			WDF_NO_OBJECT_ATTRIBUTES,
			&defaultQueue
		);
		if (!NT_SUCCESS(status)) {
			TraceError(
				TRACE_BUSLOGIC,
				"WdfIoQueueCreate (Default) failed with status %!STATUS!",
				status);
			break;
		}

#pragma endregion

	}
	while (FALSE);

	RtlFreeUnicodeString(&guidString);

	FuncExit(TRACE_BUSLOGIC, "status=%!STATUS!", status);

	return status;
}

//
// Used to compare two bus children
// 
BOOLEAN BthPS3_PDO_EvtChildListIdentificationDescriptionCompare(
	WDFCHILDLIST DeviceList,
	PWDF_CHILD_IDENTIFICATION_DESCRIPTION_HEADER FirstIdentificationDescription,
	PWDF_CHILD_IDENTIFICATION_DESCRIPTION_HEADER SecondIdentificationDescription)
{
	PPDO_IDENTIFICATION_DESCRIPTION lhs, rhs;

	UNREFERENCED_PARAMETER(DeviceList);

	lhs = CONTAINING_RECORD(FirstIdentificationDescription,
		PDO_IDENTIFICATION_DESCRIPTION,
		Header);
	rhs = CONTAINING_RECORD(SecondIdentificationDescription,
		PDO_IDENTIFICATION_DESCRIPTION,
		Header);
	//
	// BTH_ADDR of remote device is used to distinguish between children
	// 
	return (lhs->ClientConnection->RemoteAddress ==
		rhs->ClientConnection->RemoteAddress) ? TRUE : FALSE;
}

//
// Triggered on entering I/O idle state
// 
NTSTATUS BthPS3_PDO_EvtWdfDeviceD0Exit(
	WDFDEVICE Device,
	WDF_POWER_DEVICE_STATE TargetState
)
{
	NTSTATUS                    status;
	WDFDEVICE                   parentDevice;
	WDFIOTARGET                 parentTarget;
	PBTHPS3_PDO_DEVICE_CONTEXT  pPdoDevCtx;
	WDFREQUEST					dcRequest;
	WDF_OBJECT_ATTRIBUTES		attributes;
	WDFMEMORY					payload;
	

	UNREFERENCED_PARAMETER(Device);
	UNREFERENCED_PARAMETER(TargetState);

	FuncEntry(TRACE_BUSLOGIC);

	pPdoDevCtx = GetPdoDeviceContext(Device);
	parentDevice = WdfPdoGetParent(Device);
	parentTarget = WdfDeviceGetIoTarget(parentDevice);

	TraceInformation(
		TRACE_BUSLOGIC,
		"Requesting device disconnect"
	);

	/*
	 * Low power conditions are not supported by the remote device, 
	 * if low power or idle was requested, instruct radio to disconnect.
	 */

	WDF_OBJECT_ATTRIBUTES_INIT(&attributes);
	attributes.ParentObject = Device;
	
	status = WdfRequestCreate(
		&attributes,
		parentTarget,
		&dcRequest
	);
	if (!NT_SUCCESS(status))
	{
		TraceError(
			TRACE_BUSLOGIC,
			"WdfRequestCreate failed with status %!STATUS!",
			status
		);
	}

	WDF_OBJECT_ATTRIBUTES_INIT(&attributes);
	attributes.ParentObject = dcRequest;

	status = WdfMemoryCreatePreallocated(
		&attributes,
		&pPdoDevCtx->ClientConnection->RemoteAddress,
		sizeof(BTH_ADDR),
		&payload
	);
	if (!NT_SUCCESS(status))
	{
		TraceError(
			TRACE_BUSLOGIC,
			"WdfMemoryCreatePreallocated failed with status %!STATUS!",
			status
		);
	}

	status = WdfIoTargetFormatRequestForIoctl(
		parentTarget,
		dcRequest,
		IOCTL_BTH_DISCONNECT_DEVICE,
		payload,
		NULL,
		NULL,
		NULL
	);
	if (!NT_SUCCESS(status))
	{
		TraceError(
			TRACE_BUSLOGIC,
			"WdfIoTargetFormatRequestForIoctl failed with status %!STATUS!",
			status
		);
	}

	WdfRequestSetCompletionRoutine(
		dcRequest,
		BthPS3_PDO_DisconnectRequestCompleted,
		NULL
	);

	//
	// Request parent to abandon us :'(
	// 
	if (WdfRequestSend(
		dcRequest,
		parentTarget,
		WDF_NO_SEND_OPTIONS
	) == FALSE)
	{
		status = WdfRequestGetStatus(dcRequest);

		if (status != STATUS_DEVICE_NOT_CONNECTED && !NT_SUCCESS(status))
		{
			TraceError(
				TRACE_BUSLOGIC,
				"WdfRequestSend failed with status %!STATUS!",
				status
			);
		}
		else
		{
			//
			// Override
			// 
			status = STATUS_SUCCESS;
		}
	}
	
	FuncExit(TRACE_BUSLOGIC, "status=%!STATUS!", status);

	return status;
}

//
// Gets called on PDO removal
// 
VOID
BthPS3_PDO_EvtDeviceContextCleanup(
	IN WDFOBJECT Device
)
{
	PBTHPS3_PDO_DEVICE_CONTEXT pDevCtx;

	FuncEntry(TRACE_BUSLOGIC);

	pDevCtx = GetPdoDeviceContext(Device);

	//
	// At this point it's safe (for us, the PDO) to dispose the connection object
	// 
	WdfObjectDereference(WdfObjectContextGetObject(pDevCtx->ClientConnection));

	FuncExitNoReturn(TRACE_BUSLOGIC);
}

//
// Handle IRP_MJ_DEVICE_CONTROL sent to PDO
// 
void BthPS3_PDO_EvtWdfIoQueueIoDeviceControl(
	WDFQUEUE Queue,
	WDFREQUEST Request,
	size_t OutputBufferLength,
	size_t InputBufferLength,
	ULONG IoControlCode
)
{
	NTSTATUS                    status = STATUS_UNSUCCESSFUL;
	WDFDEVICE                   child = NULL;
	WDFDEVICE                   parent = NULL;
	PBTHPS3_PDO_DEVICE_CONTEXT  childCtx = NULL;
	PBTHPS3_CLIENT_CONNECTION   clientConnection = NULL;
	PVOID                       buffer = NULL;
	size_t                      bufferLength = 0;
	WDF_REQUEST_FORWARD_OPTIONS forwardOptions;


	FuncEntry(TRACE_BUSLOGIC);

	child = WdfIoQueueGetDevice(Queue);
	parent = WdfPdoGetParent(child);
	childCtx = GetPdoDeviceContext(child);
	clientConnection = childCtx->ClientConnection;

	switch (IoControlCode)
	{
#pragma region IOCTL_BTHPS3_HID_CONTROL_READ

	case IOCTL_BTHPS3_HID_CONTROL_READ:

		TraceVerbose(
			TRACE_BUSLOGIC,
			">> IOCTL_BTHPS3_HID_CONTROL_READ"
		);

		status = WdfRequestRetrieveOutputBuffer(
			Request,
			OutputBufferLength,
			&buffer,
			&bufferLength
		);

		if (!NT_SUCCESS(status)) {
			TraceError(
				TRACE_BUSLOGIC,
				"WdfRequestRetrieveOutputBuffer failed with status %!STATUS!",
				status
			);
			break;
		}

		TraceVerbose(
			TRACE_BUSLOGIC,
			"bufferLength: %d",
			(ULONG)bufferLength
		);

		status = L2CAP_PS3_ReadControlTransferAsync(
			clientConnection,
			Request,
			buffer,
			bufferLength,
			L2CAP_PS3_AsyncReadControlTransferCompleted
		);

		if (!NT_SUCCESS(status)) {
			TraceError(
				TRACE_BUSLOGIC,
				"L2CAP_PS3_ReadControlTransferAsync failed with status %!STATUS!",
				status
			);
		}
		else
		{
			status = STATUS_PENDING;
		}

		break;

#pragma endregion

#pragma region IOCTL_BTHPS3_HID_CONTROL_WRITE

	case IOCTL_BTHPS3_HID_CONTROL_WRITE:

		TraceVerbose(
			TRACE_BUSLOGIC,
			">> IOCTL_BTHPS3_HID_CONTROL_WRITE"
		);

		status = WdfRequestRetrieveInputBuffer(
			Request,
			InputBufferLength,
			&buffer,
			&bufferLength
		);

		if (!NT_SUCCESS(status)) {
			TraceError(
				TRACE_BUSLOGIC,
				"WdfRequestRetrieveInputBuffer failed with status %!STATUS!",
				status
			);
			break;
		}

		status = L2CAP_PS3_SendControlTransferAsync(
			clientConnection,
			Request,
			buffer,
			bufferLength,
			L2CAP_PS3_AsyncSendControlTransferCompleted
		);

		if (!NT_SUCCESS(status)) {
			TraceError(
				TRACE_BUSLOGIC,
				"L2CAP_PS3_SendControlTransferAsync failed with status %!STATUS!",
				status
			);
		}
		else
		{
			status = STATUS_PENDING;
		}

		break;

#pragma endregion

#pragma region IOCTL_BTHPS3_HID_INTERRUPT_READ

	case IOCTL_BTHPS3_HID_INTERRUPT_READ:

		TraceVerbose(
			TRACE_BUSLOGIC,
			">> IOCTL_BTHPS3_HID_INTERRUPT_READ"
		);

		status = WdfRequestRetrieveOutputBuffer(
			Request,
			OutputBufferLength,
			&buffer,
			&bufferLength
		);

		if (!NT_SUCCESS(status)) {
			TraceError(
				TRACE_BUSLOGIC,
				"WdfRequestRetrieveOutputBuffer failed with status %!STATUS!",
				status
			);
			break;
		}

		TraceVerbose(
			TRACE_BUSLOGIC,
			"bufferLength: %d",
			(ULONG)bufferLength
		);

		status = L2CAP_PS3_ReadInterruptTransferAsync(
			clientConnection,
			Request,
			buffer,
			bufferLength,
			L2CAP_PS3_AsyncReadInterruptTransferCompleted
		);

		if (!NT_SUCCESS(status)) {
			TraceError(
				TRACE_BUSLOGIC,
				"L2CAP_PS3_ReadInterruptTransferAsync failed with status %!STATUS!",
				status
			);
		}
		else
		{
			status = STATUS_PENDING;
		}

		break;

#pragma endregion

#pragma region IOCTL_BTHPS3_HID_INTERRUPT_WRITE

	case IOCTL_BTHPS3_HID_INTERRUPT_WRITE:

		TraceVerbose(
			TRACE_BUSLOGIC,
			">> IOCTL_BTHPS3_HID_INTERRUPT_WRITE"
		);

		status = WdfRequestRetrieveInputBuffer(
			Request,
			InputBufferLength,
			&buffer,
			&bufferLength
		);

		if (!NT_SUCCESS(status)) {
			TraceError(
				TRACE_BUSLOGIC,
				"WdfRequestRetrieveInputBuffer failed with status %!STATUS!",
				status
			);
			break;
		}

		status = L2CAP_PS3_SendInterruptTransferAsync(
			clientConnection,
			Request,
			buffer,
			bufferLength,
			L2CAP_PS3_AsyncSendInterruptTransferCompleted
		);

		if (!NT_SUCCESS(status)) {
			TraceError(
				TRACE_BUSLOGIC,
				"L2CAP_PS3_SendInterruptTransferAsync failed with status %!STATUS!",
				status
			);
		}
		else
		{
			status = STATUS_PENDING;
		}

		break;

#pragma endregion

	default:
		TraceVerbose(
			TRACE_BUSLOGIC,
			"Unknown IoControlCode received: 0x%X",
			IoControlCode
		);

		WDF_REQUEST_FORWARD_OPTIONS_INIT(&forwardOptions);
		forwardOptions.Flags = WDF_REQUEST_FORWARD_OPTION_SEND_AND_FORGET;

		status = WdfRequestForwardToParentDeviceIoQueue(
			Request,
			WdfDeviceGetDefaultQueue(parent),
			&forwardOptions
		);

		if (!NT_SUCCESS(status))
		{
			TraceError(
				TRACE_BUSLOGIC,
				"WdfRequestForwardToParentDeviceIoQueue failed with status %!STATUS!",
				status
			);
			WdfRequestComplete(Request, status);
		}

		return;
	}

	if (status != STATUS_PENDING) {
		WdfRequestComplete(Request, status);
	}

	FuncExit(TRACE_BUSLOGIC, "status=%!STATUS!", status);
}

//
// Disconnect request got completed
// 
void BthPS3_PDO_DisconnectRequestCompleted(
	_In_ WDFREQUEST Request,
	_In_ WDFIOTARGET Target,
	_In_ PWDF_REQUEST_COMPLETION_PARAMS Params,
	_In_ WDFCONTEXT Context
)
{
	UNREFERENCED_PARAMETER(Target);
	UNREFERENCED_PARAMETER(Context);

	FuncEntryArguments(
		TRACE_BUSLOGIC, 
		"status=%!STATUS!",
		Params->IoStatus.Status
	);
	
	WdfObjectDelete(Request);

	FuncExitNoReturn(TRACE_BUSLOGIC);
}

//
// Set device properties
// 
NTSTATUS BthPS3_PDO_EvtWdfDeviceSelfManagedIoInit(
	WDFDEVICE Device
)
{
	PBTHPS3_PDO_DEVICE_CONTEXT pCtx = GetPdoDeviceContext(Device);
	WCHAR deviceAddress[13];
	PWSTR manufacturer = L"Nefarius Software Solutions e.U.";
	LARGE_INTEGER lastConnectionTime;

	FuncEntry(TRACE_BUSLOGIC);
	
	KeQuerySystemTimePrecise(&lastConnectionTime);

	//
	// Common
	// 

	if (NT_SUCCESS(RtlStringCbPrintfW(
		deviceAddress,
		ARRAYSIZE(deviceAddress) * sizeof(WCHAR),
		L"%012llX",
		pCtx->ClientConnection->RemoteAddress
	)))
	{
		(void)BthPS3_AssignDeviceProperty(
			Device,
			&DEVPKEY_Bluetooth_DeviceAddress,
			DEVPROP_TYPE_STRING,
			ARRAYSIZE(deviceAddress) * sizeof(WCHAR),
			deviceAddress
		);
	}

	(void)BthPS3_AssignDeviceProperty(
		Device,
		&DEVPKEY_BluetoothRadio_Address,
		DEVPROP_TYPE_UINT64,
		sizeof(UINT64),
		&pCtx->ClientConnection->DevCtxHdr->LocalBthAddr
	);

	(void)BthPS3_AssignDeviceProperty(
		Device,
		&DEVPKEY_Device_FriendlyName,
		DEVPROP_TYPE_STRING,
		pCtx->ClientConnection->RemoteName.Length + sizeof(L'\0'),
		pCtx->ClientConnection->RemoteName.Buffer
	);
	
	(void)BthPS3_AssignDeviceProperty(
		Device,
		&DEVPKEY_Bluetooth_DeviceManufacturer,
		DEVPROP_TYPE_STRING,
		sizeof(manufacturer),
		manufacturer
	);

	(void)BthPS3_AssignDeviceProperty(
		Device,
		&DEVPKEY_Bluetooth_LastConnectedTime,
		DEVPROP_TYPE_FILETIME,
		sizeof(LARGE_INTEGER),
		&lastConnectionTime
	);
	
	switch (pCtx->ClientConnection->DeviceType)
	{
	case DS_DEVICE_TYPE_SIXAXIS:

		(void)BthPS3_AssignDeviceProperty(
			Device,
			&DEVPKEY_Bluetooth_DeviceVID,
			DEVPROP_TYPE_UINT16,
			sizeof(USHORT),
			(PUSHORT)&BTHPS3_SIXAXIS_VID
		);
		(void)BthPS3_AssignDeviceProperty(
			Device,
			&DEVPKEY_Bluetooth_DevicePID,
			DEVPROP_TYPE_UINT16,
			sizeof(USHORT),
			(PVOID)&BTHPS3_SIXAXIS_PID
		);

		break;
	case DS_DEVICE_TYPE_NAVIGATION:

		(void)BthPS3_AssignDeviceProperty(
			Device,
			&DEVPKEY_Bluetooth_DeviceVID,
			DEVPROP_TYPE_UINT16,
			sizeof(USHORT),
			(PUSHORT)&BTHPS3_NAVIGATION_VID
		);
		(void)BthPS3_AssignDeviceProperty(
			Device,
			&DEVPKEY_Bluetooth_DevicePID,
			DEVPROP_TYPE_UINT16,
			sizeof(USHORT),
			(PVOID)&BTHPS3_NAVIGATION_PID
		);
		
		break;
	case DS_DEVICE_TYPE_MOTION:

		(void)BthPS3_AssignDeviceProperty(
			Device,
			&DEVPKEY_Bluetooth_DeviceVID,
			DEVPROP_TYPE_UINT16,
			sizeof(USHORT),
			(PUSHORT)&BTHPS3_MOTION_VID
		);
		(void)BthPS3_AssignDeviceProperty(
			Device,
			&DEVPKEY_Bluetooth_DevicePID,
			DEVPROP_TYPE_UINT16,
			sizeof(USHORT),
			(PVOID)&BTHPS3_MOTION_PID
		);

		break;
	case DS_DEVICE_TYPE_WIRELESS:

		(void)BthPS3_AssignDeviceProperty(
			Device,
			&DEVPKEY_Bluetooth_DeviceVID,
			DEVPROP_TYPE_UINT16,
			sizeof(USHORT),
			(PUSHORT)&BTHPS3_WIRELESS_VID
		);
		(void)BthPS3_AssignDeviceProperty(
			Device,
			&DEVPKEY_Bluetooth_DevicePID,
			DEVPROP_TYPE_UINT16,
			sizeof(USHORT),
			(PVOID)&BTHPS3_WIRELESS_PID
		);
		
		break;
	default:
		// Doesn't happen
		break;
	}

	FuncExitNoReturn(TRACE_BUSLOGIC);
	
	return STATUS_SUCCESS;
}

NTSTATUS BthPS3_AssignDeviceProperty(
	WDFDEVICE Device, 
	const DEVPROPKEY* PropertyKey, 
	DEVPROPTYPE Type, 
	ULONG Size, 
	PVOID Data
)
{
#if KMDF_VERSION_MAJOR == 1 && KMDF_VERSION_MINOR >= 13
	
	WDF_DEVICE_PROPERTY_DATA propertyData;

	WDF_DEVICE_PROPERTY_DATA_INIT(&propertyData, PropertyKey);
	propertyData.Flags |= PLUGPLAY_PROPERTY_PERSISTENT;
	propertyData.Lcid = LOCALE_NEUTRAL;

	return WdfDeviceAssignProperty(
		Device,
		&propertyData,
		Type,
		Size,
		Data
	);

#else

	//
	// KMDF version too low to support this
	// 
	
	UNREFERENCED_PARAMETER(Device);
	UNREFERENCED_PARAMETER(PropertyKey);
	UNREFERENCED_PARAMETER(Type);
	UNREFERENCED_PARAMETER(Size);
	UNREFERENCED_PARAMETER(Data);

	TraceVerbose(
		TRACE_BUSLOGIC,
		"Assigning device properties requires KMDF 1.13 (Windows 8.x) or later"
	);
	
	return STATUS_NOT_SUPPORTED;
	
#endif
}
