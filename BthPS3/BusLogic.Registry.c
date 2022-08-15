#include "Driver.h"
#include "BusLogic.Registry.tmh"


//
// Tries to get a stored slot/serial/index number for a given remote address, if available
// 
#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
BOOLEAN
BthPS3_PDO_Registry_QuerySlot(
	PBTHPS3_DEVICE_CONTEXT_HEADER Header,
	BTH_ADDR RemoteAddress,
	PULONG Slot
)
{
	BOOLEAN ret = FALSE;
	NTSTATUS status;
	WDFKEY hKey = NULL;
	WDFKEY hDeviceKey = NULL;

	FuncEntry(TRACE_BUSLOGIC);

	PAGED_CODE();

	DECLARE_UNICODE_STRING_SIZE(deviceKeyName, BTHPS3_BTH_ADDR_MAX_CHARS);
	DECLARE_CONST_UNICODE_STRING(slotNo, BTHPS3_REG_VALUE_SLOT_NO);

	do
	{
		//
		// Open
		//   HKEY_LOCAL_MACHINE\SYSTEM\CurrentControlSet\Services\BthPS3\Parameters
		// key
		// 
		if (!NT_SUCCESS(status = WdfDriverOpenParametersRegistryKey(
			WdfGetDriver(),
			STANDARD_RIGHTS_READ,
			WDF_NO_OBJECT_ATTRIBUTES,
			&hKey
		)))
		{
			break;
		}

		if (!NT_SUCCESS(status = RtlUnicodeStringPrintf(
			&deviceKeyName,
			L"%012llX",
			RemoteAddress
		)))
		{
			break;
		}

		if (!NT_SUCCESS(status = WdfRegistryOpenKey(
			hKey,
			&deviceKeyName,
			GENERIC_READ,
			WDF_NO_OBJECT_ATTRIBUTES,
			&hDeviceKey
		)))
		{
			break;
		}

		if (!NT_SUCCESS(status = WdfRegistryQueryULong(
			hDeviceKey,
			&slotNo,
			Slot
		)))
		{
			break;
		}

		if (*Slot == 0 /* invalid value */ || *Slot > BTHPS3_MAX_NUM_DEVICES)
		{
			break;
		}

		WdfSpinLockAcquire(Header->SlotsSpinLock);

		SetBit(Header->Slots, *Slot);

		WdfSpinLockRelease(Header->SlotsSpinLock);

		ret = TRUE;

	} while (FALSE);

	if (hKey)
	{
		WdfRegistryClose(hKey);
	}

	if (hDeviceKey)
	{
		WdfRegistryClose(hDeviceKey);
	}

	FuncExit(TRACE_BUSLOGIC, "ret=%d", ret);

	return ret;
}
#pragma code_seg()

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
void
BthPS3_PDO_Registry_AssignSlot(
	PBTHPS3_DEVICE_CONTEXT_HEADER Header,
	BTH_ADDR RemoteAddress,
	ULONG Slot
)
{
	NTSTATUS status;
	WDFKEY hKey = NULL;
	WDFKEY hDeviceKey = NULL;

	FuncEntry(TRACE_BUSLOGIC);

	PAGED_CODE();

	DECLARE_UNICODE_STRING_SIZE(deviceKeyName, BTHPS3_BTH_ADDR_MAX_CHARS);
	DECLARE_CONST_UNICODE_STRING(slotNo, BTHPS3_REG_VALUE_SLOT_NO);
	DECLARE_CONST_UNICODE_STRING(slots, BTHPS3_REG_VALUE_SLOTS);

	do
	{
		if (Slot == 0 /* invalid value */ || Slot > BTHPS3_MAX_NUM_DEVICES)
		{
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

		if (!NT_SUCCESS(status = RtlUnicodeStringPrintf(
			&deviceKeyName,
			L"%012llX",
			RemoteAddress
		)))
		{
			TraceError(
				TRACE_BUSLOGIC,
				"RtlUnicodeStringPrintf failed with status %!STATUS!",
				status
			);
			break;
		}

		if (!NT_SUCCESS(status = WdfRegistryCreateKey(
			hKey,
			&deviceKeyName,
			GENERIC_WRITE,
			REG_OPTION_NON_VOLATILE,
			NULL,
			WDF_NO_OBJECT_ATTRIBUTES,
			&hDeviceKey
		)))
		{
			TraceError(
				TRACE_BUSLOGIC,
				"WdfRegistryCreateKey failed with status %!STATUS!",
				status
			);
			break;
		}

		if (!NT_SUCCESS(status = WdfRegistryAssignULong(
			hDeviceKey,
			&slotNo,
			Slot
		)))
		{
			TraceError(
				TRACE_BUSLOGIC,
				"WdfRegistryAssignULong failed with status %!STATUS!",
				status
			);
			break;
		}

		WdfSpinLockAcquire(Header->SlotsSpinLock);

		SetBit(Header->Slots, Slot);

		WdfSpinLockRelease(Header->SlotsSpinLock);

		//
		// Store occupied slots in registry
		// 
		if (!NT_SUCCESS(status = WdfRegistryAssignValue(
			hKey,
			&slots,
			REG_BINARY,
			sizeof(Header->Slots),
			&Header->Slots
		)))
		{
			TraceError(
				TRACE_BUSLOGIC,
				"WdfRegistryAssignValue failed with status %!STATUS!",
				status
			);
			break;
		}		

	} while (FALSE);

	if (hKey)
	{
		WdfRegistryClose(hKey);
	}

	if (hDeviceKey)
	{
		WdfRegistryClose(hDeviceKey);
	}

	FuncExitNoReturn(TRACE_BUSLOGIC);
}
#pragma code_seg()
