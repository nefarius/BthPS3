#include "Driver.h"
#include <ntstrsafe.h>

BOOLEAN
StringUtil_BthNameIsEqual(
    CHAR Lhs,
    WDFSTRING Rhs
)
{
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
    RtlUnicodeStringPrintf(&usLhs, L"%s", Lhs);

    //
    // Compare case-insensitive
    // 
    return RtlEqualUnicodeString(&usLhs, &usRhs, TRUE);
}
