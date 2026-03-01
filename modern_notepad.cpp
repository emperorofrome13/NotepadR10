#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commctrl.h>
#include <commdlg.h>
#include <dwmapi.h>
#include <string>
#include <vector>
#include <sstream>
#include <algorithm>

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "uxtheme.lib")
#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "comdlg32.lib")

// --- Constants ---
const wchar_t CLASS_NAME[] = L"ModernNotepadClass";
const int TAB_WIDTH = 180;
const int TAB_HEIGHT = 35;
const int STATUS_HEIGHT = 30;
const int TOOLBAR_HEIGHT = 40;

#ifndef DWMWA_USE_IMMERSIVE_DARK_MODE
#define DWMWA_USE_IMMERSIVE_DARK_MODE 20
#endif

// --- Structures ---
struct Tab {
    int id;
    std::wstring title;
    std::wstring content;
    std::wstring path;
};

// --- Global State ---
std::vector<Tab> tabs;
int activeTabId = -1;
int tabCounter = 0;
HWND hMainWindow = NULL;
HWND hEdit = NULL;
HWND hStatusBar = NULL;
HWND hToolbar = NULL;
HFONT hTabFont = NULL;
HFONT hEditFont = NULL;
HBRUSH hEditBgBrush = NULL;

// --- Rename Dialog State ---
int gRenameTabId = -1;
wchar_t gRenameBuffer[256] = L"";

// --- Function Declarations ---
LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK RenameDialogProc(HWND, UINT, WPARAM, LPARAM);
void CreateControls(HWND hWnd);
void UpdateStats();
void NewTab();
void SwitchTab(int id);
void CloseTab(int id);
void RenameTab(int id);
void RenderTabs(HDC hdc);
void SaveFile();
void OpenFile();
std::wstring GetOpenFileName(HWND hWnd);
std::wstring GetSaveFileName(HWND hWnd, const std::wstring& defaultName);
void EnableDarkMode(HWND hWnd);
int CountWords(const std::wstring& text);
int CountTokens(const std::wstring& text);
LRESULT CALLBACK EditSubclassProc(HWND, UINT, WPARAM, LPARAM, UINT_PTR, DWORD_PTR);
LPWORD wszDup(const wchar_t* str, LPWORD lpw);

// --- Helper function for dialog template ---
LPWORD wszDup(const wchar_t* str, LPWORD lpw) {
    while (*str) {
        *lpw++ = *str++;
    }
    *lpw++ = 0;
    return lpw;
}

// --- Entry Point ---
int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR, int nCmdShow) {
    INITCOMMONCONTROLSEX icex = {};
    icex.dwSize = sizeof(icex);
    icex.dwICC = ICC_STANDARD_CLASSES | ICC_BAR_CLASSES;
    InitCommonControlsEx(&icex);

    WNDCLASSW wc = {};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    RegisterClassW(&wc);

    hMainWindow = CreateWindowExW(
        0, CLASS_NAME, L"Modern Notepad",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 1200, 800,
        NULL, NULL, hInstance, NULL
    );

    if (!hMainWindow) return 0;

    hEditBgBrush = CreateSolidBrush(RGB(45, 45, 45));

    EnableDarkMode(hMainWindow);
    ShowWindow(hMainWindow, nCmdShow);
    UpdateWindow(hMainWindow);

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return 0;
}

