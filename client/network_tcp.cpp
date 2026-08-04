#include "network_tcp.h"
#include <iostream>

// Link với thư viện Winsock của Windows
#pragma comment(lib, "ws2_32.lib")

using namespace std;

bool initWinsock() {
    WSADATA wsaData;
    // Khởi tạo Winsock phiên bản 2.2
    int iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (iResult != 0) {
        cerr << "[-] Khoi tao Winsock that bai. Ma loi: " << iResult << endl;
        return false;
    }
    return true;
}

SOCKET createTCPSocket() {
    // AF_INET: IPv4, SOCK_STREAM: Giao thức TCP
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
    
    // Chuyển đổi IP dạng chuỗi (vd: "127.0.0.1") sang cấu trúc nhị phân của mạng
    if (inet_pton(AF_INET, serverIP.c_str(), &serverAddr.sin_addr) <= 0) {
        cerr << "[-] Dia chi IP khong hop le hoac khong duoc ho tro: " << serverIP << endl;
        return false;
    }

    cout << "[*] Dang ket noi den Control Channel tai " << serverIP << ":" << port << "..." << endl;
    
    // Thực hiện quá trình bắt tay 3 bước (3-way handshake) của TCP
    if (connect(sock, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        cerr << "[-] Ket noi den Server that bai. Kiem tra xem Server da bat chua. Ma loi: " << WSAGetLastError() << endl;
        return false;
    }
    
    cout << "[+] Ket noi TCP thanh cong! Kanh dieu khien (Control Channel) da san sang." << endl;
    return true;
}

void closeTCPSocket(SOCKET sock) {
    // Đóng socket nếu nó đang mở
    if (sock != INVALID_SOCKET) {
        closesocket(sock);
    }
    // Dọn dẹp tài nguyên Winsock
    WSACleanup();
    cout << "[*] Da ngat ket noi TCP va don dep tai nguyen." << endl;
}
