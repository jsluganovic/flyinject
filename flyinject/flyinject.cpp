#include <windows.h>
#include <commdlg.h>
#include <iostream>
#include <tlhelp32.h>

wchar_t dllPath[MAX_PATH] = L"";

bool InjectDLL(DWORD processId, const wchar_t* dllPath) {
    HANDLE hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, processId);
    if (hProcess == NULL) {
        std::cerr << "Could not open process" << std::endl;
        return false;
    }

    size_t dllPathSize = (wcslen(dllPath) + 1) * sizeof(wchar_t);
    void* pDllPath = VirtualAllocEx(hProcess, NULL, dllPathSize, MEM_COMMIT, PAGE_READWRITE);
    if (pDllPath == NULL) {
        std::cerr << "Could not allocate memory in the target process" << std::endl;
        CloseHandle(hProcess);
        return false;
    }

    WriteProcessMemory(hProcess, pDllPath, dllPath, dllPathSize, NULL);

    HMODULE hKernel32 = GetModuleHandle(L"kernel32.dll");
    FARPROC pLoadLibraryW = GetProcAddress(hKernel32, "LoadLibraryW");

    HANDLE hThread = CreateRemoteThread(hProcess, NULL, 0, (LPTHREAD_START_ROUTINE)pLoadLibraryW, pDllPath, 0, NULL);
    if (hThread == NULL) {
        std::cerr << "Could not create remote thread" << std::endl;
        VirtualFreeEx(hProcess, pDllPath, 0, MEM_RELEASE);
        CloseHandle(hProcess);
        return false;
    }

    WaitForSingleObject(hThread, INFINITE);

    VirtualFreeEx(hProcess, pDllPath, 0, MEM_RELEASE);
    CloseHandle(hThread);
    CloseHandle(hProcess);

    return true;
}

DWORD GetProcessIdByName(const wchar_t* processName) {
    DWORD processId = 0;
    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnapshot != INVALID_HANDLE_VALUE) {
        PROCESSENTRY32W pe;
        pe.dwSize = sizeof(PROCESSENTRY32W);
        if (Process32FirstW(hSnapshot, &pe)) {
            do {
                if (wcscmp(pe.szExeFile, processName) == 0) {
                    processId = pe.th32ProcessID;
                    break;
                }
            } while (Process32NextW(hSnapshot, &pe));
        }
        CloseHandle(hSnapshot);
    }
    return processId;
}

void ChooseDLLFile(HWND hwnd) {
    OPENFILENAMEW ofn;
    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = hwnd;
    ofn.lpstrFile = dllPath;
    ofn.lpstrFile[0] = L'\0';
    ofn.nMaxFile = sizeof(dllPath) / sizeof(dllPath[0]);
    ofn.lpstrFilter = L"DLL Files\0*.dll\0All Files\0*.*\0";
    ofn.nFilterIndex = 1;
    ofn.lpstrFileTitle = NULL;
    ofn.nMaxFileTitle = 0;
    ofn.lpstrInitialDir = NULL;
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;

    if (GetOpenFileNameW(&ofn) == TRUE) {
        InvalidateRect(hwnd, NULL, TRUE);
    }
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
    case WM_CREATE: {
        CreateWindowW(L"BUTTON", L"Choose DLL", WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON,
            10, 10, 100, 30, hwnd, (HMENU)1, (HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE), NULL);
        break;
    }
    case WM_COMMAND: {
        if (LOWORD(wParam) == 1) {
            ChooseDLLFile(hwnd);
        }
        break;
    }
    case WM_DROPFILES: {
        if (dllPath[0] == L'\0') {
            MessageBox(hwnd, L"Please choose a DLL file first.", L"Error", MB_OK | MB_ICONERROR);
            break;
        }

        HDROP hDrop = (HDROP)wParam;
        wchar_t filePath[MAX_PATH];
        DragQueryFileW(hDrop, 0, filePath, MAX_PATH);
        DragFinish(hDrop);

        // Start the dropped executable
        STARTUPINFOW si = { sizeof(si) };
        PROCESS_INFORMATION pi;
        if (!CreateProcessW(filePath, NULL, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {
            std::cerr << "Could not start process" << std::endl;
            return 0;
        }

        // Wait for the process to start
        WaitForInputIdle(pi.hProcess, INFINITE);

        // Inject the DLL
        if (InjectDLL(pi.dwProcessId, dllPath)) {
            MessageBox(hwnd, L"DLL injected successfully!", L"Success", MB_OK | MB_ICONINFORMATION);
        }
        else {
            MessageBox(hwnd, L"DLL injection failed!", L"Error", MB_OK | MB_ICONERROR);
        }

        // Close process and thread handles
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        break;
    }
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        RECT rect;
        GetClientRect(hwnd, &rect);
        DrawText(hdc, L"Choose a DLL by pressing the button.", -1, &rect, DT_CENTER | DT_TOP | DT_SINGLELINE);
        DrawText(hdc, L"Drag and drop an executable file here to inject the DLL.", -1, &rect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        if (dllPath[0] != L'\0') {
            DrawText(hdc, dllPath, -1, &rect, DT_CENTER | DT_BOTTOM | DT_SINGLELINE);
        }
        EndPaint(hwnd, &ps);
        break;
    }

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

int main() {
    const wchar_t CLASS_NAME[] = L"FlyInject > fly1nj3kt0r by jsluganovic";

    // Caution don't close console 
    std::cout << "Don't close this console it should be used for logging purposes from your DLL" << std::endl;

    WNDCLASSW wc = {};
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = GetModuleHandle(NULL);
    wc.lpszClassName = CLASS_NAME;

    RegisterClassW(&wc);

    HWND hwnd = CreateWindowExW(
        0,
        CLASS_NAME,
        L"FlyInject > fly1nj3kt0r by jsluganovic",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 700, 400, // Adjusted width and height
        NULL,
        NULL,
        GetModuleHandle(NULL),
        NULL
    );

    if (hwnd == NULL) {
        return 0;
    }

    DragAcceptFiles(hwnd, TRUE);

    ShowWindow(hwnd, SW_SHOWDEFAULT);

    MSG msg = {};
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return 0;
}