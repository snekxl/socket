#ifndef NETWORK_UDP_H
#define NETWORK_UDP_H

#include <winsock2.h>
#include <ws2tcpip.h>
#include <string>
#include <cstdint>

#define FLAG_DATA 0x01  
#define FLAG_ACK  0x02  
#define FLAG_FIN  0x04  

#pragma pack(push, 1)
struct RDTHeader {
    uint32_t seq_num;
    uint32_t ack_num;
    uint16_t checksum;
    uint8_t flags;
    uint16_t payload_len;
};

#define PAYLOAD_SIZE 1024

struct RDTPacket {
    RDTHeader header;
    char payload[PAYLOAD_SIZE];
};
#pragma pack(pop)

SOCKET createUDPSocket();
bool receiveFileUDP(SOCKET udpSocket, int localPort, const std::string& serverIP, int serverPort, const std::string& savePath);
bool sendFileUDP(SOCKET udpSocket, const std::string& serverIP, int serverPort, const std::string& filePath);
void closeUDPSocket(SOCKET udpSocket);

#endif
