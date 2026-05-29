#include <iostream>
#include <WinSock2.h>
#include <WS2tcpip.h>
#include <stdio.h>
#pragma comment(lib, "Ws2_32.lib")
using namespace std;

const char IP_SERV[] = "127.0.0.1";
const int  PORT_NUM = 1234;
const int  BUFF_SIZE = 4096;

// --- Log ---
#define LOG_MAX 100
char logMessages[LOG_MAX][256];
int  logCount = 0;

void logAdd(const char* msg) {
    if (logCount < LOG_MAX) {
        strncpy_s(logMessages[logCount], 256, msg, _TRUNCATE);
        logCount++;
    }
}

void showLog() {
    cout << "\n=== MESSAGE LOG ===" << endl;
    if (logCount == 0) cout << "(log is empty)" << endl;
    else for (int i = 0; i < logCount; i++)
        cout << "[" << i + 1 << "] " << logMessages[i] << endl;
    cout << "===================" << endl;
}

// --- Send file ---
// Protocol:
//   sender sends header:  "FILE:<filename>:<filesize>\n"
//   then sends raw bytes of the file
//   receiver saves to "received_<filename>"

bool sendFile(SOCKET sock, const char* filepath) {
    // Extract filename from path
    const char* filename = filepath;
    for (const char* p = filepath; *p; p++)
        if (*p == '\\' || *p == '/') filename = p + 1;

    // Open file
    FILE* f = nullptr;
    if (fopen_s(&f, filepath, "rb") != 0 || !f) {
        cout << "Cannot open file: " << filepath << endl;
        return false;
    }

    // Get file size
    fseek(f, 0, SEEK_END);
    long filesize = ftell(f);
    fseek(f, 0, SEEK_SET);

    // Send header
    char header[512];
    sprintf_s(header, "FILE:%s:%ld\n", filename, filesize);
    send(sock, header, (int)strlen(header), 0);

    // Send file contents
    char buf[BUFF_SIZE];
    long sent = 0;
    while (sent < filesize) {
        int toRead = (filesize - sent) < BUFF_SIZE ? (int)(filesize - sent) : BUFF_SIZE;
        int n = (int)fread(buf, 1, toRead, f);
        if (n <= 0) break;
        int s = send(sock, buf, n, 0);
        if (s == SOCKET_ERROR) {
            cout << "Send error #" << WSAGetLastError() << endl;
            fclose(f);
            return false;
        }
        sent += s;
    }
    fclose(f);

    cout << "File sent: " << filename << " (" << filesize << " bytes)" << endl;
    char entry[256];
    sprintf_s(entry, "File sent: %s (%ld bytes)", filename, filesize);
    logAdd(entry);
    return true;
}

bool receiveFile(SOCKET sock, const char* header) {
    // Parse header: FILE:<filename>:<filesize>
    char filename[256] = {};
    long filesize = 0;

    // header already stripped of "FILE:" prefix — parse from full header
    // header format: "FILE:<filename>:<filesize>"
    char tmp[512];
    strncpy_s(tmp, 512, header, _TRUNCATE);

    char* ctx = nullptr;
    char* tok = strtok_s(tmp, ":", &ctx);  // "FILE"
    tok = strtok_s(nullptr, ":", &ctx);     // filename
    if (!tok) return false;
    strncpy_s(filename, 256, tok, _TRUNCATE);
    tok = strtok_s(nullptr, ":", &ctx);     // filesize
    if (!tok) return false;
    filesize = atol(tok);

    // Build output filename
    char outpath[512];
    sprintf_s(outpath, "received_%s", filename);

    FILE* f = nullptr;
    if (fopen_s(&f, outpath, "wb") != 0 || !f) {
        cout << "Cannot create file: " << outpath << endl;
        return false;
    }

    // Receive file contents
    char buf[BUFF_SIZE];
    long received = 0;
    while (received < filesize) {
        int toRead = (filesize - received) < BUFF_SIZE ? (int)(filesize - received) : BUFF_SIZE;
        int n = recv(sock, buf, toRead, 0);
        if (n <= 0) break;
        fwrite(buf, 1, n, f);
        received += n;
    }
    fclose(f);

    cout << "File received and saved as: " << outpath << " (" << received << " bytes)" << endl;
    char entry[256];
    sprintf_s(entry, "File received: %s (%ld bytes)", outpath, received);
    logAdd(entry);
    return true;
}