// --- Window Procedure ---
LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_CREATE:
        CreateControls(hWnd);
        NewTab();
        break;

    case WM_SIZE: {
        RECT rc;
        GetClientRect(hWnd, &rc);
        int editLeft = TAB_WIDTH;
        int editTop = TOOLBAR_HEIGHT;
        int editRight = rc.right;
        int editBottom = rc.bottom - STATUS_HEIGHT;

        MoveWindow(hToolbar, 0, 0, rc.right, TOOLBAR_HEIGHT, TRUE);
        MoveWindow(hEdit, editLeft, editTop, editRight - editLeft, editBottom - editTop, TRUE);
        MoveWindow(hStatusBar, 0, rc.bottom - STATUS_HEIGHT, rc.right, STATUS_HEIGHT, TRUE);
        InvalidateRect(hWnd, NULL, TRUE);
        break;
    }

    case WM_COMMAND:
        if (HIWORD(wParam) == BN_CLICKED || HIWORD(wParam) == 0) {
            if (LOWORD(wParam) == 1001) NewTab();
            else if (LOWORD(wParam) == 1002) OpenFile();
            else if (LOWORD(wParam) == 1003) SaveFile();
        }
        break;

    case WM_CTLCOLOREDIT: {
        HWND hCtrl = (HWND)lParam;
        if (hCtrl == hEdit) {
            HDC hdc = (HDC)wParam;
            SetBkColor(hdc, RGB(45, 45, 45));
            SetTextColor(hdc, RGB(255, 255, 255));
            return (LRESULT)hEditBgBrush;
        }
        break;
    }

    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hWnd, &ps);
        
        RECT rc;
        GetClientRect(hWnd, &rc);
        
        HBRUSH darkBrush = CreateSolidBrush(RGB(32, 32, 32));
        FillRect(hdc, &rc, darkBrush);
        DeleteObject(darkBrush);

        RECT tabRect = {0, TOOLBAR_HEIGHT, TAB_WIDTH, rc.bottom - STATUS_HEIGHT};
        HBRUSH tabBrush = CreateSolidBrush(RGB(32, 32, 32));
        FillRect(hdc, &tabRect, tabBrush);
        DeleteObject(tabBrush);

        RenderTabs(hdc);

        HPEN pen = CreatePen(PS_SOLID, 1, RGB(56, 56, 56));
        HPEN oldPen = (HPEN)SelectObject(hdc, pen);
        MoveToEx(hdc, TAB_WIDTH, TOOLBAR_HEIGHT, NULL);
        LineTo(hdc, TAB_WIDTH, rc.bottom - STATUS_HEIGHT);
        SelectObject(hdc, oldPen);
        DeleteObject(pen);

        EndPaint(hWnd, &ps);
        break;
    }

    case WM_LBUTTONDOWN: {
        RECT rc;
        GetClientRect(hWnd, &rc);
        
        int x = LOWORD(lParam);
        int y = HIWORD(lParam);
        
        if (x < TAB_WIDTH && y > TOOLBAR_HEIGHT && y < (rc.bottom - STATUS_HEIGHT)) {
            int relativeY = y - TOOLBAR_HEIGHT;
            int tabIndex = relativeY / TAB_HEIGHT;
            
            if (tabIndex >= 0 && tabIndex < static_cast<int>(tabs.size())) {
                if (x >= TAB_WIDTH - 25) {
                    CloseTab(tabs[tabIndex].id);
                } else {
                    SwitchTab(tabs[tabIndex].id);
                }
            }
        }
        break;
    }

    case WM_RBUTTONDOWN: {
        RECT rc;
        GetClientRect(hWnd, &rc);
        
        int x = LOWORD(lParam);
        int y = HIWORD(lParam);
        
        if (x < TAB_WIDTH && y > TOOLBAR_HEIGHT && y < (rc.bottom - STATUS_HEIGHT)) {
            int relativeY = y - TOOLBAR_HEIGHT;
            int tabIndex = relativeY / TAB_HEIGHT;
            
            if (tabIndex >= 0 && tabIndex < static_cast<int>(tabs.size())) {
                SwitchTab(tabs[tabIndex].id);
                RenameTab(tabs[tabIndex].id);
            }
        }
        break;
    }

    case WM_CLOSE:
        DestroyWindow(hWnd);
        break;

    case WM_DESTROY:
        RemoveWindowSubclass(hEdit, EditSubclassProc, 1);
        if (hTabFont) DeleteObject(hTabFont);
        if (hEditFont) DeleteObject(hEditFont);
        if (hEditBgBrush) DeleteObject(hEditBgBrush);
        PostQuitMessage(0);
        break;

    default:
        return DefWindowProcW(hWnd, msg, wParam, lParam);
    }
    return 0;
}

