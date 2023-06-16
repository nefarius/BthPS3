#include "Driver.h"
#include <ntimage.h>
#include "Compatibility.tmh"


// Function to find the base address of the driver module
PVOID FindDriverBaseAddress(STRING ModuleName)
{
    ULONG bufferSize = 0;
    PSYSTEM_MODULE_INFORMATION moduleInfo = NULL;
    PVOID baseAddress = NULL;

    const ULONG SystemModuleInformation = 11;

    // Query the required buffer size for module information
    NTSTATUS status = ZwQuerySystemInformation(
        SystemModuleInformation,
        &bufferSize,
        0,
        &bufferSize
    );

    if (status != STATUS_INFO_LENGTH_MISMATCH)
    {
        TraceError(
            TRACE_COMPAT,
            "ZwQuerySystemInformation failed with unexpected error"
        );
        return NULL;
    }

#pragma warning(disable:4996)
    // Allocate memory for the module information
    moduleInfo = (PSYSTEM_MODULE_INFORMATION)ExAllocatePoolWithTag(
        NonPagedPool,
        bufferSize,
        BTHPS_POOL_TAG
    );
#pragma warning(default:4996)

    if (moduleInfo == NULL)
    {
        TraceError(
            TRACE_COMPAT,
            "ExAllocatePoolWithTag failed"
        );
        return NULL;
    }

    // Retrieve the module information
    status = ZwQuerySystemInformation(
        SystemModuleInformation,
        moduleInfo,
        bufferSize,
        NULL
    );

    if (!NT_SUCCESS(status))
    {
        TraceError(
            TRACE_COMPAT,
            "ZwQuerySystemInformation failed with status %!STATUS!",
            status
        );
        ExFreePool(moduleInfo);
        return NULL;
    }

    STRING currentImageName;

    // Iterate through the loaded modules and find the desired module
    for (ULONG i = 0; i < moduleInfo->Count; i++)
    {
        RtlInitAnsiString(&currentImageName, moduleInfo->Module[i].ImageName);

        TraceVerbose(
            TRACE_COMPAT,
            "Current image name: %Z",
            &currentImageName
        );

        if (0 == RtlCompareString(&ModuleName, &currentImageName, TRUE))
        {
            TraceInformation(
                TRACE_COMPAT,
                "Found module"
            );

            // Found the module, store the base address
            baseAddress = moduleInfo->Module[i].Base;
            break;
        }
    }

    ExFreePool(moduleInfo);

    return baseAddress;
}


VOID EnumerateExportedFunctions(PVOID ModuleBase)
{
    ULONG exportSize;

    DECLARE_CONST_UNICODE_STRING(routineName, L"RtlImageDirectoryEntryToData");

    const t_RtlImageDirectoryEntryToData fp_RtlImageDirectoryEntryToData =
        (t_RtlImageDirectoryEntryToData)MmGetSystemRoutineAddress((PUNICODE_STRING)&routineName);

    if (fp_RtlImageDirectoryEntryToData == NULL)
    {
        TraceError(
            TRACE_COMPAT,
            "RtlImageDirectoryEntryToData not found"
        );
        return;
    }

    // Retrieve the export directory information
    const PIMAGE_EXPORT_DIRECTORY exportDirectory = (PIMAGE_EXPORT_DIRECTORY)fp_RtlImageDirectoryEntryToData(
        ModuleBase,
        TRUE,
        IMAGE_DIRECTORY_ENTRY_EXPORT,
        &exportSize
    );

    if (exportDirectory == NULL)
    {
        TraceError(
            TRACE_COMPAT,
            "Export directory not found in the module"
        );
        return;
    }

#if defined(_M_X64) || defined(__amd64__) || defined(__aarch64__) || defined(_M_ARM64)
    const PULONGLONG functionAddresses = (PULONGLONG)((ULONG_PTR)ModuleBase + exportDirectory->AddressOfFunctions);
#else
    const PULONG functionAddresses = (PULONG)((ULONG_PTR)ModuleBase + exportDirectory->AddressOfFunctions);
#endif

    const PULONG functionNames = (PULONG)((ULONG_PTR)ModuleBase + exportDirectory->AddressOfNames);
    const PUSHORT functionOrdinals = (PUSHORT)((ULONG_PTR)ModuleBase + exportDirectory->AddressOfNameOrdinals);

    UNREFERENCED_PARAMETER(functionAddresses);

    for (DWORD i = 0; i < exportDirectory->NumberOfNames; i++)
    {
#if defined(_M_X64) || defined(__amd64__) || defined(__aarch64__) || defined(_M_ARM64)
        PVOID functionAddress = (PULONGLONG)((ULONG_PTR)ModuleBase + functionAddresses[i]);
#else
        PVOID functionAddress = (PULONG)((ULONG_PTR)ModuleBase + functionAddresses[i]);
#endif
        const char* functionName = (const char*)((ULONG_PTR)ModuleBase + functionNames[i]);
        USHORT functionOrdinal = functionOrdinals[i];

        // Process the exported function name and ordinal as needed
        // ...

        DbgPrint("Exported Function: %s (Ordinal: %hu)\n", functionName, functionOrdinal);

        TraceInformation(
            TRACE_COMPAT,
            "Exported Function: %s (Ordinal: %hu, Address: %p)",
            functionName,
            functionOrdinal,
            functionAddress
        );
    }
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
