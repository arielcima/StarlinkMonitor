#include <windows.h>
#include <shellapi.h>
#include <wininet.h>
#include <winsock2.h>
#include <iphlpapi.h>
#include <icmpapi.h>

#pragma comment(lib, "wininet.lib")
#pragma comment(lib, "iphlpapi.lib")
#pragma comment(lib, "ws2_32.lib")

#define WM_TRAYICON (WM_USER + 1)
#define TIMER_ID 1
#define CHECK_INTERVAL 1000

#define FAIL_THRESHOLD 3
#define SUCCESS_THRESHOLD 2

NOTIFYICONDATA nid;
HICON hGreen;
HICON hRed;

int consecutiveFails = 0;
int consecutiveSuccess = 0;
BOOL isOnline = FALSE;

BOOL CheckInternet()
{
    HANDLE hIcmpFile;
    unsigned long ipaddr = inet_addr("8.8.8.8");
    DWORD dwRetVal = 0;
    char SendData[32] = "Data Buffer";
    LPVOID ReplyBuffer = NULL;
    DWORD ReplySize = 0;

    hIcmpFile = IcmpCreateFile();
    if (hIcmpFile == INVALID_HANDLE_VALUE) {
        return FALSE;
    }

    ReplySize = sizeof(ICMP_ECHO_REPLY) + sizeof(SendData);
    ReplyBuffer = (VOID*)malloc(ReplySize);
    if (ReplyBuffer == NULL) {
        IcmpCloseHandle(hIcmpFile);
        return FALSE;
    }

    dwRetVal = IcmpSendEcho(hIcmpFile, ipaddr, SendData, sizeof(SendData),
        NULL, ReplyBuffer, ReplySize, 1000);

    BOOL success = (dwRetVal != 0);

    free(ReplyBuffer);
    IcmpCloseHandle(hIcmpFile);

    return success;
}

void SetTrayIcon(BOOL online)
{
    nid.hIcon = online ? hGreen : hRed;
    Shell_NotifyIcon(NIM_MODIFY, &nid);
}

void UpdateStatus()
{
    if (CheckInternet())
    {
        consecutiveSuccess++;
        consecutiveFails = 0;

        if (!isOnline && consecutiveSuccess >= SUCCESS_THRESHOLD)
        {
            isOnline = TRUE;
            SetTrayIcon(TRUE);
        }
    }
    else
    {
        consecutiveFails++;
        consecutiveSuccess = 0;

        if (isOnline && consecutiveFails >= FAIL_THRESHOLD)
        {
            isOnline = FALSE;
            SetTrayIcon(FALSE);
        }
    }
}

void ShowContextMenu(HWND hwnd)
{
    POINT pt;
    GetCursorPos(&pt);

    HMENU hMenu = CreatePopupMenu();
    InsertMenu(hMenu, -1, MF_BYPOSITION, 1, "Exit");

    SetForegroundWindow(hwnd);
    int cmd = TrackPopupMenu(
        hMenu,
        TPM_RETURNCMD | TPM_NONOTIFY,
        pt.x,
        pt.y,
        0,
        hwnd,
        NULL);

    if (cmd == 1)
        PostQuitMessage(0);

    DestroyMenu(hMenu);
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_CREATE:
        SetTimer(hwnd, TIMER_ID, CHECK_INTERVAL, NULL);
        break;

    case WM_TIMER:
        UpdateStatus();
        break;

    case WM_TRAYICON:
        if (lParam == WM_RBUTTONUP)
            ShowContextMenu(hwnd);
        break;

    case WM_DESTROY:
        Shell_NotifyIcon(NIM_DELETE, &nid);
        KillTimer(hwnd, TIMER_ID);
        PostQuitMessage(0);
        break;
    }

    return DefWindowProc(hwnd, msg, wParam, lParam);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
                   LPSTR lpCmdLine, int nCmdShow)
{
    WNDCLASS wc = {0};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = "NetMonitorClass";
    RegisterClass(&wc);

    HWND hwnd = CreateWindow(
        "NetMonitorClass",
        "NetMonitor",
        0,
        0, 0, 0, 0,
        NULL, NULL, hInstance, NULL);

    // Replace with custom 16x16 circle icons for best result
    // Load icons from embedded resources
    hGreen = LoadIcon(hInstance, "ID_ICON_GREEN");
    hRed   = LoadIcon(hInstance, "ID_ICON_RED");

    ZeroMemory(&nid, sizeof(nid));
    nid.cbSize = sizeof(NOTIFYICONDATA);
    nid.hWnd = hwnd;
    nid.uID = 1;
    nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    nid.uCallbackMessage = WM_TRAYICON;
    nid.hIcon = hRed;
    lstrcpy(nid.szTip, "Internet Status");

    Shell_NotifyIcon(NIM_ADD, &nid);

    // Initial state check
    isOnline = CheckInternet();
    SetTrayIcon(isOnline);

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return 0;
}