// --- Control Creation ---
void CreateControls(HWND hWnd) {
    hToolbar = CreateWindowExW(0, L"STATIC", L"",
        WS_CHILD | WS_VISIBLE,
        0, 0, 0, TOOLBAR_HEIGHT,
        hWnd, NULL, GetModuleHandle(NULL), NULL);

    CreateWindowW(L"BUTTON", L"+ New",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        10, 8, 60, 25, hWnd, (HMENU)1001, GetModuleHandle(NULL), NULL);
    
    CreateWindowW(L"BUTTON", L"Open",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        80, 8, 60, 25, hWnd, (HMENU)1002, GetModuleHandle(NULL), NULL);
    
    CreateWindowW(L"BUTTON", L"Save",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        150, 8, 60, 25, hWnd, (HMENU)1003, GetModuleHandle(NULL), NULL);

    hEdit = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
        WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_AUTOVSCROLL | ES_AUTOHSCROLL | 
        WS_VSCROLL | ES_WANTRETURN | ES_LEFT | WS_TABSTOP,
        0, 0, 0, 0,
        hWnd, NULL, GetModuleHandle(NULL), NULL);

    hStatusBar = CreateWindowExW(0, STATUSCLASSNAMEW, L"",
        WS_CHILD | WS_VISIBLE | SBARS_SIZEGRIP,
        0, 0, 0, STATUS_HEIGHT,
        hWnd, NULL, GetModuleHandle(NULL), NULL);

    hTabFont = CreateFontW(14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");

    hEditFont = CreateFontW(15, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, FIXED_PITCH | FF_MODERN, L"Consolas");

    SendMessageW(hEdit, WM_SETFONT, (WPARAM)hEditFont, TRUE);
    
    SetWindowSubclass(hEdit, EditSubclassProc, 1, 0);
}

// --- Tab Logic ---
void NewTab() {
    tabCounter++;
    Tab newTab;
    newTab.id = tabCounter;
    newTab.title = L"Untitled " + std::to_wstring(tabCounter);
    newTab.content = L"";
    newTab.path = L"";
    tabs.push_back(newTab);
    SwitchTab(newTab.id);
}

void SwitchTab(int id) {
    for (auto& t : tabs) {
        if (t.id == activeTabId) {
            int len = GetWindowTextLengthW(hEdit);
            if (len > 0) {
                std::vector<wchar_t> buffer(len + 1);
                GetWindowTextW(hEdit, buffer.data(), len + 1);
                t.content = buffer.data();
            } else {
                t.content.clear();
            }
            break;
        }
    }

    activeTabId = id;
    
    for (auto& t : tabs) {
        if (t.id == id) {
            SetWindowTextW(hEdit, t.content.c_str());
            break;
        }
    }
    
    InvalidateRect(hMainWindow, NULL, TRUE);
    UpdateStats();
    SetFocus(hEdit);
}

void CloseTab(int id) {
    if (tabs.size() == 1) {
        tabs[0].content.clear();
        tabs[0].title = L"Untitled 1";
        tabs[0].path.clear();
        SetWindowTextW(hEdit, L"");
        tabCounter = 1;
        activeTabId = tabs[0].id;
        InvalidateRect(hMainWindow, NULL, TRUE);
        UpdateStats();
        return;
    }

    auto it = std::find_if(tabs.begin(), tabs.end(), 
        [id](const Tab& t){ return t.id == id; });
    
    if (it != tabs.end()) {
        int idx = static_cast<int>(std::distance(tabs.begin(), it));
        tabs.erase(it);
        
        if (activeTabId == id) {
            int newIdx = (idx > 0) ? idx - 1 : 0;
            if (newIdx < static_cast<int>(tabs.size())) {
                SwitchTab(tabs[newIdx].id);
            }
        } else {
            InvalidateRect(hMainWindow, NULL, TRUE);
        }
    }
}

