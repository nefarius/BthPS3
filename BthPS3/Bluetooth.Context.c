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
#include "Bluetooth.Context.tmh"


//
// Initializes struct _BTHPS3_DEVICE_CONTEXT_HEADER members
// 
#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
BthPS3_DeviceContextHeaderInit(
	PBTHPS3_DEVICE_CONTEXT_HEADER Header,
	WDFDEVICE Device
)
{
	NTSTATUS status;
	WDF_OBJECT_ATTRIBUTES attributes;

	FuncEntry(TRACE_BTH);

	PAGED_CODE()

	Header->Device = Device;

	Header->IoTarget = WdfDeviceGetIoTarget(Device);

	//
	// Initialize request object
	//

	WDF_OBJECT_ATTRIBUTES_INIT(&attributes);
	attributes.ParentObject = Device;

	do
	{
		if (!NT_SUCCESS(status = WdfRequestCreate(
			&attributes,
			Header->IoTarget,
			&Header->HostInitRequest
		)))
		{
			TraceError(
				TRACE_BTH,
				"Failed to pre-allocate request in device context with status %!STATUS!",
				status
			);
			break;
		}

		if (!NT_SUCCESS(status = WdfSpinLockCreate(
			&attributes,
			&Header->ClientsLock
		)))
		{
			break;
		}

		if (!NT_SUCCESS(status = WdfCollectionCreate(
			&attributes,
			&Header->Clients
		)))
		{
			break;
		}

		if (!NT_SUCCESS(status = WdfSpinLockCreate(
			&attributes,
			&Header->SlotsSpinLock
		)))
		{
			break;
		}

	} while (FALSE);

	FuncExit(TRACE_BTH, "status=%!STATUS!", status);

	return status;
}
#pragma code_seg()

//
// Initializes struct _BTHPS3_SERVER_CONTEXT members
// 
#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
BthPS3_ServerContextInit(
	PBTHPS3_SERVER_CONTEXT Context,
	WDFDEVICE Device
)
{
	NTSTATUS status;
	WDF_OBJECT_ATTRIBUTES attributes;
	WDF_TIMER_CONFIG timerCfg;

	FuncEntry(TRACE_BTH);

	PAGED_CODE()

	do
	{
		//
		// Initialize crucial header struct first
		// 
		if (!NT_SUCCESS(status = BthPS3_DeviceContextHeaderInit(&Context->Header, Device)))
		{
			break;
		}

		WDF_OBJECT_ATTRIBUTES_INIT(&attributes);
		attributes.ParentObject = Device;

		WDF_TIMER_CONFIG_INIT(&timerCfg, BthPS3_EnablePatchEvtWdfTimer);

		if (!NT_SUCCESS(status = WdfTimerCreate(
			&timerCfg,
			&attributes,
			&Context->PsmFilter.AutoResetTimer
		)))
		{
			break;
		}

		WDF_OBJECT_ATTRIBUTES_INIT(&attributes);
		attributes.ParentObject = Device;

		if (!NT_SUCCESS(status = WdfCollectionCreate(
			&attributes,
			&Context->Settings.SIXAXISSupportedNames
		)))
		{
			break;
		}

		if (!NT_SUCCESS(status = WdfCollectionCreate(
			&attributes,
			&Context->Settings.NAVIGATIONSupportedNames
		)))
		{
			break;
		}

		if (!NT_SUCCESS(status = WdfCollectionCreate(
			&attributes,
			&Context->Settings.MOTIONSupportedNames
		)))
		{
			break;
		}

		if (!NT_SUCCESS(status = WdfCollectionCreate(
			&attributes,
			&Context->Settings.WIRELESSSupportedNames
		)))
		{
			break;
		}

		//
		// Query registry for dynamic values
		// 
		status = BthPS3_SettingsContextInit(Context);

	} while (FALSE);

	FuncExit(TRACE_BTH, "status=%!STATUS!", status);

	return status;
}
#pragma code_seg()

