#pragma once
#define _SILENCE_CXX17_CODECVT_HEADER_DEPRECATION_WARNING 1
#include <codecvt> // codecvt_utf8
#include <locale>  // wstring_convert

// locale-independent to_wstring that actually works, no nonsense
[[nodiscard]] inline std::wstring to_wstring(const std::string& utf8)
{
    std::wstring_convert<std::codecvt_utf8<wchar_t>> cvt;
    return cvt.from_bytes(utf8);
}

// locale-independent to_string that actually works, no nonsense
[[nodiscard]] inline std::string to_string(const std::wstring& unicode)
{
    std::wstring_convert<std::codecvt_utf8<wchar_t>> cvt;
    return cvt.to_bytes(unicode);
}

// locale-independent to_upper that actually works, no nonsense
inline void to_upper(std::wstring& unicode)
{
	std::locale invariant{""}; // (*) - required for proper conversion
    std::use_facet<std::ctype<wchar_t>>(invariant)
        .toupper(&unicode[0], &unicode[0] + unicode.size());
}

// locale-independent to_lower that actually works, no nonsense
inline void to_lower(std::wstring& unicode)
{
	std::locale invariant{""}; // (*) - required for proper conversion
    std::use_facet<std::ctype<wchar_t>>(invariant)
        .tolower(&unicode[0], &unicode[0] + unicode.size());
}

// locale-independent to_upper that actually works, no nonsense
[[nodiscard]] inline std::string to_upper(const std::string& u8)
{
    std::wstring_convert<std::codecvt_utf8<wchar_t>> cvt;
    std::wstring wstr = cvt.from_bytes(u8);
    to_upper(wstr);
    return cvt.to_bytes(wstr);
}

// locale-independent to_lower that actually works, no nonsense
[[nodiscard]] inline std::string to_lower(const std::string& u8)
{
    std::wstring_convert<std::codecvt_utf8<wchar_t>> cvt;
    std::wstring wstr = cvt.from_bytes(u8);
    to_lower(wstr);
    return cvt.to_bytes(wstr);
}
