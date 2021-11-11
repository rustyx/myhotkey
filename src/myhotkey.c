// myhotkey.c
// Created by rustyx (me@rustyx.org)

#define _WIN32_IE 0x0500
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>
#include <userenv.h>
#include <stdio.h>
#include <share.h>
#include <ctype.h>
#include "syserr.h"
#include "resource.h"
#ifndef ERROR_ELEVATION_REQUIRED
#  define ERROR_ELEVATION_REQUIRED 740
#endif

typedef struct configEntry {
    UINT hotkey;
    UINT modifiers;
    LPWSTR exename;
    LPWSTR params;
    LPWSTR workdir;
    DWORD status;
    DWORD pid;
    HWND hwnd;
    struct configEntry *prev;
    struct configEntry *next;
} CONFIG_ENTRY;

HINSTANCE ghInst;
HANDLE ghEventExit;
LPWSTR gsConfigFile, gsAppName, gsMsgReloaded;
CONFIG_ENTRY* gConfig;
HWND ghWnd;
#define wmReload (WM_USER + 101)

BOOL CALLBACK EnumWndProc(HWND hwnd, LPARAM lparam)
{
    CONFIG_ENTRY* cfg = (CONFIG_ENTRY*)lparam;
    DWORD pid = 0;
    GetWindowThreadProcessId(hwnd, &pid);
    if (pid == cfg->pid && !GetWindow(hwnd, GW_OWNER) && IsWindowVisible(hwnd)) {
        cfg->hwnd = hwnd;
        return FALSE;
    }
    return TRUE;
}

void start(CONFIG_ENTRY *cfg)
{
    HANDLE htoken;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY|TOKEN_DUPLICATE, &htoken)) {
        syserr(L"OpenProcessToken");
        return;
    }
    LPVOID env;
    if (!CreateEnvironmentBlock(&env, htoken, FALSE)) {
        syserr(L"CreateEnvironmentBlock");
        CloseHandle(htoken);
        return;
    }

    STARTUPINFOW si;
    PROCESS_INFORMATION pi;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    if (cfg->pid) {
        HANDLE hProcess = OpenProcess(SYNCHRONIZE, FALSE, cfg->pid);
        if (hProcess) {
            CloseHandle(hProcess);
            EnumWindows(EnumWndProc, (LPARAM)cfg);
            if (cfg->hwnd) {
                BringWindowToTop(cfg->hwnd);
                if (SetForegroundWindow(cfg->hwnd)) {
                    return;
                }
                cfg->pid = 0;
            }
        }
        else {
            cfg->pid = 0;
        }
    }
    if (!CreateProcessW(cfg->exename, cfg->params, NULL, NULL, FALSE, CREATE_UNICODE_ENVIRONMENT, env, cfg->workdir, &si, &pi)) {
        if (GetLastError() == ERROR_ELEVATION_REQUIRED) {
            // ShellExecute is slower than CreateProcess. Only use if elevation required.
            SHELLEXECUTEINFOW ei = { 0 };
            ei.cbSize = sizeof(ei);
            ei.lpFile = cfg->exename;
            ei.lpParameters = cfg->params;
            ei.lpDirectory = cfg->workdir;
            ei.nShow = SW_SHOWNORMAL;
            ei.fMask = SEE_MASK_NOCLOSEPROCESS;
            if (!ShellExecuteExW(&ei) || !ei.hProcess) {
                syserr(L"Cannot start \"%s\" \"%s\" in \"%s\"", cfg->exename, cfg->params, cfg->workdir);
            }
            else {
                cfg->hwnd = NULL;
                cfg->pid = GetProcessId(ei.hProcess);
                CloseHandle(ei.hProcess);
            }
        } else {
            syserr(L"Cannot start \"%s\" \"%s\" in \"%s\"", cfg->exename, cfg->params, cfg->workdir);
        }
    } else {
        cfg->hwnd = NULL;
        cfg->pid = GetProcessId(pi.hProcess);
        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);
    }
    DestroyEnvironmentBlock(env);
    CloseHandle(htoken);
}

