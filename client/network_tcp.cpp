#include "network_tcp.h"
#include <iostream>

#pragma comment(lib, "ws2_32.lib")

using namespace std;

bool initWinsock() {
    WSADATA wsaData;
    int iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (iResult != 0) {
        cerr << "[-] Khoi tao Winsock that bai. Ma loi: " << iResult << endl;
        return false;
    }
    return true;
}

SOCKET createTCPSocket() {
    SOCKET sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock == INVALID_SOCKET) {
        cerr << "[-] Tao socket TCP that bai. Ma loi: " << WSAGetLastError() << endl;
    }
    return sock;
}

bool connectToServer(SOCKET sock, const string& serverIP, int port) {
    sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(port);

    if (inet_pton(AF_INET, serverIP.c_str(), &serverAddr.sin_addr) <= 0) {
        cerr << "[-] Dia chi IP khong hop le hoac khong duoc ho tro: " << serverIP << endl;
        return false;
    }

    cout << "[*] Dang ket noi den Control Channel tai " << serverIP << ":" << port << "..." << endl;

    if (connect(sock, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        cerr << "[-] Ket noi den Server that bai. Kiem tra xem Server da bat chua. Ma loi: " << WSAGetLastError() << endl;
        return false;
    }

    cout << "[+] Ket noi TCP thanh cong! Kanh dieu khien (Control Channel) da san sang." << endl;
    return true;
}

void closeTCPSocket(SOCKET sock) {
    if (sock != INVALID_SOCKET) {
        closesocket(sock);
    }
    WSACleanup();
    cout << "[*] Da ngat ket noi TCP va don dep tai nguyen." << endl;
}
