#ifndef NETWORK_TCP_H
#define NETWORK_TCP_H

#include <winsock2.h>
#include <ws2tcpip.h>
#include <string>

// Khởi tạo thư viện Winsock (Bắt buộc trên Windows)
bool initWinsock();

// Tạo socket TCP cho kênh điều khiển (Control Channel)
SOCKET createTCPSocket();

// Kết nối đến Server thông qua IP và Port
bool connectToServer(SOCKET sock, const std::string& serverIP, int port);

// Đóng kết nối TCP và dọn dẹp bộ nhớ
void closeTCPSocket(SOCKET sock);

#endif
