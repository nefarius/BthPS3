#pragma once

BOOLEAN
StringUtil_BthNameIsEqual(
    PCHAR Lhs,
    WDFSTRING Rhs
);

BOOLEAN
StringUtil_BthNameIsInCollection(
    PCHAR Entry,
    WDFCOLLECTION Array
);