LPWSTR expandStr(LPWSTR arg)
{
    if (!arg[0] || (arg[0] == L'-' && arg[1] == 0))
        return NULL;
    DWORD len = (DWORD)wcslen(arg) + 1;
    if (wcschr(arg, L'%'))
        len *= 4;
    if (len < 1024)
        len = 1024;
    LPWSTR buf = malloc(len * sizeof(arg[0]));
    if (!buf)
        return arg;
    ExpandEnvironmentStringsW(arg, buf, len);
    LPWSTR retbuf = _wcsdup(buf);
    if (!retbuf)
        return arg;
    free(buf);
    return retbuf;
}

LPWSTR searchPath(LPWSTR arg, LPCWSTR path)
{
    if (!arg)
        return NULL;
    wchar_t buf[MAX_PATH + 256];
    DWORD len = SearchPathW(path, arg, L".exe", _countof(buf), buf, NULL);
    if (!len)
        return arg;
    LPWSTR retbuf = _wcsdup(buf);
    if (!retbuf)
        return arg;
    free(arg);
    return retbuf;
}

CONFIG_ENTRY* loadConfig(LPCWSTR cfgfile)
{
    CONFIG_ENTRY *cfg = NULL, *entry, *prev = NULL;
    char cbuf[4096];
    wchar_t wbuf[4096];
    size_t i, line = 0, errors = 0;
    CONFIG_ENTRY cur;
    FILE* f;
    for (i = 0; i < 50; ++i) {
        f = _wfsopen(cfgfile, L"rt", _SH_DENYNO);
        if (f)
            break;
        Sleep(100);
    }
    if (!f)
        return NULL;
    DWORD pathlen = GetEnvironmentVariableW(L"PATH", NULL, 0) + 4;
    wchar_t *path = malloc(pathlen * sizeof(wchar_t));
    GetEnvironmentVariableW(L"PATH", path, pathlen);

    while (fgets(cbuf, sizeof(cbuf), f)) {
        for (i = 0; i < sizeof(cbuf) - 1 && cbuf[i] != '\r' && cbuf[i] != '\n' && cbuf[i]; i++)
            ;
        cbuf[i] = 0;
        ++line;
        if (!MultiByteToWideChar(CP_UTF8, 0, cbuf, -1, wbuf, _countof(wbuf))) {
            syserr(L"MultiByteToWideChar line %ld", line);
            break;
        }
        for (i = 0; iswspace(wbuf[i]); i++)
            ;
        if (wbuf[i] == L'#')
            continue;

        cur.modifiers = 0;
        for (; wbuf[i] != L'\t' && wbuf[i]; i++) {
            switch (towupper(wbuf[i])) {
            case L'C': cur.modifiers |= MOD_CONTROL; break;
            case L'A': cur.modifiers |= MOD_ALT; break;
            case L'S': cur.modifiers |= MOD_SHIFT; break;
            case L'W': cur.modifiers |= MOD_WIN; break;
            default:
                if (errors++ == 0)
                    syserr(L"Unknown modifier '%c' on line %ld", wbuf[i], line);
                break;
            }
        }
        i++;
        cur.hotkey = toupper((char)wbuf[i]);
        if (cur.hotkey == '~') {
            cur.hotkey = 0;
            i++;
            for (; wbuf[i] != L'\t' && wbuf[i]; i++) {
                if ((wbuf[i] - 0x30) < 0 || (wbuf[i] - 0x30) > 9) {
                    cur.hotkey = 0;
                    break;
                }
                cur.hotkey *= 10;
                cur.hotkey += (wbuf[i] - 0x30);
            }
        }
        if (!cur.modifiers || cur.hotkey < 32)
            continue;
        for (; wbuf[i] != '\t' && wbuf[i]; i++)
            ;
        for (; wbuf[i] == '\t'; i++)
            ;
        if (!wbuf[i])
            continue;
        cur.exename = wbuf + i;
        for (; wbuf[i] != '\t' && wbuf[i]; i++)
            ;
        cur.params = L"";
        cur.workdir = L"";
        if (wbuf[i]) {
            wbuf[i] = 0;
            while (wbuf[++i] == '\t')
                ;
            cur.workdir = wbuf + i;
            for (; wbuf[i] != '\t' && wbuf[i]; i++)
                ;
            if (wbuf[i]) {
                wbuf[i] = 0;
                while (wbuf[++i] == '\t')
                    ;
                cur.params = wbuf + i;
            }
        }
        entry = calloc(sizeof(CONFIG_ENTRY), 1);
        if (!entry)
            break;
        entry->hotkey = cur.hotkey;
        entry->modifiers = cur.modifiers;
        entry->exename = searchPath(expandStr(cur.exename), path);
        entry->params = expandStr(cur.params);
        entry->workdir = expandStr(cur.workdir);
        entry->prev = prev;
        if (prev)
            prev->next = entry;
        else
            cfg = entry;
        prev = entry;
    }
    fclose(f);
    return cfg;
}

