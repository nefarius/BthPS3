#include "Driver.h"
#include <ntimage.h>
#include "Compatibility.tmh"

typedef VOID(*t_WppRecorderReplay)(
	_In_ PVOID       WppCb,
	_In_ TRACEHANDLE WppTraceHandle,
	_In_ ULONG       EnableFlags,
	_In_ UCHAR       EnableLevel
	);

// Structure representing a loaded module
typedef struct _SYSTEM_MODULE_INFORMATION_ENTRY {
	PVOID   Unknown1;
	PVOID   Unknown2;
	PVOID   Base;
	ULONG   Size;
	ULONG   Flags;
	USHORT  Index;
	USHORT  NameLength;
	USHORT  LoadCount;
	USHORT  PathLength;
	CHAR    ImageName[256];
} SYSTEM_MODULE_INFORMATION_ENTRY, * PSYSTEM_MODULE_INFORMATION_ENTRY;

// Structure representing the loaded module information
typedef struct _SYSTEM_MODULE_INFORMATION {
	ULONG   Count;
	SYSTEM_MODULE_INFORMATION_ENTRY Module[1];
} SYSTEM_MODULE_INFORMATION, * PSYSTEM_MODULE_INFORMATION;

// Function prototype for ZwQuerySystemInformation
NTSYSAPI NTSTATUS NTAPI ZwQuerySystemInformation(
	ULONG SystemInformationClass,
	PVOID SystemInformation,
	ULONG SystemInformationLength,
	PULONG ReturnLength
);

// Function to find the base address of the driver module
static PVOID FindDriverBaseAddress(STRING moduleName)
{
	ULONG bufferSize = 0;
	PSYSTEM_MODULE_INFORMATION moduleInfo = NULL;
	PVOID baseAddress = NULL;

	const ULONG SystemModuleInformation = 11;

	// Query the required buffer size for module information
	NTSTATUS status = ZwQuerySystemInformation(SystemModuleInformation, &bufferSize, 0, &bufferSize);

	if (status != STATUS_INFO_LENGTH_MISMATCH)
		return NULL;

	// Allocate memory for the module information
	moduleInfo = (PSYSTEM_MODULE_INFORMATION)ExAllocatePool2(NonPagedPool, bufferSize, BTHPS_POOL_TAG);

	if (moduleInfo == NULL)
		return NULL;

	// Retrieve the module information
	status = ZwQuerySystemInformation(SystemModuleInformation, moduleInfo, bufferSize, NULL);

	if (!NT_SUCCESS(status))
	{
		ExFreePool(moduleInfo);
		return NULL;
	}

	STRING currentImageName;

	// Iterate through the loaded modules and find the desired module
	for (ULONG i = 0; i < moduleInfo->Count; i++)
	{
		RtlInitAnsiString(&currentImageName, (PCSZ)moduleInfo->Module[i].ImageName);

		if (0 == RtlCompareString(&moduleName, &currentImageName, TRUE))
		{
			// Found the module, store the base address
			baseAddress = moduleInfo->Module[i].Base;
			break;
		}
	}

	ExFreePool(moduleInfo);
	return baseAddress;
}

/*
__declspec(dllexport)
void
imp_WppRecorderReplay(
	_In_ PVOID       WppCb,
	_In_ TRACEHANDLE WppTraceHandle,
	_In_ ULONG       EnableFlags,
	_In_ UCHAR       EnableLevel
)
{
	FuncEntry(TRACE_COMPAT);

	UNREFERENCED_PARAMETER(WppCb);
	UNREFERENCED_PARAMETER(WppTraceHandle);
	UNREFERENCED_PARAMETER(EnableFlags);
	UNREFERENCED_PARAMETER(EnableLevel);

	const KIRQL irql = KeGetCurrentIrql();

	if (irql != PASSIVE_LEVEL)
	{
		TraceInformation(TRACE_COMPAT, "Incompatible IRQL %!irql!", irql);
		return;
	}

	const STRING targetModuleName = RTL_CONSTANT_STRING("WppRecorder.sys");

	const PVOID driverBaseAddress = FindDriverBaseAddress(targetModuleName);

	if (driverBaseAddress != NULL)
	{
		TraceVerbose(TRACE_COMPAT, "Module found");

		RTL_OSVERSIONINFOEXW versionInfo;
		versionInfo.dwOSVersionInfoSize = sizeof(versionInfo);
		NTSTATUS status = RtlGetVersion((PRTL_OSVERSIONINFOW)&versionInfo);

		if (NT_SUCCESS(status))
		{
			TraceInformation(
				TRACE_COMPAT,
				"Version: %d.%d.%d",
				versionInfo.dwMajorVersion,
				versionInfo.dwMinorVersion,
				versionInfo.dwBuildNumber
			);
		}

		if (TRUE)
			return;
		// Calculate the address of the desired function
		const ULONG_PTR moduleBase = (ULONG_PTR)driverBaseAddress;
		const ULONG_PTR functionOffset = 0x000040D0;  // imp_WppRecorderReplay

		const PVOID functionAddress = (PVOID)(moduleBase + functionOffset);

		// Typecast the function address to the desired function pointer type
		const t_WppRecorderReplay functionPointer = (t_WppRecorderReplay)functionAddress;

		// Call the function
		functionPointer(WppCb, WppTraceHandle, EnableFlags, EnableLevel);
	}
	else
	{
		TraceVerbose(TRACE_COMPAT, "Module not found");
		// Driver module not found
	}

	FuncExitNoReturn(TRACE_COMPAT);
}

*/