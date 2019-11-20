/*
* BthPS3 - Windows kernel-mode Bluetooth profile and bus driver
*
* MIT License
*
* Copyright (C) 2018-2019  Nefarius Software Solutions e.U. and Contributors
*
* Permission is hereby granted, free of charge, to any person obtaining a copy
* of this software and associated documentation files (the "Software"), to deal
* in the Software without restriction, including without limitation the rights
* to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
* copies of the Software, and to permit persons to whom the Software is
* furnished to do so, subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included in all
* copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
* AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
* OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
* SOFTWARE.
*/


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
