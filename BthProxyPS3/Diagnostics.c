#include "Driver.h"
#include "diagnostics.tmh"
#include <stdio.h>

VOID
DumpL2CAPSignallingCommandCode(
    PCSTR Prefix,
    L2CAP_SIGNALLING_COMMAND_CODE Code,
    PVOID Buffer
)
{
    switch (Code)
    {
    case L2CAP_Command_Reject:
    {
        PL2CAP_SIGNALLING_COMMAND_REJECT data = (PL2CAP_SIGNALLING_COMMAND_REJECT)Buffer;

        TraceEvents(TRACE_LEVEL_INFORMATION,
            TRACE_DIAG,
            "%s L2CAP_Command_Reject " \
            "[Code: 0x%02X, Identifier: 0x%02X, Length: %hu, Reason: 0x%04X]",
            Prefix,
            data->Code,
            data->Identifier,
            data->Length,
            data->Reason
        );

        break;
    }

#pragma region L2CAP_Connection_Request

    case L2CAP_Connection_Request:
    {
        PL2CAP_SIGNALLING_CONNECTION_REQUEST data = (PL2CAP_SIGNALLING_CONNECTION_REQUEST)Buffer;

        TraceEvents(TRACE_LEVEL_INFORMATION,
            TRACE_DIAG,
            "%s L2CAP_Connection_Request " \
            "[Code: 0x%02X, Identifier: 0x%02X, Length: %hu, PSM: 0x%04X, SCID: 0x%04X]",
            Prefix,
            data->Code,
            data->Identifier,
            data->Length,
            data->PSM,
            *((PUSHORT)&data->SCID)
        );

        break;
    }

#pragma endregion

#pragma region L2CAP_Connection_Response

    case L2CAP_Connection_Response:
    {
        PL2CAP_SIGNALLING_CONNECTION_RESPONSE data = (PL2CAP_SIGNALLING_CONNECTION_RESPONSE)Buffer;

        TraceEvents(TRACE_LEVEL_INFORMATION,
            TRACE_DIAG,
            "%s L2CAP_Connection_Response " \
            "[Code: 0x%02X, Identifier: 0x%02X, Length: %hu, DCID: 0x%04X, SCID: 0x%04X, Result: 0x%04X, Status: 0x%04X]",
            Prefix,
            data->Code,
            data->Identifier,
            data->Length,
            *((PUSHORT)&data->DCID),
            *((PUSHORT)&data->SCID),
            data->Result,
            data->Status
        );

        break;
    }

#pragma endregion

#pragma region L2CAP_Configuration_Request

    case L2CAP_Configuration_Request:
    {
        PL2CAP_SIGNALLING_CONFIGURATION_REQUEST data = (PL2CAP_SIGNALLING_CONFIGURATION_REQUEST)Buffer;

        TraceEvents(TRACE_LEVEL_INFORMATION,
            TRACE_DIAG,
            "%s L2CAP_Configuration_Request " \
            "[Code: 0x%02X, Identifier: 0x%02X, Length: %hu, DCID: 0x%04X, Flags: 0x%04X, Options: 0x%08X]",
            Prefix,
            data->Code,
            data->Identifier,
            data->Length,
            *((PUSHORT)&data->DCID),
            data->Flags,
            data->Options
        );

        break;
    }

#pragma endregion

#pragma region L2CAP_Configuration_Response

    case L2CAP_Configuration_Response:
    {
        PL2CAP_SIGNALLING_CONFIGURATION_RESPONSE data = (PL2CAP_SIGNALLING_CONFIGURATION_RESPONSE)Buffer;

        TraceEvents(TRACE_LEVEL_INFORMATION,
            TRACE_DIAG,
            "%s L2CAP_Configuration_Response " \
            "[Code: 0x%02X, Identifier: 0x%02X, Length: %hu, SCID: 0x%04X, Flags: 0x%04X, Result: 0x%04X, Options: 0x%04X]",
            Prefix,
            data->Code,
            data->Identifier,
            data->Length,
            *((PUSHORT)&data->SCID),
            data->Flags,
            data->Result,
            data->Options
        );

        break;
    }

#pragma endregion

#pragma region L2CAP_Disconnection_Request

    case L2CAP_Disconnection_Request:
    {
        PL2CAP_SIGNALLING_DISCONNECTION_REQUEST data = (PL2CAP_SIGNALLING_DISCONNECTION_REQUEST)Buffer;

        TraceEvents(TRACE_LEVEL_INFORMATION,
            TRACE_DIAG,
            "%s L2CAP_Disconnection_Request " \
            "[Code: 0x%02X, Identifier: 0x%02X, Length: %hu, DCID: 0x%04X, SCID: 0x%04X]",
            Prefix,
            data->Code,
            data->Identifier,
            data->Length,
            *((PUSHORT)&data->DCID),
            *((PUSHORT)&data->SCID)
        );

        break;
    }

#pragma endregion

#pragma region L2CAP_Disconnection_Response

    case L2CAP_Disconnection_Response:
    {
        PL2CAP_SIGNALLING_DISCONNECTION_RESPONSE data = (PL2CAP_SIGNALLING_DISCONNECTION_RESPONSE)Buffer;

        TraceEvents(TRACE_LEVEL_INFORMATION,
            TRACE_DIAG,
            "%s L2CAP_Disconnection_Response " \
            "[Code: 0x%02X, Identifier: 0x%02X, Length: %hu, DCID: 0x%04X, SCID: 0x%04X]",
            Prefix,
            data->Code,
            data->Identifier,
            data->Length,
            *((PUSHORT)&data->DCID),
            *((PUSHORT)&data->SCID)
        );

        break;
    }

#pragma endregion

    default:
        TraceEvents(TRACE_LEVEL_WARNING,
            TRACE_DIAG,
            "Unknown L2CAP command: 0x%02X",
            Code);
        break;
    }
}

VOID
TraceDumpBuffer(
    PVOID Buffer,
    ULONG BufferLength
)
{
    PWSTR   dumpBuffer;
    size_t  dumpBufferLength;
    ULONG   i;

    dumpBufferLength = ((BufferLength * sizeof(WCHAR)) * 3) + 2;
    dumpBuffer = ExAllocatePoolWithTag(
        NonPagedPoolNx,
        dumpBufferLength,
        'egrA'
    );
    RtlZeroMemory(dumpBuffer, dumpBufferLength);

    for (i = 0; i < BufferLength; i++)
    {
        swprintf(&dumpBuffer[i * 3], L"%02X ", ((PUCHAR)Buffer)[i]);
    }

    TraceEvents(TRACE_LEVEL_INFORMATION,
        TRACE_DIAG,
        "%ws",
        dumpBuffer);

    ExFreePoolWithTag(dumpBuffer, 'egrA');
}

