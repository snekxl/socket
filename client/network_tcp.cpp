#include "network_tcp.h"
#include <ws2tcpip.h>
#include <iostream>

using namespace std;

SOCKET connectTCP(const string& ip, int port) {
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        cerr << "Loi: Khong the khoi tao Winsock!" << endl;
        return INVALID_SOCKET;
    }

    SOCKET sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock == INVALID_SOCKET) {
        cerr << "Loi tao socket: " << WSAGetLastError() << endl;
        WSACleanup();
        return INVALID_SOCKET;
    }

    sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    inet_pton(AF_INET, ip.c_str(), &server_addr.sin_addr);

    cout << "[TCP] Dang ket noi den Server " << ip << ":" << port << "..." << endl;

    if (connect(sock, (sockaddr*)&server_addr, sizeof(server_addr)) == SOCKET_ERROR) {
        cerr << "-> Khong the ket noi! (Ban da bat Server chua?)" << endl;
        closesocket(sock);      // Dọn dẹp cái đầu cắm bị lỗi
        return INVALID_SOCKET;  // Báo về cho parser biết là mạng đã "tạch"
    }
    else {
        cout << "-> Ket noi TCP thanh cong!" << endl;
    }

    return sock;

}

string receiveTCP(SOCKET sock) {
    if (sock == INVALID_SOCKET) return ""; // Tránh lỗi nếu chưa kết nối

    char buffer[1024]; // Tạo một cái xô dung tích 1024 byte để hứng dữ liệu
    memset(buffer, 0, sizeof(buffer)); // Rửa sạch xô trước khi dùng

    // Đứng chờ hứng dữ liệu từ Server
    int bytes_received = recv(sock, buffer, sizeof(buffer) - 1, 0);

    if (bytes_received > 0) {
        return string(buffer); // Hứng thành công, chuyển thành chuỗi và trả về
    }
    else if (bytes_received == 0) {
        cout << "[TCP] Server da chu dong dong ket noi." << endl;
    }
    else {
        cerr << "[TCP] Loi nhan du lieu: " << WSAGetLastError() << endl;
    }
    return "";
}

void cleanupTCP(SOCKET sock) {
    if (sock != INVALID_SOCKET) {
        closesocket(sock);
    }
    WSACleanup();
}