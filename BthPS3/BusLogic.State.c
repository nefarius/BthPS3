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
#include "BusLogic.State.tmh"


 //
 // Called before PDO creation
 // 
_IRQL_requires_max_(PASSIVE_LEVEL)
_IRQL_requires_same_
NTSTATUS
BthPS3_PDO_EvtPreCreate(
	_In_ DMFMODULE DmfModule,
	_In_ PWDFDEVICE_INIT DeviceInit,
	_In_ PDMFDEVICE_INIT DmfDeviceInit,
	_In_ PDO_RECORD* PdoRecord
)
{
	FuncEntry(TRACE_BUSLOGIC);

	NTSTATUS status = STATUS_SUCCESS;

	UNREFERENCED_PARAMETER(DmfModule);
	UNREFERENCED_PARAMETER(DmfDeviceInit);
	UNREFERENCED_PARAMETER(PdoRecord);

	WdfDeviceInitSetDeviceType(DeviceInit, FILE_DEVICE_BUS_EXTENDER);

	//
	// Increases stack size required to forward HID requests as BRBs
	// 
	WdfPdoInitAllowForwardingRequestToParent(DeviceInit);

	UCHAR minorFunctionsToFilter[] =
	{
		IRP_MN_SET_POWER
	};

	//
	// Intercept IRP_MJ_POWER / IRP_MN_SET_POWER IRP
	// 
	if (!NT_SUCCESS(status = WdfDeviceInitAssignWdmIrpPreprocessCallback(
		DeviceInit,
		BthPS3_PDO_SetPowerIrpPreprocess,
		IRP_MJ_POWER,
		minorFunctionsToFilter,
		ARRAYSIZE(minorFunctionsToFilter)
	)))
	{
		TraceError(
			TRACE_BUSLOGIC,
			"WdfDeviceInitAssignWdmIrpPreprocessCallback failed with status %!STATUS!",
			status
		);
	}

	FuncExit(TRACE_BUSLOGIC, "status=%!STATUS!", status);

	return status;
}

//
// Called after PDO creation
// 
_IRQL_requires_max_(PASSIVE_LEVEL)
_IRQL_requires_same_
NTSTATUS
BthPS3_PDO_EvtPostCreate(
	_In_ DMFMODULE DmfModule,
	_In_ WDFDEVICE ChildDevice,
	_In_ PDMFDEVICE_INIT DmfDeviceInit,
	_In_ PDO_RECORD* PdoRecord
)
{
	FuncEntry(TRACE_BUSLOGIC);

	NTSTATUS status = STATUS_UNSUCCESSFUL;
	WDF_DEVICE_POWER_POLICY_IDLE_SETTINGS idleSettings;
	WDFKEY hKey = NULL;
	ULONG idleTimeout = 10000; // 10 secs idle timeout
	WDF_OBJECT_ATTRIBUTES attributes;
	WDF_IO_QUEUE_CONFIG queueCfg;

	DECLARE_CONST_UNICODE_STRING(idleTimeoutValue, BTHPS3_REG_VALUE_CHILD_IDLE_TIMEOUT);

	const PBTHPS3_PDO_CONTEXT pPdoCtx = GetPdoContext(ChildDevice);

	do
	{
		//
		// Create queues to dispatch HID Control & Interrupt requests
		// 

		WDF_OBJECT_ATTRIBUTES_INIT(&attributes);
		WDF_IO_QUEUE_CONFIG_INIT(&queueCfg, WdfIoQueueDispatchManual);
		queueCfg.PowerManaged = WdfTrue;

		if (!NT_SUCCESS(status = WdfIoQueueCreate(
			ChildDevice,
			&queueCfg,
			&attributes,
			&pPdoCtx->Queues.HidControlReadRequests
		)))
		{
			TraceError(
				TRACE_BUSLOGIC,
				"WdfIoQueueCreate (HidControlReadRequests) failed with status %!STATUS!",
				status
			);
			break;
		}

		if (!NT_SUCCESS(status = WdfIoQueueCreate(
			ChildDevice,
			&queueCfg,
			&attributes,
			&pPdoCtx->Queues.HidControlWriteRequests
		)))
		{
			TraceError(
				TRACE_BUSLOGIC,
				"WdfIoQueueCreate (HidControlReadRequests) failed with status %!STATUS!",
				status
			);
			break;
		}

		if (!NT_SUCCESS(status = WdfIoQueueCreate(
			ChildDevice,
			&queueCfg,
			&attributes,
			&pPdoCtx->Queues.HidInterruptReadRequests
		)))
		{
			TraceError(
				TRACE_BUSLOGIC,
				"WdfIoQueueCreate (HidControlReadRequests) failed with status %!STATUS!",
				status
			);
			break;
		}

		if (!NT_SUCCESS(status = WdfIoQueueCreate(
			ChildDevice,
			&queueCfg,
			&attributes,
			&pPdoCtx->Queues.HidInterruptWriteRequests
		)))
		{
			TraceError(
				TRACE_BUSLOGIC,
				"WdfIoQueueCreate (HidControlReadRequests) failed with status %!STATUS!",
				status
			);
			break;
		}

		//
		// Open
		//   HKEY_LOCAL_MACHINE\SYSTEM\CurrentControlSet\Services\BthPS3\Parameters
		// key
		// 
		if (!NT_SUCCESS(status = WdfDriverOpenParametersRegistryKey(
			WdfGetDriver(),
			STANDARD_RIGHTS_ALL,
			WDF_NO_OBJECT_ATTRIBUTES,
			&hKey
		)))
		{
			TraceError(
				TRACE_BUSLOGIC,
				"WdfDriverOpenParametersRegistryKey failed with status %!STATUS!",
				status
			);
			break;
		}

		//
		// Don't care, if it fails, keep default value
		// 
		(void)WdfRegistryQueryULong(
			hKey,
			&idleTimeoutValue,
			&idleTimeout
		);

		//
		// Idle settings
		// 

		WDF_DEVICE_POWER_POLICY_IDLE_SETTINGS_INIT(&idleSettings, IdleCannotWakeFromS0);
		idleSettings.IdleTimeout = idleTimeout;
		status = WdfDeviceAssignS0IdleSettings(ChildDevice, &idleSettings);

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

	} while (FALSE);

	if (hKey)
	{
		WdfRegistryClose(hKey);
	}

	UNREFERENCED_PARAMETER(DmfModule);
	UNREFERENCED_PARAMETER(DmfDeviceInit);
	UNREFERENCED_PARAMETER(PdoRecord);

	FuncExit(TRACE_BUSLOGIC, "status=%!STATUS!", status);

	return status;
}

//
// Intercepts IRP_MJ_POWER / IRP_MN_SET_POWER IRPs
// 
NTSTATUS
BthPS3_PDO_SetPowerIrpPreprocess(
	IN WDFDEVICE Device,
	IN OUT PIRP Irp
)
{
	const PIO_STACK_LOCATION pStack = IoGetCurrentIrpStackLocation(Irp);

	if (pStack->Parameters.Power.Type == DevicePowerState
		&& pStack->Parameters.Power.State.DeviceState == PowerDeviceD0)
	{
		IofCompleteRequest(Irp, IO_NO_INCREMENT);
		return STATUS_CONTINUE_COMPLETION;
	}

	IoSkipCurrentIrpStackLocation(Irp);
	return WdfDeviceWdmDispatchPreprocessedIrp(Device, Irp);
}