// --- Server session ---
void runServer() {
    int erStat;

    in_addr ip_to_num;
    erStat = inet_pton(AF_INET, IP_SERV, &ip_to_num);
    if (erStat <= 0) { cout << "Error in IP translation." << endl; return; }

    WSADATA wsData;
    erStat = WSAStartup(MAKEWORD(2, 2), &wsData);
    if (erStat != 0) { cout << "Error WinSock init #" << WSAGetLastError() << endl; return; }
    cout << "WinSock is initialized." << endl;

    SOCKET ServSock = socket(AF_INET, SOCK_STREAM, 0);
    if (ServSock == INVALID_SOCKET) {
        cout << "Error creating socket #" << WSAGetLastError() << endl;
        WSACleanup(); return;
    }
    cout << "Server socket is initialized." << endl;

    sockaddr_in servInfo;
    ZeroMemory(&servInfo, sizeof(servInfo));
    servInfo.sin_family = AF_INET;
    servInfo.sin_addr = ip_to_num;
    servInfo.sin_port = htons(PORT_NUM);

    erStat = bind(ServSock, (sockaddr*)&servInfo, sizeof(servInfo));
    if (erStat != 0) {
        cout << "Error binding socket #" << WSAGetLastError() << endl;
        closesocket(ServSock); WSACleanup(); return;
    }
    cout << "Socket bound to port " << PORT_NUM << "." << endl;

    erStat = listen(ServSock, SOMAXCONN);
    if (erStat != 0) {
        cout << "Error listen #" << WSAGetLastError() << endl;
        closesocket(ServSock); WSACleanup(); return;
    }
    cout << "Listening. Waiting for client..." << endl;

    sockaddr_in clientInfo;
    ZeroMemory(&clientInfo, sizeof(clientInfo));
    int clientInfo_size = sizeof(clientInfo);
    SOCKET ClientConn = accept(ServSock, (sockaddr*)&clientInfo, &clientInfo_size);
    if (ClientConn == INVALID_SOCKET) {
        cout << "Error accepting client #" << WSAGetLastError() << endl;
        closesocket(ServSock); WSACleanup(); return;
    }

    char clientIP[22];
    inet_ntop(AF_INET, &clientInfo.sin_addr, clientIP, INET_ADDRSTRLEN);
    cout << "Client connected. IP: " << clientIP << endl;
    char entry[256];
    sprintf_s(entry, "Client connected from IP: %s", clientIP);
    logAdd(entry);

    char recvBuf[BUFF_SIZE];
    char sendBuf[BUFF_SIZE];
    int  packet_size = 0;

    cout << "\n[Commands: type message and Enter | 'sendfile' to send a file | 'exit' to quit]\n" << endl;

    while (true) {
        ZeroMemory(recvBuf, BUFF_SIZE);
        ZeroMemory(sendBuf, BUFF_SIZE);

        // Receive from client
        packet_size = recv(ClientConn, recvBuf, BUFF_SIZE, 0);
        if (packet_size == SOCKET_ERROR || packet_size == 0) {
            cout << "Client disconnected." << endl;
            logAdd("Client disconnected.");
            break;
        }

        // Check if it's a file
        if (strncmp(recvBuf, "FILE:", 5) == 0) {
            receiveFile(ClientConn, recvBuf);
            continue;
        }

        cout << "Client message: " << recvBuf << endl;
        sprintf_s(entry, "Client: %s", recvBuf);
        logAdd(entry);

        // Server's turn to respond
        cout << "Server (message / 'sendfile' / 'exit'): ";
        fgets(sendBuf, BUFF_SIZE, stdin);

        int len = (int)strlen(sendBuf);
        if (len > 0 && sendBuf[len - 1] == '\n') sendBuf[len - 1] = '\0';

        // exit
        if (strcmp(sendBuf, "exit") == 0) {
            logAdd("Server ended session with 'exit'.");
            send(ClientConn, "exit", 5, 0);
            shutdown(ClientConn, SD_BOTH);
            break;
        }

        // send file
        if (strcmp(sendBuf, "sendfile") == 0) {
            char filepath[512];
            cout << "Enter file path: ";
            fgets(filepath, 512, stdin);
            len = (int)strlen(filepath);
            if (len > 0 && filepath[len - 1] == '\n') filepath[len - 1] = '\0';
            sendFile(ClientConn, filepath);
            continue;
        }

        // send text message
        packet_size = send(ClientConn, sendBuf, (int)strlen(sendBuf) + 1, 0);
        if (packet_size == SOCKET_ERROR) {
            cout << "Send error #" << WSAGetLastError() << endl;
            break;
        }
        sprintf_s(entry, "Server: %s", sendBuf);
        logAdd(entry);
    }

    closesocket(ClientConn);
    closesocket(ServSock);
    WSACleanup();
    cout << "Server stopped." << endl;
}

// --- Menu ---
void showMenu() {
    cout << "\n==============================" << endl;
    cout << "    SERVER - MAIN MENU" << endl;
    cout << "==============================" << endl;
    cout << "  1. Start server" << endl;
    cout << "  2. View log" << endl;
    cout << "  3. Exit" << endl;
    cout << "==============================" << endl;
    cout << "Select option: ";
}

int main(void) {
    int choice = 0;
    while (true) {
        showMenu();
        cin >> choice;
        cin.ignore();
        switch (choice) {
        case 1: runServer(); break;
        case 2: showLog();   break;
        case 3: cout << "Goodbye!" << endl; return 0;
        default: cout << "Invalid option." << endl; break;
        }
    }
    return 0;
}
