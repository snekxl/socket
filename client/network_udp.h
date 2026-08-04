#ifndef NETWORK_UDP_H
#define NETWORK_UDP_H

#include <winsock2.h>
#include <ws2tcpip.h>
#include <string>

// Khởi tạo socket UDP
SOCKET createUDPSocket();

// Nhận dữ liệu file qua UDP (dành cho lệnh tải file - RETR)
bool receiveFileUDP(SOCKET udpSocket, int localPort, const std::string& savePath);

// Gửi dữ liệu file qua UDP (dành cho lệnh đẩy file - STOR)
bool sendFileUDP(SOCKET udpSocket, const std::string& serverIP, int serverPort, const std::string& filePath);

// Đóng socket UDP an toàn
void closeUDPSocket(SOCKET udpSocket);

#endif
