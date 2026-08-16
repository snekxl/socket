#include "network_udp.h"
#include <iostream>
#include <fstream>

#pragma comment(lib, "ws2_32.lib")

using namespace std;

SOCKET createUDPSocket() {
    SOCKET sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock == INVALID_SOCKET) {
        cerr << "[-] Tao socket UDP that bai. Ma loi: " << WSAGetLastError() << endl;
    }
    return sock;
}

// ==========================================================
// HÀM NHẬN FILE (DÙNG CHO LỆNH RETR / LIST)
// ==========================================================
bool receiveFileUDP(SOCKET udpSocket, int localPort, const string& serverIP, int serverPort, const string& savePath) {
    sockaddr_in localAddr;
    localAddr.sin_family = AF_INET;
    localAddr.sin_addr.s_addr = INADDR_ANY;
    localAddr.sin_port = htons(localPort);

    if (bind(udpSocket, (sockaddr*)&localAddr, sizeof(localAddr)) == SOCKET_ERROR) {
        cerr << "[-] Bind socket UDP that bai. Ma loi: " << WSAGetLastError() << endl;
        return false;
    }

    sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(serverPort);
    inet_pton(AF_INET, serverIP.c_str(), &serverAddr.sin_addr);
    
    string hello_msg = "Hello";
    sendto(udpSocket, hello_msg.c_str(), hello_msg.length(), 0, (sockaddr*)&serverAddr, sizeof(serverAddr));

    ofstream file(savePath, ios::binary);
    if (!file.is_open()) {
        cerr << "[-] Khong the tao file de luu: " << savePath << endl;
        return false;
    }

    sockaddr_in senderAddr;
    int senderAddrSize = sizeof(senderAddr);
    uint32_t expected_seq = 0;

    while (true) {
        RDTPacket packet;
        int bytesReceived = recvfrom(udpSocket, (char*)&packet, sizeof(packet), 0, (sockaddr*)&senderAddr, &senderAddrSize);
        
        if (bytesReceived <= 0) continue;

        if (packet.header.flags & FLAG_FIN) {
            RDTPacket ack_pkt = {0};
            ack_pkt.header.flags = FLAG_ACK | FLAG_FIN;
            ack_pkt.header.ack_num = packet.header.seq_num;
            sendto(udpSocket, (char*)&ack_pkt, sizeof(ack_pkt), 0, (sockaddr*)&senderAddr, senderAddrSize);
            break;
        }

        if (packet.header.flags & FLAG_DATA) {
            RDTPacket ack_pkt = {0};
            ack_pkt.header.flags = FLAG_ACK;

            if (packet.header.seq_num == expected_seq) {
                file.write(packet.payload, packet.header.payload_len);
                ack_pkt.header.ack_num = expected_seq;
                sendto(udpSocket, (char*)&ack_pkt, sizeof(ack_pkt), 0, (sockaddr*)&senderAddr, senderAddrSize);
                expected_seq = 1 - expected_seq; 
            } else {
                ack_pkt.header.ack_num = packet.header.seq_num; 
                sendto(udpSocket, (char*)&ack_pkt, sizeof(ack_pkt), 0, (sockaddr*)&senderAddr, senderAddrSize);
            }
        }
    }

    file.close();
    cout << "[+] Da nhan va luu file thanh cong." << endl;
    return true;
}

// ==========================================================
// HÀM GỬI FILE (DÙNG CHO LỆNH STOR)
// ==========================================================
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

    DWORD timeout = 1000;
    setsockopt(udpSocket, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout, sizeof(timeout));

    uint32_t current_seq = 0; 
    RDTPacket packet = {0};

    while (file.peek() != EOF) {
        file.read(packet.payload, PAYLOAD_SIZE);
        packet.header.payload_len = file.gcount();
        packet.header.seq_num = current_seq;
        packet.header.flags = FLAG_DATA;

        bool ack_received = false;
        int retries = 0;

        while (!ack_received && retries < 5) {
            sendto(udpSocket, (char*)&packet, sizeof(packet), 0, (sockaddr*)&serverAddr, sizeof(serverAddr));
            
            RDTPacket ack_pkt;
            sockaddr_in fromAddr;
            int fromLen = sizeof(fromAddr);
            
            int recvBytes = recvfrom(udpSocket, (char*)&ack_pkt, sizeof(ack_pkt), 0, (sockaddr*)&fromAddr, &fromLen);
            
            if (recvBytes > 0 && (ack_pkt.header.flags & FLAG_ACK) && ack_pkt.header.ack_num == current_seq) {
                ack_received = true;
                current_seq = 1 - current_seq; 
            } else {
                retries++;
            }
        }
        
        if (!ack_received) {
            cerr << "[-] Ngat ket noi do Timeout qua 5 lan.\n";
            file.close();
            return false;
        }
    }

    packet.header.seq_num = current_seq;
    packet.header.flags = FLAG_FIN;
    packet.header.payload_len = 0;
    sendto(udpSocket, (char*)&packet, sizeof(packet), 0, (sockaddr*)&serverAddr, sizeof(serverAddr));

    file.close();
    cout << "[+] Da gui file hoan tat qua giao thuc tin cay (RDT)." << endl;
    return true;
}

void closeUDPSocket(SOCKET udpSocket) {
    if (udpSocket != INVALID_SOCKET) {
        closesocket(udpSocket);
    }
}
