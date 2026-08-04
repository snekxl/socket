#include "network_udp.h"
#include <iostream>
#include <fstream>

// Link với thư viện Winsock của Windows
#pragma comment(lib, "ws2_32.lib")

#define BUFFER_SIZE 4096

using namespace std;

SOCKET createUDPSocket() {
    SOCKET sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock == INVALID_SOCKET) {
        cerr << "[-] Tao socket UDP that bai. Ma loi: " << WSAGetLastError() << endl;
    }
    return sock;
}

bool receiveFileUDP(SOCKET udpSocket, int localPort, const string& savePath) {
    sockaddr_in localAddr;
    localAddr.sin_family = AF_INET;
    localAddr.sin_addr.s_addr = INADDR_ANY;
    localAddr.sin_port = htons(localPort);

    // Mở port tại Client để chờ Server nã dữ liệu vào
    if (bind(udpSocket, (sockaddr*)&localAddr, sizeof(localAddr)) == SOCKET_ERROR) {
        cerr << "[-] Bind socket UDP that bai. Ma loi: " << WSAGetLastError() << endl;
        return false;
    }

    ofstream file(savePath, ios::binary);
    if (!file.is_open()) {
        cerr << "[-] Khong the tao file de luu: " << savePath << endl;
        return false;
    }

    char buffer[BUFFER_SIZE];
    sockaddr_in senderAddr;
    int senderAddrSize = sizeof(senderAddr);

    cout << "[*] Dang cho nhan du lieu tren port UDP " << localPort << "..." << endl;

    while (true) {
        int bytesReceived = recvfrom(udpSocket, buffer, BUFFER_SIZE, 0, (sockaddr*)&senderAddr, &senderAddrSize);
        if (bytesReceived == SOCKET_ERROR) {
            cerr << "[-] Loi khi nhan du lieu UDP. Ma loi: " << WSAGetLastError() << endl;
            file.close();
            return false;
        }

        // Quy ước: Nhận được gói tin có chữ "EOF" thì ngắt vòng lặp, hoàn tất file
        if (bytesReceived == 3 && strncmp(buffer, "EOF", 3) == 0) {
            break;
        }

        file.write(buffer, bytesReceived);
    }

    file.close();
    cout << "[+] Da nhan va luu file thanh cong: " << savePath << endl;
    return true;
}

bool sendFileUDP(SOCKET udpSocket, const string& serverIP, int serverPort, const string& filePath) {
    ifstream file(filePath, ios::binary);
    if (!file.is_open()) {
        cerr << "[-] Khong the mo file de gui: " << filePath << endl;
        return false;
    }

    sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(serverPort);
    inet_pton(AF_INET, serverIP.c_str(), &serverAddr.sin_addr);

    char buffer[BUFFER_SIZE];
    while (file.read(buffer, BUFFER_SIZE) || file.gcount() > 0) {
        int bytesRead = file.gcount();
        int bytesSent = sendto(udpSocket, buffer, bytesRead, 0, (sockaddr*)&serverAddr, sizeof(serverAddr));
        if (bytesSent == SOCKET_ERROR) {
            cerr << "[-] Loi khi gui du lieu UDP. Ma loi: " << WSAGetLastError() << endl;
            file.close();
            return false;
        }
    }

    // Gửi gói tin báo hiệu kết thúc file cho Server
    const char* eofMarker = "EOF";
    sendto(udpSocket, eofMarker, strlen(eofMarker), 0, (sockaddr*)&serverAddr, sizeof(serverAddr));

    file.close();
    cout << "[+] Da gui file thanh cong qua UDP: " << filePath << endl;
    return true;
}

void closeUDPSocket(SOCKET udpSocket) {
    if (udpSocket != INVALID_SOCKET) {
        closesocket(udpSocket);
    }
}
