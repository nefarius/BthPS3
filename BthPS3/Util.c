/**********************************************************************************
 *                                                                                *
 * BthPS3 - Windows kernel-mode Bluetooth profile and bus driver                  *
 *                                                                                *
 * BSD 3-Clause License                                                           *
 *                                                                                *
 * Copyright (c) 2018-2021, Nefarius Software Solutions e.U.                      *
 * All rights reserved.                                                           *
 *                                                                                *
 * Redistribution and use in source and binary forms, with or without             *
 * modification, are permitted provided that the following conditions are met:    *
 *                                                                                *
 * 1. Redistributions of source code must retain the above copyright notice, this *
 *    list of conditions and the following disclaimer.                            *
 *                                                                                *
 * 2. Redistributions in binary form must reproduce the above copyright notice,   *
 *    this list of conditions and the following disclaimer in the documentation   *
 *    and/or other materials provided with the distribution.                      *
 *                                                                                *
 * 3. Neither the name of the copyright holder nor the names of its               *
 *    contributors may be used to endorse or promote products derived from        *
 *    this software without specific prior written permission.                    *
 *                                                                                *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"    *
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE      *
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE *
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE   *
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL     *
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR     *
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER     *
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,  *
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE  *
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.           *
 *                                                                                *
 **********************************************************************************/


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
