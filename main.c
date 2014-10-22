/* rg@z */

/*
TODO:

. optimize logging
    - hash up process/window names for faster logging
    - do custom FormatMessage

. explore SetWinEventHook, SetWindowsHookEx to avoid using timer

*/

/*

[+00:00:00.000] 2010-07-13 18:31:36.038 {rg@rg-0} log start

[+00:00:00.000] 2010-07-13 18:31:36.038 |rg@rg-0| (firefox.exe) WM_ACTIVATE Message (Windows) - Mozilla Firefox
[+00:01:46.361] 2010-07-13 18:33:22.399 |rg@rg-0| (vim.exe) main.c + (e:\progs\tlog) - VIM9

*/


#include <windows.h>
#include <psapi.h>
#include <wtsapi32.h>

#if 0
#include <strsafe.h> /* StringCbPrintfA */
#endif

#define WINDOW_NAME     L"tlog"
#define LOGFILE_NAME    L"tlog.log"

#define TIMER_FREQ  (500)

#define WINDOWTITLE_MAXLEN  (256)
#define LOGBUF_SIZE         (512)

#define IDLE_TIME_THRESHOLD (2*60*1000) /* in milliseconds */

enum tlog_Reason {
    Init, Exit, Timer, Idle, Logon, Logoff, Lock, Unlock, SessionChange, QueryEndSession, Shutdown
};

int tlog(HWND, enum tlog_Reason);


LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);


int WINAPI
    WinMain (
        HINSTANCE   hInstance,
        HINSTANCE   hPrevInstance,
        LPSTR       pCmdLine,
        int         nCmdShow
)
{
    HWND        hW;
    WNDCLASSEX  wcx;

    ZeroMemory(&wcx, sizeof(wcx));
    wcx.cbSize          = sizeof(WNDCLASSEX);
    wcx.lpfnWndProc     = WindowProc;
    wcx.lpszClassName   = WINDOW_NAME;

    RegisterClassEx(&wcx);

    if (hW = CreateWindow(WINDOW_NAME, WINDOW_NAME, 0, 0, 0, 0, 0, 0, 0, 0, 0)) { /* a faceless window */

        tlog(hW, Init);

        { /* the message loop */
            MSG     msg;
            BOOL    ret;

            while(ret = GetMessage(&msg, NULL, 0, 0)) {

                if (ret == -1) {
                    OutputDebugString(L"GetMessage failed.\n");
                    return -1;
                }

                DispatchMessage(&msg);
            }
        }

        tlog(hW, Exit);

    } else {

        OutputDebugString(L"CreateWindow failed.\n");
        return -1;

    }

    return 0;
}

LRESULT CALLBACK WindowProc(
    HWND    hWnd,
    UINT    uMsg,
    WPARAM  wParam,
    LPARAM  lParam
)
{
    switch(uMsg) {
        case WM_CLOSE:
        case WM_DESTROY:
            PostQuitMessage(0);
            break;
        case WM_TIMER:
            tlog(hWnd, Timer);
            break;
        case WM_WTSSESSION_CHANGE:
            switch (wParam) {
                case WTS_SESSION_LOGON:
                    tlog(hWnd, Logon);
                    break;
                case WTS_SESSION_LOGOFF:
                    tlog(hWnd, Logoff);
                    break;
                case WTS_SESSION_LOCK:
                    tlog(hWnd, Lock);
                    break;
                case WTS_SESSION_UNLOCK:
                    tlog(hWnd, Unlock);
                    break;
                default:
                    tlog(hWnd, SessionChange);
                    break;
            }
            break;
        case WM_QUERYENDSESSION:
            tlog(hWnd, QueryEndSession);
            break;
        case WM_ENDSESSION:
            tlog(hWnd, Shutdown);
            PostQuitMessage(0);
            break;
        default:
            return DefWindowProc(hWnd, uMsg, wParam, lParam);
    }

    return 0;
}


int
    tlog_log (
        HANDLE          hF,
        SYSTEMTIME *    time,
        char *          source,
        char *          message
)
{
    static char logbuf[LOGBUF_SIZE];
    int         loglen;

    static SYSTEMTIME   time_prev = {0};

    static const char * formatstr   =
                            "%1!4.4d!-%2!2.2d!-%3!2.2d!% %4!2.2d!:%5!2.2d!:%6!2.2d!%.%7!3.3d! (%8!s!) %9!s!\n";

    DWORD_PTR           formatarg [] = {
                            time->wYear, time->wMonth, time->wDay, time->wHour, time->wMinute, time->wSecond,
                            time->wMilliseconds, (DWORD_PTR)source, (DWORD_PTR)message
                        };

    /* TODO: compute difference between time_prev and time; time_prev = time; */

    if (loglen = FormatMessageA(FORMAT_MESSAGE_FROM_STRING | FORMAT_MESSAGE_ARGUMENT_ARRAY,
                formatstr, 0, 0, logbuf, sizeof(logbuf), (va_list *)formatarg)) {
        DWORD written;

        OutputDebugStringA(logbuf);

        if (!WriteFile(hF, logbuf, loglen, &written, NULL)) {
            OutputDebugString(L"WriteFile failed.\n");
            return -1;
        }

    } else {
        OutputDebugString(L"FormatMessage message failed!\n");
        return -2;
    }

    return 0;
}



