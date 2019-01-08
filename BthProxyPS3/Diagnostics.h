#pragma once

VOID
DumpL2CAPSignallingCommandCode(
    PCSTR Prefix,
    L2CAP_SIGNALLING_COMMAND_CODE Code,
    PVOID Buffer
);

VOID
TraceDumpBuffer(
    PVOID Buffer,
    ULONG BufferLength
);
