#ifndef NETWORK_UDP_H
#define NETWORK_UDP_H

#include <winsock2.h>
#include <ws2tcpip.h>
#include <string>
#include <cstdint>

// Định nghĩa các cờ (Flags) cho giao thức tự tạo
#define FLAG_DATA 0x01  // Gói tin chứa dữ liệu file
#define FLAG_ACK  0x02  // Gói tin phản hồi xác nhận
#define FLAG_FIN  0x04  // Gói tin báo hiệu kết thúc

// Cấu trúc Custom UDP Header (bắt buộc theo đặc tả đồ án)
struct RDTHeader {
    uint32_t seq_num;     // Số thứ tự
    uint32_t ack_num;     // Số xác nhận
    uint16_t checksum;    // Kiểm tra lỗi
    uint8_t flags;        // Loại gói tin
    uint16_t payload_len; // Kích thước dữ liệu thực
};

#define PAYLOAD_SIZE 1024

struct RDTPacket {
    RDTHeader header;
    char payload[PAYLOAD_SIZE];
};

SOCKET createUDPSocket();
bool receiveFileUDP(SOCKET udpSocket, int localPort, const std::string& savePath);
bool sendFileUDP(SOCKET udpSocket, const std::string& serverIP, int serverPort, const std::string& filePath);
void closeUDPSocket(SOCKET udpSocket);

#endif