int
    tlog (
        HWND                hW_main,
        enum tlog_Reason    reason
)
{
    static enum {Running, Idle}     state = Running;
    static enum {Enabled, Disabled} timer = Enabled;

    static HANDLE   hF;
    SYSTEMTIME      time;
    static int      len_prev = -1; /* can be set to -1 to force a log next timer tick */

    if (reason == Timer) {

        HWND            hW;
        static HWND     hW_prev = NULL;

        static char *   image;

        if (Timer == Disabled) {
            return 0;
        }

        if (hW = GetForegroundWindow( )) {

            static char     title0[WINDOWTITLE_MAXLEN];
            static char     title1[WINDOWTITLE_MAXLEN];
            static char *   title       = title0;
            static char *   title_prev  = NULL;
            int             len;

            len = GetWindowTextA(hW, title, WINDOWTITLE_MAXLEN);

            if ((len != len_prev) || strncmp(title, title_prev, len)) {

                HANDLE  hP;

                GetLocalTime(&time);

                if (hW != hW_prev) { /* don't look-up process name if window hasn't changed (ex: browser tabs) */

                    DWORD   pid;

                    GetWindowThreadProcessId(hW, &pid);

                    if (hP = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pid)) {

                        static char     imagename[MAX_PATH];

                        image = imagename + GetProcessImageFileNameA(hP, imagename, sizeof(imagename));

                        if (image != imagename) {

                            while ((*--image != '\\') && (image != imagename));
                            if (image != imagename) {
                                ++image;
                            }

                        } else {

                            image = "-unknown-";
                            OutputDebugString(L"GetProcessImageFileName failed.\n");
                        }

                        CloseHandle(hP);

                    } else {
                        image = "-unknown-";
                        OutputDebugString(L"OpenProcess failed.\n");
                    }

                    hW_prev = hW;
                }

                tlog_log(hF, &time, image, title);

                title_prev   = title;
                len_prev     = len;
                title = (title == title0) ? title1 : title0;

                if (state == Idle) {
                    state = Running;
                }

            } else {

                LASTINPUTINFO   lii = {sizeof(lii)};
                DWORD           tick;
                DWORD           idle;

                if (GetLastInputInfo(&lii)) {

                    tick = GetTickCount( );
                    idle = (tick >= lii.dwTime) ? (tick - lii.dwTime) : (0xFFFFFFFF - lii.dwTime + tick);

#if 0 /* idle debug */
                    {
                        char buf[100];
                        StringCbPrintfA(buf, sizeof(buf), "lii=%d; tick=%d; idle=%d; threshold=%d; state=%s\n",
                            lii.dwTime, tick, idle, IDLE_TIME_THRESHOLD, (state == Idle) ? "Idle" : "Running");
                        OutputDebugStringA(buf);
                    }
#endif


                    if (idle > IDLE_TIME_THRESHOLD) {

                        if (state != Idle) {

                            GetLocalTime(&time);
                            state = Idle;
                            tlog_log(hF, &time, "-tlog-", "idle ..");
 
                        }

                    } else if (state == Idle) {

                        state = Running;
                        len_prev = -1; /* re-log foreground window */

                    }
                }
            }
        }

        SetTimer(hW_main, 1, TIMER_FREQ, NULL);

    } else {

        char *  source;
        char *  message;

        GetLocalTime(&time);

        source  = "-system-";

        switch (reason) {
            case Lock:
                message = "locked screen";
                timer   = Disabled;
                break;
            case Unlock:
                message = "unlocked screen";
                timer   = Enabled;
                break;
            case Logon:
                message = "logon";
                timer   = Enabled;
                break;
            case Logoff:
                message = "logoff";
                timer   = Disabled;
                break;
            case Shutdown:
                message = "shutdown (endsession)";
                timer   = Disabled;
                break;
            case QueryEndSession:
                message = "pre-shutdown (queryendsession)";
                timer   = Enabled;
                break;
            case SessionChange:
                message = "unknown session change event";
                timer   = Enabled;
                break;
            case Init:
                {
                    hF = CreateFile(LOGFILE_NAME, FILE_APPEND_DATA | SYNCHRONIZE, FILE_SHARE_READ | FILE_SHARE_WRITE,
                            NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);

                    if (hF == INVALID_HANDLE_VALUE) {
                        OutputDebugString(L"CreateFile failed.\n");
                        return -1;
                    }

                    /* SetFilePointer(hF, 0, NULL, FILE_END); */


                    if (!WTSRegisterSessionNotification(hW_main, NOTIFY_FOR_THIS_SESSION)) {
                        OutputDebugString(L"WTSRegisterSessionNotification failed.\n");
                    }

                    source  = "-tlog-";
                    message = "started";
                    timer   = Enabled;
                }
                break;
            case Exit:

                CloseHandle(hF);

                source  = "-tlog-";
                message = "exit ..";
                timer   = Disabled;

                break;
            default:
                source  = "-system-";
                message = "unknown event";
                timer   = Enabled;
                break;
        }

        if (timer == Enabled) {
            SetTimer(hW_main, 1, TIMER_FREQ, NULL);
            len_prev = -1; /* re-log foreground window */
        }

        tlog_log(hF, &time, source, message);

    }

    return 0;
}

