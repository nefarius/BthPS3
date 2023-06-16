#pragma once

typedef VOID (*t_WppRecorderReplay)(
    _In_ PVOID WppCb,
    _In_ TRACEHANDLE WppTraceHandle,
    _In_ ULONG EnableFlags,
    _In_ UCHAR EnableLevel
);

// Structure representing a loaded module
typedef struct _SYSTEM_MODULE_INFORMATION_ENTRY
{
    PVOID Unknown1;
    PVOID Unknown2;
    PVOID Base;
    ULONG Size;
    ULONG Flags;
    USHORT Index;
    USHORT NameLength;
    USHORT LoadCount;
    USHORT PathLength;
    CHAR ImageName[256];
} SYSTEM_MODULE_INFORMATION_ENTRY, *PSYSTEM_MODULE_INFORMATION_ENTRY;

// Structure representing the loaded module information
typedef struct _SYSTEM_MODULE_INFORMATION
{
    ULONG Count;
    SYSTEM_MODULE_INFORMATION_ENTRY Module[1];
} SYSTEM_MODULE_INFORMATION, *PSYSTEM_MODULE_INFORMATION;

// Function prototype for ZwQuerySystemInformation
NTSYSAPI NTSTATUS NTAPI ZwQuerySystemInformation(
    ULONG SystemInformationClass,
    PVOID SystemInformation,
    ULONG SystemInformationLength,
    PULONG ReturnLength
);

typedef struct _LDR_DATA_TABLE_ENTRY
{
    LIST_ENTRY64 InLoadOrderLinks;
    PVOID ExceptionTable;
    ULONG ExceptionTableSize;
    PVOID GpValue;
    PVOID NonPagedDebugInfo;
    PVOID ImageBase;
    PVOID EntryPoint;
    ULONG SizeOfImage;
    UNICODE_STRING FullImageName;
    UNICODE_STRING BaseImageName;
    ULONG Flags;
    USHORT LoadCount;
    USHORT TlsIndex;
    LIST_ENTRY64 HashLinks;
    PVOID SectionPointer;
    ULONG CheckSum;
    ULONG TimeDateStamp;
    PVOID LoadedImports;
    PVOID EntryPointActivationContext;
    PVOID PatchInformation;
} LDR_DATA_TABLE_ENTRY, *PLDR_DATA_TABLE_ENTRY;

typedef PVOID (NTAPI* t_RtlImageDirectoryEntryToData)(
    IN PVOID Base,
    IN BOOLEAN MappedAsImage,
    IN USHORT DirectoryEntry,
    OUT PULONG Size
);


_Success_(return == STATUS_SUCCESS)
_Must_inspect_result_
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
FindDriverBaseAddress(
    _In_ STRING ModuleName,
    _Inout_ PVOID* ModuleBase
);

_Success_(return == STATUS_SUCCESS)
_Must_inspect_result_
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
FundExportedFunctionAddress(
    _In_ PVOID ModuleBase,
    _In_ STRING FunctionName,
    _Inout_ PVOID* FunctionAddress
);
