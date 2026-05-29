#include <iostream>
#include <WinSock2.h>
#include <WS2tcpip.h>
#include <stdio.h>
#pragma comment(lib, "Ws2_32.lib")
using namespace std;

const short SERVER_PORT_NUM = 1234;
const int   BUFF_SIZE = 4096;

// --- Send file ---
bool sendFile(SOCKET sock, const char* filepath) {
    const char* filename = filepath;
    for (const char* p = filepath; *p; p++)
        if (*p == '\\' || *p == '/') filename = p + 1;

    FILE* f = nullptr;
    if (fopen_s(&f, filepath, "rb") != 0 || !f) {
        cout << "Cannot open file: " << filepath << endl;
        return false;
    }

    fseek(f, 0, SEEK_END);
    long filesize = ftell(f);
    fseek(f, 0, SEEK_SET);

    char header[512];
    sprintf_s(header, "FILE:%s:%ld\n", filename, filesize);
    send(sock, header, (int)strlen(header), 0);

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
    return true;
}

bool receiveFile(SOCKET sock, const char* header) {
    char filename[256] = {};
    long filesize = 0;

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

    char outpath[512];
    sprintf_s(outpath, "received_%s", filename);

    FILE* f = nullptr;
    if (fopen_s(&f, outpath, "wb") != 0 || !f) {
        cout << "Cannot create file: " << outpath << endl;
        return false;
    }

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
    return true;
}

// --- Client session ---
void runClient() {
    int erStat;

    // Ask for server IP
    char SERVER_IP[64];
    cout << "Enter server IP (e.g. 192.168.1.105, or 127.0.0.1 for same PC): ";
    fgets(SERVER_IP, 64, stdin);
    int iplen = (int)strlen(SERVER_IP);
    if (iplen > 0 && SERVER_IP[iplen - 1] == '\n') SERVER_IP[iplen - 1] = '\0';

    in_addr ip_to_num;
    if (inet_pton(AF_INET, SERVER_IP, &ip_to_num) <= 0) {
        cout << "Invalid IP address: " << SERVER_IP << endl;
        return;
    }

    WSADATA wsData;
    erStat = WSAStartup(MAKEWORD(2, 2), &wsData);
    if (erStat != 0) { cout << "Error WinSock init #" << WSAGetLastError() << endl; return; }
    cout << "WinSock is initialized." << endl;

    SOCKET ClientSock = socket(AF_INET, SOCK_STREAM, 0);
    if (ClientSock == INVALID_SOCKET) {
        cout << "Error creating socket #" << WSAGetLastError() << endl;
        WSACleanup(); return;
    }
    cout << "Client socket is initialized." << endl;

    sockaddr_in servInfo;
    ZeroMemory(&servInfo, sizeof(servInfo));
    servInfo.sin_family = AF_INET;
    servInfo.sin_addr = ip_to_num;
    servInfo.sin_port = htons(SERVER_PORT_NUM);

    erStat = connect(ClientSock, (sockaddr*)&servInfo, sizeof(servInfo));
    if (erStat != 0) {
        cout << "Connection failed. Error #" << WSAGetLastError() << endl;
        closesocket(ClientSock); WSACleanup(); return;
    }
    cout << "Connection established. Server: " << SERVER_IP << ":" << SERVER_PORT_NUM << endl;
    cout << "\n[Commands: type message and Enter | 'sendfile' to send a file | 'exit' to quit]\n" << endl;

    char sendBuf[BUFF_SIZE];
    char recvBuf[BUFF_SIZE];
    int  packet_size = 0;

    while (true) {
        ZeroMemory(sendBuf, BUFF_SIZE);
        ZeroMemory(recvBuf, BUFF_SIZE);

        // Client's turn to send
        cout << "Client (message / 'sendfile' / 'exit'): ";
        fgets(sendBuf, BUFF_SIZE, stdin);

        int len = (int)strlen(sendBuf);
        if (len > 0 && sendBuf[len - 1] == '\n') sendBuf[len - 1] = '\0';

        // exit
        if (strcmp(sendBuf, "exit") == 0) {
            send(ClientSock, "exit", 5, 0);
            shutdown(ClientSock, SD_BOTH);
            break;
        }

        // send file
        if (strcmp(sendBuf, "sendfile") == 0) {
            char filepath[512];
            cout << "Enter file path: ";
            fgets(filepath, 512, stdin);
            len = (int)strlen(filepath);
            if (len > 0 && filepath[len - 1] == '\n') filepath[len - 1] = '\0';
            sendFile(ClientSock, filepath);
            continue; // skip recv — server doesn't reply to file, just saves it
        }
        else {
            // send text message
            packet_size = send(ClientSock, sendBuf, (int)strlen(sendBuf) + 1, 0);
            if (packet_size == SOCKET_ERROR) {
                cout << "Send error #" << WSAGetLastError() << endl;
                break;
            }
        }

        // Receive server response
        packet_size = recv(ClientSock, recvBuf, BUFF_SIZE, 0);
        if (packet_size == SOCKET_ERROR || packet_size == 0) {
            cout << "Server disconnected." << endl;
            break;
        }

        // Check if server sent exit
        if (strcmp(recvBuf, "exit") == 0) {
            cout << "Server ended the session." << endl;
            break;
        }

        // Check if server sent a file
        if (strncmp(recvBuf, "FILE:", 5) == 0) {
            receiveFile(ClientSock, recvBuf);
            continue;
        }

        cout << "Server message: " << recvBuf << endl;
    }

    closesocket(ClientSock);
    WSACleanup();
    cout << "Connection closed." << endl;
}

// --- Menu ---
void showMenu() {
    cout << "\n==============================" << endl;
    cout << "    CLIENT - MAIN MENU" << endl;
    cout << "==============================" << endl;
    cout << "  1. Connect to server" << endl;
    cout << "  2. Exit" << endl;
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
        case 1: runClient(); break;
        case 2: cout << "Goodbye!" << endl; return 0;
        default: cout << "Invalid option." << endl; break;
        }
    }
    return 0;
}
