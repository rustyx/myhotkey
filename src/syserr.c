// syserr.c
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include "syserr.h"

void syserr(LPCWSTR fmt,...)
{
    DWORD err = GetLastError();
    DWORD errOk = 0;
    LPVOID lpMsgBuf = NULL;
    va_list va;
    wchar_t buf[1024];
    if (err)
        errOk = FormatMessageW(
            FORMAT_MESSAGE_ALLOCATE_BUFFER |
            FORMAT_MESSAGE_FROM_SYSTEM |
            FORMAT_MESSAGE_IGNORE_INSERTS,
            NULL, err,
            MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
            (LPWSTR)& lpMsgBuf, 0, NULL);
    va_start (va, fmt);
    _vswprintf_c(buf, _countof(buf), fmt, va);
    buf[_countof(buf) - 1] = 0;
    va_end(va);
    if (errOk) {
        wcscat_s(buf, _countof(buf), L": ");
        wcscat_s(buf, _countof(buf), lpMsgBuf);
        LocalFree(lpMsgBuf);
    }
    MessageBoxW(NULL, buf, gsAppName, MB_OK | MB_ICONSTOP | MB_SETFOREGROUND);
}

void formatErrMsg(wchar_t *buf, size_t count, DWORD err)
{
    LPVOID lpMsgBuf;
    if (!err) {
        buf[0] = 0;
    }
    else if (FormatMessageW(
        FORMAT_MESSAGE_ALLOCATE_BUFFER |
        FORMAT_MESSAGE_FROM_SYSTEM |
        FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL, err,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        (LPWSTR)& lpMsgBuf, 0, NULL))
    {
        wcscat_s(buf, count, lpMsgBuf);
        LocalFree(lpMsgBuf);
    } else {
        wcscat_s(buf, count, L"Unknown error");
    }
}