// --- Rename Dialog Procedure ---
INT_PTR CALLBACK RenameDialogProc(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_INITDIALOG: {
        // Set the current tab name in the edit control
        SetDlgItemTextW(hDlg, 1001, gRenameBuffer);
        
        // Center the dialog over the main window
        RECT rcDlg, rcParent;
        GetWindowRect(hDlg, &rcDlg);
        GetWindowRect(hMainWindow, &rcParent);
        int x = rcParent.left + (rcParent.right - rcParent.left - (rcDlg.right - rcDlg.left)) / 2;
        int y = rcParent.top + (rcParent.bottom - rcParent.top - (rcDlg.bottom - rcDlg.top)) / 2;
        SetWindowPos(hDlg, NULL, x, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
        
        // Focus on the edit control and select all text
        HWND hEditCtrl = GetDlgItem(hDlg, 1001);
        SetFocus(hEditCtrl);
        SendMessageW(hEditCtrl, EM_SETSEL, 0, -1);
        
        return FALSE; // We set focus manually
    }
    
    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case IDOK:
            // Get the new name from the edit control
            GetDlgItemTextW(hDlg, 1001, gRenameBuffer, 256);
            
            // Update the tab title if name is not empty
            if (wcslen(gRenameBuffer) > 0) {
                for (auto& t : tabs) {
                    if (t.id == gRenameTabId) {
                        t.title = gRenameBuffer;
                        // Note: We do NOT clear the path here - the user can rename
                        // the tab without affecting the file association
                        break;
                    }
                }
                InvalidateRect(hMainWindow, NULL, TRUE);
            }
            EndDialog(hDlg, IDOK);
            return TRUE;
            
        case IDCANCEL:
            EndDialog(hDlg, IDCANCEL);
            return TRUE;
        }
        break;
        
    case WM_CLOSE:
        EndDialog(hDlg, IDCANCEL);
        return TRUE;
    }
    return FALSE;
}

