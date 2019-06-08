// syserr.h

extern LPWSTR gsAppName;

void syserr(LPCWSTR fmt,...);
void formatErrMsg(wchar_t*buf, size_t count, DWORD err);

#ifndef _countof
#  define _countof(array) (sizeof(array) / sizeof(array[0]))
#endif