void registerHooks()
{
    CONFIG_ENTRY *entry;
    int i;
    for (i = 0, entry = gConfig; entry; i++, entry = entry->next) {
        if (!RegisterHotKey(NULL, i, entry->modifiers, entry->hotkey)) {
            wchar_t buf[128] = { 0 };
            if (entry->modifiers & MOD_CONTROL)
                wcscat_s(buf, _countof(buf), L"Ctrl-");
            if (entry->modifiers & MOD_ALT)
                wcscat_s(buf, _countof(buf), L"Alt-");
            if (entry->modifiers & MOD_SHIFT)
                wcscat_s(buf, _countof(buf), L"Shift-");
            if (entry->modifiers & MOD_WIN)
                wcscat_s(buf, _countof(buf), L"Win-");
            syserr(L"RegisterHotKey('%s%c') failed", buf, entry->hotkey);
            entry->status = GetLastError();
        } else {
            entry->status = 0;
        }
    }
}

void unregisterHooks()
{
    CONFIG_ENTRY *entry;
    int i;
    for (i = 0, entry = gConfig; entry; i++, entry = entry->next) {
        UnregisterHotKey(NULL, i);
    }
}

int init() {
    gConfig = loadConfig(gsConfigFile);
    if (!gConfig) {
        syserr(L"Unable to read \"%s\". Exiting.", gsConfigFile);
        return 0;
    }
    registerHooks();
    return 1;
}

void deinit() {
    CONFIG_ENTRY *entry;
    if (gConfig) {
        unregisterHooks();
        while (gConfig) {
            entry = gConfig;
            gConfig = gConfig->next;
            if (entry->exename) free(entry->exename);
            if (entry->params) free(entry->params);
            if (entry->workdir) free(entry->workdir);
            free(entry);
        }
    }
}
LPWSTR loadString(UINT id)
{
    LPWSTR ptr = NULL;
    size_t len = max(0, LoadStringW(ghInst, id, (LPWSTR)& ptr, 0));
    LPWSTR res = malloc((len + 1) * sizeof(wchar_t));
    if (res) {
        memcpy(res, ptr, len * sizeof(wchar_t));
        res[len] = 0;
    }
    return res;
}

DWORD fileTimeTag(LPWSTR fname)
{
    FILETIME mtime = {0};
    HANDLE h = CreateFileW(fname, FILE_READ_ATTRIBUTES, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h != INVALID_HANDLE_VALUE) {
        GetFileTime(h, NULL, NULL, &mtime);
        CloseHandle(h);
    }
    return mtime.dwLowDateTime ^ mtime.dwHighDateTime;
}

DWORD WINAPI ReconfigThread(LPVOID param)
{
    wchar_t dir[1024];
    LPWSTR pfile = NULL;
    DWORD mtime = fileTimeTag(gsConfigFile);
    if (!GetFullPathNameW(gsConfigFile, _countof(dir), dir, &pfile) || !pfile) {
        syserr(L"Config file read error: %s", gsConfigFile);
        return 1;
    }
    pfile[0] = 0;
    for (;;) {
        HANDLE hNotify = FindFirstChangeNotificationW(dir, FALSE, FILE_NOTIFY_CHANGE_LAST_WRITE);
        HANDLE handles[] = { ghEventExit, hNotify };
        if (hNotify == INVALID_HANDLE_VALUE) {
            syserr(L"Notify %s", dir);
            break;
        }
        DWORD rc = WaitForMultipleObjects(2, handles, FALSE, INFINITE);
        if (rc != 1)
            break;
        DWORD mtime2 = fileTimeTag(gsConfigFile);
        if (mtime2 != mtime) {
            for (int i = 0; i < 25; ++i) {
                Sleep(200);
                if ((mtime = fileTimeTag(gsConfigFile)) != 0)
                    break;
            }
            PostMessageW(ghWnd, wmReload, 0, 0);
        }
        FindCloseChangeNotification(hNotify);
    }
    PostQuitMessage(1);
    return 0;
}