// --- Rename Tab Feature ---
void RenameTab(int id) {
    // Find the tab and get its current title
    Tab* tab = nullptr;
    for (auto& t : tabs) {
        if (t.id == id) {
            tab = &t;
            break;
        }
    }
    
    if (!tab) return;
    
    // Store the tab ID and current title for the dialog
    gRenameTabId = id;
    wcsncpy(gRenameBuffer, tab->title.c_str(), 255);
    gRenameBuffer[255] = L'\0';
    
    // Create the dialog template in memory
    HGLOBAL hgbl = GlobalAlloc(GMEM_ZEROINIT, 1024);
    if (!hgbl) return;
    
    LPDLGTEMPLATE lpdt = (LPDLGTEMPLATE)GlobalLock(hgbl);
    
    // Dialog template
    lpdt->style = WS_POPUP | WS_BORDER | WS_SYSMENU | WS_CAPTION | DS_MODALFRAME | DS_CENTER | DS_SETFONT;
    lpdt->dwExtendedStyle = WS_EX_DLGMODALFRAME;
    lpdt->cdit = 4; // 4 controls
    lpdt->x = 0; lpdt->y = 0;
    lpdt->cx = 186; lpdt->cy = 75;
    
    LPWORD lpw = (LPWORD)(lpdt + 1);
    *lpw++ = 0; // No menu
    *lpw++ = 0; // Default dialog class
    
    // Dialog title
    lpw = wszDup(L"Rename Tab", lpw);
    
    // Font
    *lpw++ = 9; // Point size
    lpw = wszDup(L"Segoe UI", lpw);
    
    // Align to DWORD for dialog items
    lpw = (LPWORD)(((ULONG_PTR)lpw + 3) & ~3);
    
    // --- Static control: "Enter new name:" ---
    LPDLGITEMTEMPLATE lpdi = (LPDLGITEMTEMPLATE)lpw;
    lpdi->style = WS_CHILD | WS_VISIBLE | SS_LEFT;
    lpdi->dwExtendedStyle = 0;
    lpdi->x = 7; lpdi->y = 7;
    lpdi->cx = 165; lpdi->cy = 12;
    lpdi->id = static_cast<WORD>(-1); // Static control ID
    
    lpw = (LPWORD)(lpdi + 1);
    *lpw++ = 0xFFFF; *lpw++ = 0x0082; // Static class
    lpw = wszDup(L"Enter new tab name:", lpw);
    *lpw++ = 0; // No creation data
    
    // Align to DWORD
    lpw = (LPWORD)(((ULONG_PTR)lpw + 3) & ~3);
    
    // --- Edit control ---
    lpdi = (LPDLGITEMTEMPLATE)lpw;
    lpdi->style = WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_BORDER | ES_AUTOHSCROLL;
    lpdi->dwExtendedStyle = WS_EX_CLIENTEDGE;
    lpdi->x = 7; lpdi->y = 22;
    lpdi->cx = 165; lpdi->cy = 14;
    lpdi->id = 1001; // Edit control ID
    
    lpw = (LPWORD)(lpdi + 1);
    *lpw++ = 0xFFFF; *lpw++ = 0x0081; // Edit class
    lpw = wszDup(L"", lpw); // Empty initial text (will be set in WM_INITDIALOG)
    *lpw++ = 0; // No creation data
    
    // Align to DWORD
    lpw = (LPWORD)(((ULONG_PTR)lpw + 3) & ~3);
    
    // --- OK Button ---
    lpdi = (LPDLGITEMTEMPLATE)lpw;
    lpdi->style = WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON;
    lpdi->dwExtendedStyle = 0;
    lpdi->x = 50; lpdi->y = 45;
    lpdi->cx = 50; lpdi->cy = 14;
    lpdi->id = IDOK;
    
    lpw = (LPWORD)(lpdi + 1);
    *lpw++ = 0xFFFF; *lpw++ = 0x0080; // Button class
    lpw = wszDup(L"OK", lpw);
    *lpw++ = 0;
    
    // Align to DWORD
    lpw = (LPWORD)(((ULONG_PTR)lpw + 3) & ~3);
    
    // --- Cancel Button ---
    lpdi = (LPDLGITEMTEMPLATE)lpw;
    lpdi->style = WS_CHILD | WS_VISIBLE | WS_TABSTOP;
    lpdi->dwExtendedStyle = 0;
    lpdi->x = 110; lpdi->y = 45;
    lpdi->cx = 50; lpdi->cy = 14;
    lpdi->id = IDCANCEL;
    
    lpw = (LPWORD)(lpdi + 1);
    *lpw++ = 0xFFFF; *lpw++ = 0x0080; // Button class
    lpw = wszDup(L"Cancel", lpw);
    *lpw++ = 0;
    
    GlobalUnlock(hgbl);
    
    // Show the dialog
    DialogBoxIndirectW(GetModuleHandle(NULL), lpdt, hMainWindow, RenameDialogProc);
    
    GlobalFree(hgbl);
}

void RenderTabs(HDC hdc) {
    SetBkMode(hdc, TRANSPARENT);
    SelectObject(hdc, hTabFont);

    for (size_t i = 0; i < tabs.size(); i++) {
        RECT tabRect = {
            0,
            static_cast<LONG>(TOOLBAR_HEIGHT + static_cast<int>(i) * TAB_HEIGHT),
            TAB_WIDTH,
            static_cast<LONG>(TOOLBAR_HEIGHT + (static_cast<int>(i) + 1) * TAB_HEIGHT)
        };

        bool isActive = (tabs[i].id == activeTabId);
        
        HBRUSH bgBrush = CreateSolidBrush(isActive ? RGB(42, 42, 42) : RGB(32, 32, 32));
        FillRect(hdc, &tabRect, bgBrush);
        DeleteObject(bgBrush);

        if (isActive) {
            HPEN borderPen = CreatePen(PS_SOLID, 3, RGB(96, 205, 255));
            HPEN oldPen = (HPEN)SelectObject(hdc, borderPen);
            MoveToEx(hdc, tabRect.left, tabRect.top, NULL);
            LineTo(hdc, tabRect.left, tabRect.bottom);
            SelectObject(hdc, oldPen);
            DeleteObject(borderPen);
        }

        SetTextColor(hdc, isActive ? RGB(255, 255, 255) : RGB(160, 160, 160));
        RECT textRect = tabRect;
        textRect.left += 15;
        textRect.right -= 30;
        DrawTextW(hdc, tabs[i].title.c_str(), -1, &textRect, 
            DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS | DT_VCENTER);

        RECT closeRect = {
            tabRect.right - 25, 
            tabRect.top + 8, 
            tabRect.right - 8, 
            tabRect.bottom - 8
        };
        SetTextColor(hdc, RGB(200, 200, 200));
        DrawTextW(hdc, L"\x00D7", -1, &closeRect, DT_CENTER | DT_SINGLELINE | DT_VCENTER);
    }
}