//
// Read runtime properties from registry
// 
#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
BthPS3_SettingsContextInit(
	PBTHPS3_SERVER_CONTEXT Context
)
{
	NTSTATUS status;
	WDFKEY hKey = NULL;
	WDF_OBJECT_ATTRIBUTES attributes;

	PAGED_CODE()

	DECLARE_CONST_UNICODE_STRING(autoEnableFilter, BTHPS3_REG_VALUE_AUTO_ENABLE_FILTER);
	DECLARE_CONST_UNICODE_STRING(autoDisableFilter, BTHPS3_REG_VALUE_AUTO_DISABLE_FILTER);
	DECLARE_CONST_UNICODE_STRING(autoEnableFilterDelay, BTHPS3_REG_VALUE_AUTO_ENABLE_FILTER_DELAY);

	DECLARE_CONST_UNICODE_STRING(isSIXAXISSupported, BTHPS3_REG_VALUE_IS_SIXAXIS_SUPPORTED);
	DECLARE_CONST_UNICODE_STRING(isNAVIGATIONSupported, BTHPS3_REG_VALUE_IS_NAVIGATION_SUPPORTED);
	DECLARE_CONST_UNICODE_STRING(isMOTIONSupported, BTHPS3_REG_VALUE_IS_MOTION_SUPPORTED);
	DECLARE_CONST_UNICODE_STRING(isWIRELESSSupported, BTHPS3_REG_VALUE_IS_WIRELESS_SUPPORTED);

	DECLARE_CONST_UNICODE_STRING(SIXAXISSupportedNames, BTHPS3_REG_VALUE_SIXAXIS_SUPPORTED_NAMES);
	DECLARE_CONST_UNICODE_STRING(NAVIGATIONSupportedNames, BTHPS3_REG_VALUE_NAVIGATION_SUPPORTED_NAMES);
	DECLARE_CONST_UNICODE_STRING(MOTIONSupportedNames, BTHPS3_REG_VALUE_MOTION_SUPPORTED_NAMES);
	DECLARE_CONST_UNICODE_STRING(WIRELESSSupportedNames, BTHPS3_REG_VALUE_WIRELESS_SUPPORTED_NAMES);

	//
	// Set default values
	//
	Context->Settings.AutoEnableFilter = TRUE;
	Context->Settings.AutoDisableFilter = TRUE;
	Context->Settings.AutoEnableFilterDelay = 10; // Seconds

	Context->Settings.IsSIXAXISSupported = TRUE;
	Context->Settings.IsNAVIGATIONSupported = TRUE;
	Context->Settings.IsMOTIONSupported = TRUE;
	Context->Settings.IsWIRELESSSupported = TRUE;

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
			&autoEnableFilter,
			&Context->Settings.AutoEnableFilter
		);

		(void)WdfRegistryQueryULong(
			hKey,
			&autoDisableFilter,
			&Context->Settings.AutoDisableFilter
		);

		(void)WdfRegistryQueryULong(
			hKey,
			&autoEnableFilterDelay,
			&Context->Settings.AutoEnableFilterDelay
		);

		(void)WdfRegistryQueryULong(
			hKey,
			&isSIXAXISSupported,
			&Context->Settings.IsSIXAXISSupported
		);

		(void)WdfRegistryQueryULong(
			hKey,
			&isNAVIGATIONSupported,
			&Context->Settings.IsNAVIGATIONSupported
		);

		(void)WdfRegistryQueryULong(
			hKey,
			&isMOTIONSupported,
			&Context->Settings.IsMOTIONSupported
		);

		(void)WdfRegistryQueryULong(
			hKey,
			&isWIRELESSSupported,
			&Context->Settings.IsWIRELESSSupported
		);

		WDF_OBJECT_ATTRIBUTES_INIT(&attributes);
		attributes.ParentObject = Context->Settings.SIXAXISSupportedNames;
		(void)WdfRegistryQueryMultiString(
			hKey,
			&SIXAXISSupportedNames,
			&attributes,
			Context->Settings.SIXAXISSupportedNames
		);

		WDF_OBJECT_ATTRIBUTES_INIT(&attributes);
		attributes.ParentObject = Context->Settings.NAVIGATIONSupportedNames;
		(void)WdfRegistryQueryMultiString(
			hKey,
			&NAVIGATIONSupportedNames,
			&attributes,
			Context->Settings.NAVIGATIONSupportedNames
		);

		WDF_OBJECT_ATTRIBUTES_INIT(&attributes);
		attributes.ParentObject = Context->Settings.MOTIONSupportedNames;
		(void)WdfRegistryQueryMultiString(
			hKey,
			&MOTIONSupportedNames,
			&attributes,
			Context->Settings.MOTIONSupportedNames
		);

		WDF_OBJECT_ATTRIBUTES_INIT(&attributes);
		attributes.ParentObject = Context->Settings.WIRELESSSupportedNames;
		(void)WdfRegistryQueryMultiString(
			hKey,
			&WIRELESSSupportedNames,
			&attributes,
			Context->Settings.WIRELESSSupportedNames
		);

		WdfRegistryClose(hKey);
	}

	return status;
}
#pragma code_seg()