int WINAPI wWinMain(
    _In_ HINSTANCE hInstance,
    _In_opt_ HINSTANCE hPrevInstance,
    _In_ LPWSTR lpCmdLine,
    _In_ int nShowCmd)
{
    MSG msg;
    CONFIG_ENTRY *entry;
    ghInst = hInstance;
    gsConfigFile = L"myhotkey.conf";
    gsAppName = loadString(IDS_APPNAME);
    gsMsgReloaded = loadString(IDS_MSG_RELOADED);
    if (lpCmdLine && lpCmdLine[0]) {
        gsConfigFile = lpCmdLine;
    }
    ghEventExit = CreateEventW(NULL, TRUE, FALSE, L"Global\\myhotkey_exit");
    if (!ghEventExit) {
        syserr(L"CreateEvent");
        return 1;
    }
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        CloseHandle(ghEventExit);
        return 1; // already running - exit
    }
    if (!init()) {
        CloseHandle(ghEventExit);
        return 1;
    }
    WNDCLASSW c;
    c.style = 0;
    c.lpfnWndProc = DefWindowProc;
    c.cbClsExtra = 0;
    c.cbWndExtra = 0;
    c.hInstance = hInstance;
    c.hIcon = LoadIcon(ghInst, MAKEINTRESOURCE(IDI_ICON1));
    c.hCursor = 0;
    c.hbrBackground = 0;
    c.lpszMenuName = NULL;
    c.lpszClassName = L"myhotkey_window";
    RegisterClassW(&c);
    ghWnd = CreateWindowExW(0, c.lpszClassName, gsAppName, 0, 0, 0, 1, 1, NULL, NULL, hInstance, NULL);
    if (!ghWnd) {
        syserr(L"CreateWindow");
        deinit();
        CloseHandle(ghEventExit);
        return 1;
    }
    if (CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE) > S_FALSE) {
        syserr(L"CoInitialize");
        deinit();
        CloseHandle(ghEventExit);
        return 1;
    }
    NOTIFYICONDATAW tooltip = { 0 };
    int i;
    BOOL tooltipShown = FALSE;
    tooltip.cbSize = sizeof(tooltip);
    tooltip.hWnd = ghWnd;
    tooltip.uID = 1;
    tooltip.hIcon = c.hIcon;
    tooltip.uFlags = NIF_ICON | NIF_INFO;
    tooltip.dwInfoFlags = NIIF_INFO;
    tooltip.uTimeout = 3000;
    wcscpy_s(tooltip.szInfo, _countof(tooltip.szInfo), gsMsgReloaded);
    wcscpy_s(tooltip.szInfoTitle, _countof(tooltip.szInfoTitle), gsAppName);
    HANDLE hReconfigureThread = CreateThread(NULL, 0, &ReconfigThread, NULL, 0, NULL);
    if (!hReconfigureThread) {
        syserr(L"CreateThread");
        deinit();
        CloseHandle(ghEventExit);
        return 1;
    }
    while (GetMessageW(&msg, NULL, 0, 0)) {
        if (TranslateMessage(&msg)) continue;
        switch (msg.message) {
        case WM_HOTKEY:
            for (i=0, entry = gConfig; entry; i++, entry = entry->next) {
                if (msg.wParam == i) {
                    if (entry->exename && !wcscmp(L"<reload>", entry->exename)) {
                        PostMessageW(ghWnd, wmReload, 0, 0);
                    } else {
                        start(entry);
                    }
                    break;
                }
            }
            break;
        case wmReload:
            deinit();
            if (!init())
                PostQuitMessage(1);
            if (!tooltipShown) {
                tooltipShown = TRUE;
                Shell_NotifyIconW(NIM_ADD, &tooltip);
                SetTimer(ghWnd, 1, tooltip.uTimeout, NULL);
            }
            break;
        case WM_TIMER:
            KillTimer(ghWnd, 1);
            Shell_NotifyIconW(NIM_DELETE, &tooltip);
            tooltipShown = FALSE;
            break;
        }
    }
    SetEvent(ghEventExit);
    WaitForSingleObject(hReconfigureThread, 100);
    deinit();
    CloseHandle(ghEventExit);
    return 0;
}