// --- Stats Logic ---
void UpdateStats() {
    int len = GetWindowTextLengthW(hEdit);
    std::wstring text;
    
    if (len > 0) {
        std::vector<wchar_t> buffer(len + 1);
        GetWindowTextW(hEdit, buffer.data(), len + 1);
        text = buffer.data();
    }

    int words = CountWords(text);
    int chars = static_cast<int>(text.length());
    int tokens = CountTokens(text);

    std::wstringstream ss;
    ss << L"Words: " << words << L"   |   Tokens: " << tokens << L"   |   Chars: " << chars;
    SendMessageW(hStatusBar, SB_SETTEXTW, 0, (LPARAM)ss.str().c_str());
}

int CountWords(const std::wstring& text) {
    if (text.empty()) return 0;
    std::wstringstream ss(text);
    std::wstring word;
    int count = 0;
    while (ss >> word) count++;
    return count;
}

int CountTokens(const std::wstring& text) {
    return (static_cast<int>(text.length()) + 3) / 4;
}

LRESULT CALLBACK EditSubclassProc(HWND hWnd, UINT msg, WPARAM wParam, 
                                   LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData) {
    if (msg == WM_KEYUP || msg == WM_CHAR || msg == WM_PASTE || msg == WM_CUT) {
        UpdateStats();
    }
    return DefSubclassProc(hWnd, msg, wParam, lParam);
}

// --- File Operations ---
void OpenFile() {
    std::wstring path = GetOpenFileName(hMainWindow);
    if (path.empty()) return;

    HANDLE hFile = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, 
                               NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) return;

    DWORD size = GetFileSize(hFile, NULL);
    if (size == INVALID_FILE_SIZE) {
        CloseHandle(hFile);
        return;
    }

    std::vector<char> byteBuffer(size + 2);
    DWORD bytesRead;
    ReadFile(hFile, byteBuffer.data(), size, &bytesRead, NULL);
    CloseHandle(hFile);
    
    byteBuffer[bytesRead] = 0;
    byteBuffer[bytesRead + 1] = 0;
    
    std::wstring content;
    if (bytesRead >= 2 && byteBuffer[0] == (char)0xFF && byteBuffer[1] == (char)0xFE) {
        content = std::wstring(reinterpret_cast<wchar_t*>(byteBuffer.data() + 2), 
                              (bytesRead - 2) / sizeof(wchar_t));
    } else {
        content = std::wstring(byteBuffer.data(), byteBuffer.data() + bytesRead);
    }

    std::wstring title = path;
    size_t pos = title.find_last_of(L"\\/");
    if (pos != std::wstring::npos) {
        title = title.substr(pos + 1);
    }

    Tab newTab;
    newTab.id = ++tabCounter;
    newTab.title = title;
    newTab.content = content;
    newTab.path = path;
    tabs.push_back(newTab);
    SwitchTab(newTab.id);
}

