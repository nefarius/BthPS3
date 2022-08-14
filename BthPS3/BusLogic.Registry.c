#include "Driver.h"
#include "BusLogic.Registry.tmh"


//
// Tries to get a stored slot/serial/index number for a given remote address, if available
// 
#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
BOOLEAN
BthPS3_PDO_Registry_QuerySlot(
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

	DECLARE_UNICODE_STRING_SIZE(deviceKeyName, 13 /* 12 characters + NULL terminator */);
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
	BTH_ADDR RemoteAddress,
	ULONG Slot
)
{
	NTSTATUS status;
	WDFKEY hKey = NULL;
	WDFKEY hDeviceKey = NULL;

	FuncEntry(TRACE_BUSLOGIC);

	PAGED_CODE();

	DECLARE_UNICODE_STRING_SIZE(deviceKeyName, 13 /* 12 characters + NULL terminator */);
	DECLARE_CONST_UNICODE_STRING(slotNo, BTHPS3_REG_VALUE_SLOT_NO);

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
			break;
		}

		if (!NT_SUCCESS(status = WdfRegistryAssignULong(
			hDeviceKey,
			&slotNo,
			Slot
		)))
		{
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
