#include "Driver.h"
#include <ntstrsafe.h>
#include "util.tmh"

//
// Compares a remote name (UTF8 char*) to a WDFSTRING
// 
BOOLEAN
StringUtil_BthNameIsEqual(
    PCHAR Lhs,
    WDFSTRING Rhs
)
{
    NTSTATUS status;
    UNICODE_STRING usRhs;
    DECLARE_UNICODE_STRING_SIZE(usLhs, BTH_MAX_NAME_SIZE);

    //
    // WDFSTRING to UNICODE_STRING
    // 
    WdfStringGetUnicodeString(
        Rhs,
        &usRhs
    );

    //
    // CHAR to UNICODE_STRING
    // 
    status = RtlUnicodeStringPrintf(&usLhs, L"%hs", Lhs);
    if (!NT_SUCCESS(status)) {
        TraceEvents(TRACE_LEVEL_ERROR,
            TRACE_UTIL,
            "RtlUnicodeStringPrintf failed with status %!STATUS!",
            status
        );
    }

    TraceEvents(TRACE_LEVEL_VERBOSE,
        TRACE_UTIL,
        "%!FUNC! LHS: \"%wZ\" RHS: \"%wZ\"",
        &usLhs, &usRhs
    );

    //
    // Compare case-insensitive
    // 
    return RtlEqualUnicodeString(&usLhs, &usRhs, TRUE);
}

//
// Checks if a remote name (UTF8 char*) is within a WDFCOLLECTION
// 
BOOLEAN
StringUtil_BthNameIsInCollection(
    PCHAR Entry,
    WDFCOLLECTION Array
)
{
    ULONG i;

    for (i = 0; i < WdfCollectionGetCount(Array); i++)
    {
        if (StringUtil_BthNameIsEqual(Entry, WdfCollectionGetItem(Array, i)))
            return TRUE;
    }

    return FALSE;
}