void SaveFile() {
    Tab* activeTab = nullptr;
    for (auto& t : tabs) {
        if (t.id == activeTabId) { 
            activeTab = &t; 
            break; 
        }
    }
    if (!activeTab) return;

    int len = GetWindowTextLengthW(hEdit);
    std::wstring content;
    
    if (len > 0) {
        std::vector<wchar_t> buffer(len + 1);
        GetWindowTextW(hEdit, buffer.data(), len + 1);
        content = buffer.data();
    }

    std::wstring path = activeTab->path;
    
    if (path.empty()) {
        path = GetSaveFileName(hMainWindow, activeTab->title);
        if (path.empty()) return;
        
        activeTab->path = path;
        activeTab->title = path.substr(path.find_last_of(L"\\/") + 1);
        InvalidateRect(hMainWindow, NULL, TRUE);
    }

    HANDLE hFile = CreateFileW(path.c_str(), GENERIC_WRITE, 0, NULL, 
                               CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) return;

    DWORD bytesWritten;
    WriteFile(hFile, content.c_str(), static_cast<DWORD>(content.length() * sizeof(wchar_t)), 
              &bytesWritten, NULL);
    CloseHandle(hFile);
}

std::wstring GetOpenFileName(HWND hWnd) {
    OPENFILENAMEW ofn = {};
    wchar_t szFile[MAX_PATH] = L"";
    
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = hWnd;
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrFilter = L"Text Files (*.txt)\0*.txt\0All Files\0*.*\0";
    ofn.nFilterIndex = 1;
    ofn.lpstrFileTitle = NULL;
    ofn.nMaxFileTitle = 0;
    ofn.lpstrInitialDir = NULL;
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;

    if (GetOpenFileNameW(&ofn)) {
        return szFile;
    }
    return L"";
}

std::wstring GetSaveFileName(HWND hWnd, const std::wstring& defaultName) {
    OPENFILENAMEW ofn = {};
    wchar_t szFile[MAX_PATH] = L"";
    
    if (!defaultName.empty()) {
        size_t len = defaultName.length();
        if (len >= MAX_PATH) len = MAX_PATH - 1;
        wcsncpy(szFile, defaultName.c_str(), len);
        szFile[len] = L'\0';
    }
    
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = hWnd;
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrFilter = L"Text Files (*.txt)\0*.txt\0All Files\0*.*\0";
    ofn.nFilterIndex = 1;
    ofn.lpstrFileTitle = NULL;
    ofn.nMaxFileTitle = 0;
    ofn.lpstrInitialDir = NULL;
    ofn.Flags = OFN_OVERWRITEPROMPT;

    if (GetSaveFileNameW(&ofn)) {
        return szFile;
    }
    return L"";
}

// --- Dark Mode ---
void EnableDarkMode(HWND hWnd) {
    BOOL darkMode = TRUE;
    
    HMODULE hDwmApi = LoadLibraryW(L"dwmapi.dll");
    if (hDwmApi) {
        typedef HRESULT (WINAPI *DwmSetWindowAttribute_t)(HWND, DWORD, LPCVOID, DWORD);
        auto pDwmSetWindowAttribute = reinterpret_cast<DwmSetWindowAttribute_t>(
            GetProcAddress(hDwmApi, "DwmSetWindowAttribute"));
        
        if (pDwmSetWindowAttribute) {
            pDwmSetWindowAttribute(hWnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &darkMode, sizeof(darkMode));
        }
        FreeLibrary(hDwmApi);
    }
    
    HMODULE hUxTheme = LoadLibraryW(L"uxtheme.dll");
    if (hUxTheme) {
        typedef HRESULT (WINAPI *SetPreferredAppMode_t)(BOOL);
        auto pSetPreferredAppMode = reinterpret_cast<SetPreferredAppMode_t>(
            GetProcAddress(hUxTheme, MAKEINTRESOURCEA(135)));
        if (pSetPreferredAppMode) {
            pSetPreferredAppMode(TRUE);
        }
        
        typedef HRESULT (WINAPI *FlushMenuThemes_t)();
        auto pFlushMenuThemes = reinterpret_cast<FlushMenuThemes_t>(
            GetProcAddress(hUxTheme, MAKEINTRESOURCEA(136)));
        if (pFlushMenuThemes) {
            pFlushMenuThemes();
        }
        FreeLibrary(hUxTheme);
    }
}
