#include "Driver.h"
#include "BusLogic.Slots.tmh"


//
// Gets a stored slot/serial/index number for a given remote address or selects a free one
// 
#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
BthPS3_PDO_QuerySlot(
	PBTHPS3_DEVICE_CONTEXT_HEADER Header,
	BTH_ADDR RemoteAddress,
	PULONG Slot
)
{
	NTSTATUS status;
	WDFKEY hKey = NULL;
	WDFKEY hDeviceKey = NULL;

	FuncEntry(TRACE_BUSLOGIC);

	PAGED_CODE();

	DECLARE_UNICODE_STRING_SIZE(deviceKeyName, REG_CACHED_DEVICE_KEY_FMT_LEN);
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
			TraceError(
				TRACE_BUSLOGIC,
				"WdfDriverOpenParametersRegistryKey failed with status %!STATUS!",
				status
			);
			break;
		}

		if (!NT_SUCCESS(status = RtlUnicodeStringPrintf(
			&deviceKeyName,
			REG_CACHED_DEVICE_KEY_FMT,
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

		//
		// Try to get cached value
		// 
		if (NT_SUCCESS(WdfRegistryOpenKey(
			hKey,
			&deviceKeyName,
			GENERIC_READ,
			WDF_NO_OBJECT_ATTRIBUTES,
			&hDeviceKey
		))
			&& NT_SUCCESS(status = WdfRegistryQueryULong(
				hDeviceKey,
				&slotNo,
				Slot
			)))
		{
			TraceVerbose(
				TRACE_BUSLOGIC,
				"Found cached serial"
			);

			//
			// Validate
			// 
			if (*Slot == 0 /* invalid value */ || *Slot > BTHPS3_MAX_NUM_DEVICES)
			{
				status = STATUS_INVALID_PARAMETER;
				break;
			}

			WdfSpinLockAcquire(Header->SlotsSpinLock);

			SetBit(Header->Slots, *Slot);

			WdfSpinLockRelease(Header->SlotsSpinLock);

			status = STATUS_SUCCESS;
		}
		//
		// Get next free one
		// 
		else
		{
			TraceVerbose(
				TRACE_BUSLOGIC,
				"Looking for free serial"
			);

			WdfSpinLockAcquire(Header->SlotsSpinLock);

			//
			// ...otherwise get next free serial number
			// 
			for (*Slot = 1; *Slot <= BTHPS3_MAX_NUM_DEVICES; (*Slot)++)
			{
				if (!TestBit(Header->Slots, *Slot))
				{
					TraceVerbose(
						TRACE_BUSLOGIC,
						"Assigned serial: %d",
						*Slot
					);

					SetBit(Header->Slots, *Slot);

					status = STATUS_SUCCESS;
					break;
				}
			}

			if (*Slot > BTHPS3_MAX_NUM_DEVICES)
			{
				status = STATUS_NO_MORE_ENTRIES;
			}

			WdfSpinLockRelease(Header->SlotsSpinLock);
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

	FuncExit(TRACE_BUSLOGIC, "status=%!STATUS!", status);

	return status;
}
#pragma code_seg()

//
// Caches an occupied slot in the registry
// 
#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
BthPS3_PDO_AssignSlot(
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

	DECLARE_UNICODE_STRING_SIZE(deviceKeyName, REG_CACHED_DEVICE_KEY_FMT_LEN);
	DECLARE_CONST_UNICODE_STRING(slotNo, BTHPS3_REG_VALUE_SLOT_NO);
	DECLARE_CONST_UNICODE_STRING(slots, BTHPS3_REG_VALUE_SLOTS);

	do
	{
		if (Slot == 0 /* invalid value */ || Slot > BTHPS3_MAX_NUM_DEVICES)
		{
			status = STATUS_INVALID_PARAMETER;
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
			REG_CACHED_DEVICE_KEY_FMT,
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

		//
		// Create key for device
		// 
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

		//
		// Store serial value
		// 
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

	FuncExit(TRACE_BUSLOGIC, "status=%!STATUS!", status);

	return status;
}
#pragma code_seg()
