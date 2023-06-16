#include "Driver.h"
#include <ntimage.h>
#include "Compatibility.tmh"


// 
// Finds the base address of a driver module
// 
NTSTATUS
FindDriverBaseAddress(
    STRING ModuleName,
    PVOID* ModuleBase
)
{
    ULONG bufferSize = 0;
    PSYSTEM_MODULE_INFORMATION moduleInfo = NULL;

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
        return status;
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
        return STATUS_INSUFFICIENT_RESOURCES;
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
        ExFreePool(moduleInfo);
        return status;
    }

    STRING currentImageName;

    status = STATUS_NOT_FOUND;
    // Iterate through the loaded modules and find the desired module
    for (ULONG i = 0; i < moduleInfo->Count; i++)
    {
        RtlInitAnsiString(&currentImageName, moduleInfo->Module[i].ImageName);

        if (0 == RtlCompareString(&ModuleName, &currentImageName, TRUE))
        {
            // Found the module, store the base address
            if (ModuleBase)
            {
                status = STATUS_SUCCESS;
                *ModuleBase = moduleInfo->Module[i].Base;
            }
            break;
        }
    }

    ExFreePool(moduleInfo);

    return status;
}

VOID
imp_WppRecorderReplay(
    _In_ PVOID       WppCb,
    _In_ TRACEHANDLE WppTraceHandle,
    _In_ ULONG       EnableFlags,
    _In_ UCHAR       EnableLevel
);

VOID EnumerateExportedFunctions(PVOID ModuleBase)
{
    ULONG exportSize;

    FuncEntry(TRACE_COMPAT);

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
    
    const PULONG functionAddresses = (PULONG)((ULONG_PTR)ModuleBase + exportDirectory->AddressOfFunctions);
    const PULONG functionNames = (PULONG)((ULONG_PTR)ModuleBase + exportDirectory->AddressOfNames);
    const PUSHORT functionOrdinals = (PUSHORT)((ULONG_PTR)ModuleBase + exportDirectory->AddressOfNameOrdinals);

    for (DWORD i = 0; i < exportDirectory->NumberOfNames; i++)
    {
        const char* functionName = (const char*)((ULONG_PTR)ModuleBase + functionNames[i]);
        const USHORT functionOrdinal = functionOrdinals[i];

        const ULONG functionRva = functionAddresses[i];
        const PVOID functionAddress = (PVOID)((ULONG_PTR)ModuleBase + functionRva);

        // Process the exported function name and ordinal as needed
        // ...

        DbgPrint("Exported Function: %s (Ordinal: %hu)\n", functionName, functionOrdinal);
        
        TraceInformation(
            TRACE_COMPAT,
            "Exported Function: %s (Ordinal: %hu, Address: 0x%p)",
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
