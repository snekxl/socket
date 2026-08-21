#pragma once
#include <string>
#include <winsock2.h>
#include <ws2tcpip.h>

#define FLAG_DATA 0x01
#define FLAG_ACK  0x02
#define FLAG_FIN  0x04
#define PAYLOAD_SIZE 1024

#pragma pack(push, 1) 
struct RDTHeader {
    uint32_t seq_num;
    uint32_t ack_num;
    uint16_t checksum;
    uint8_t flags;
    uint16_t payload_len;
};

struct RDTPacket {
    RDTHeader header;
    char payload[PAYLOAD_SIZE];
};
#pragma pack(pop)

bool sendFileUDP(SOCKET udpSocket, sockaddr_in clientAddr, const std::string& filePath);
bool receiveFileUDP(SOCKET udpSocket, const std::string& savePath, bool append = false);
