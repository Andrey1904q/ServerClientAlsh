#include <iostream>
#include <WinSock2.h>
#include <WS2tcpip.h>
#include <stdio.h>
#pragma comment(lib, "Ws2_32.lib")
using namespace std;

const char IP_SERV[] = "127.0.0.1";
const int  PORT_NUM = 1234;
const short BUFF_SIZE = 1024;

#define LOG_MAX 100
char logMessages[LOG_MAX][256];
int  logCount = 0;

void logAdd(const char* msg)
{
    if (logCount < LOG_MAX) {
        strncpy_s(logMessages[logCount], 256, msg, _TRUNCATE);
        logCount++;
    }
}

void showLog()
{
    cout << "\n=== MESSAGE LOG ===" << endl;
    if (logCount == 0) {
        cout << "(log is empty)" << endl;
    }
    else {
        for (int i = 0; i < logCount; i++)
            cout << "[" << i + 1 << "] " << logMessages[i] << endl;
    }
    cout << "===================" << endl;
}


void runServer()
{
    int erStat;


    in_addr ip_to_num;
    erStat = inet_pton(AF_INET, IP_SERV, &ip_to_num);
    if (erStat <= 0) {
        cout << "Error in IP translation to special numeric format." << endl;
        return;
    }

    WSADATA wsData;
    erStat = WSAStartup(MAKEWORD(2, 2), &wsData);
    if (erStat != 0) {
        cout << "Error WinSock version initialization #" << WSAGetLastError() << endl;
        return;
    }
    cout << "WinSock is initialized." << endl;


    SOCKET ServSock = socket(AF_INET, SOCK_STREAM, 0);
    if (ServSock == INVALID_SOCKET) {
        cout << "Error initialization socket # " << WSAGetLastError() << endl;
        WSACleanup();
        return;
    }
    cout << "Server socket is initialized." << endl;


    sockaddr_in servInfo;
    ZeroMemory(&servInfo, sizeof(servInfo));
    servInfo.sin_family = AF_INET;
    servInfo.sin_addr = ip_to_num;
    servInfo.sin_port = htons(PORT_NUM);

    erStat = bind(ServSock, (sockaddr*)&servInfo, sizeof(servInfo));
    if (erStat != 0) {
        cout << "Error Socket binding to server info. Error # " << WSAGetLastError() << endl;
        closesocket(ServSock);
        WSACleanup();
        return;
    }
    cout << "Socket binding to server information completed. Port: " << PORT_NUM << endl;


    erStat = listen(ServSock, SOMAXCONN);
    if (erStat != 0) {
        cout << "Can't start to listen to. Error # " << WSAGetLastError() << endl;
        closesocket(ServSock);
        WSACleanup();
        return;
    }
    cout << "Listening. Waiting for client connection..." << endl;


    sockaddr_in clientInfo;
    ZeroMemory(&clientInfo, sizeof(clientInfo));
    int clientInfo_size = sizeof(clientInfo);

    SOCKET ClientConn = accept(ServSock, (sockaddr*)&clientInfo, &clientInfo_size);
    if (ClientConn == INVALID_SOCKET) {
        cout << "Client detected, but can't connect to a client. Error # " << WSAGetLastError() << endl;
        closesocket(ServSock);
        WSACleanup();
        return;
    }

    char clientIP[22];
    inet_ntop(AF_INET, &clientInfo.sin_addr, clientIP, INET_ADDRSTRLEN);
    cout << "Connection to a client established. Client IP: " << clientIP << endl;

    char logEntry[256];
    sprintf_s(logEntry, "Client connected from IP: %s", clientIP);
    logAdd(logEntry);


    char servBuff[BUFF_SIZE];
    char clientBuff[BUFF_SIZE];
    short packet_size = 0;

    cout << "\nType 'exit' to end the session.\n" << endl;

    while (true) {
        ZeroMemory(servBuff, BUFF_SIZE);
        ZeroMemory(clientBuff, BUFF_SIZE);
        packet_size = recv(ClientConn, servBuff, BUFF_SIZE, 0);
        if (packet_size == SOCKET_ERROR || packet_size == 0) {
            cout << "Client disconnected." << endl;
            logAdd("Client disconnected.");
            break;
        }
        cout << "Client message: " << servBuff << endl;

        sprintf_s(logEntry, "Client: %s", servBuff);
        logAdd(logEntry);

        cout << "Server message: ";
        fgets(clientBuff, BUFF_SIZE, stdin);

        int len = (int)strlen(clientBuff);
        if (len > 0 && clientBuff[len - 1] == '\n')
            clientBuff[len - 1] = '\0';

        if (clientBuff[0] == 'e' && clientBuff[1] == 'x' &&
            clientBuff[2] == 'i' && clientBuff[3] == 't') {
            logAdd("Server ended session with 'exit'.");
            shutdown(ClientConn, SD_BOTH);
            break;
        }

        sprintf_s(logEntry, "Server: %s", clientBuff);
        logAdd(logEntry);

        packet_size = send(ClientConn, clientBuff, BUFF_SIZE, 0);
        if (packet_size == SOCKET_ERROR) {
            cout << "Can't send message to Client. Error # " << WSAGetLastError() << endl;
            break;
        }
    }

    closesocket(ClientConn);
    closesocket(ServSock);
    WSACleanup();
    cout << "Server stopped." << endl;
}

void showMenu()
{
    cout << "\n==============================" << endl;
    cout << "    SERVER - MAIN MENU" << endl;
    cout << "==============================" << endl;
    cout << "  1. Start server" << endl;
    cout << "  2. View log" << endl;
    cout << "  3. Exit" << endl;
    cout << "==============================" << endl;
    cout << "Select option: ";
}

int main(void)
{
    int choice = 0;

    while (true) {
        showMenu();
        cin >> choice;
        cin.ignore(); 

        switch (choice) {
        case 1:
            runServer();
            break;
        case 2:
            showLog();
            break;
        case 3:
            cout << "Goodbye!" << endl;
            return 0;
        default:
            cout << "Invalid option. Please try again." << endl;
            break;
        }
    }

    return 0;
}
