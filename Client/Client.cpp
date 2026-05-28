#include <iostream>
#include <WinSock2.h>
#include <WS2tcpip.h>
#include <stdio.h>
#pragma comment(lib, "Ws2_32.lib")
using namespace std;

const char SERVER_IP[] = "127.0.0.1";
const short SERVER_PORT_NUM = 1234;
const short BUFF_SIZE = 1024;

void runClient()
{
    int erStat;

    in_addr ip_to_num;
    inet_pton(AF_INET, SERVER_IP, &ip_to_num);
    WSADATA wsData;
    erStat = WSAStartup(MAKEWORD(2, 2), &wsData);
    if (erStat != 0) {
        cout << "Error WinSock version initialization #" << WSAGetLastError() << endl;
        return;
    }
    cout << "WinSock is initialized." << endl;

    SOCKET ClientSock = socket(AF_INET, SOCK_STREAM, 0);
    if (ClientSock == INVALID_SOCKET) {
        cout << "Error initialization socket # " << WSAGetLastError() << endl;
        WSACleanup();
        return;
    }
    cout << "Client socket is initialized." << endl;
    sockaddr_in servInfo;
    ZeroMemory(&servInfo, sizeof(servInfo));
    servInfo.sin_family = AF_INET;
    servInfo.sin_addr = ip_to_num;
    servInfo.sin_port = htons(SERVER_PORT_NUM);

    erStat = connect(ClientSock, (sockaddr*)&servInfo, sizeof(servInfo));
    if (erStat != 0) {
        cout << "Connection to Server is failed. Error # " << WSAGetLastError() << endl;
        closesocket(ClientSock);
        WSACleanup();
        return;
    }
    cout << "Connection is established. Server: " << SERVER_IP << ":" << SERVER_PORT_NUM << endl;
    cout << "\nType 'exit' to end the session.\n" << endl;

    char servBuff[BUFF_SIZE];
    char clientBuff[BUFF_SIZE];
    short packet_size = 0;

    while (true) {
        ZeroMemory(servBuff, BUFF_SIZE);
        ZeroMemory(clientBuff, BUFF_SIZE);

        cout << "Client message to Server: ";
        fgets(clientBuff, BUFF_SIZE, stdin);

        int len = (int)strlen(clientBuff);
        if (len > 0 && clientBuff[len - 1] == '\n')
            clientBuff[len - 1] = '\0';

        if (clientBuff[0] == 'e' && clientBuff[1] == 'x' &&
            clientBuff[2] == 'i' && clientBuff[3] == 't') {
            shutdown(ClientSock, SD_BOTH);
            break;
        }

        packet_size = send(ClientSock, clientBuff, BUFF_SIZE, 0);
        if (packet_size == SOCKET_ERROR) {
            cout << "Can't send message to Server. Error # " << WSAGetLastError() << endl;
            break;
        }

        packet_size = recv(ClientSock, servBuff, BUFF_SIZE, 0);
        if (packet_size == SOCKET_ERROR || packet_size == 0) {
            cout << "Server disconnected." << endl;
            break;
        }
        cout << "Server message: " << servBuff << endl;
    }

    closesocket(ClientSock);
    WSACleanup();
    cout << "Connection closed." << endl;
}

void showMenu()
{
    cout << "\n==============================" << endl;
    cout << "    CLIENT - MAIN MENU" << endl;
    cout << "==============================" << endl;
    cout << "  1. Connect to server" << endl;
    cout << "  2. Exit" << endl;
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
            runClient();
            break;
        case 2:
            cout << "Goodbye!" << endl;
            return 0;
        default:
            cout << "Invalid option. Please try again." << endl;
            break;
        }
    }

    return 0;
}
