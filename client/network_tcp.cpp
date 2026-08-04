#include "network_udp.h"
#include <iostream>
#include <fstream>

#pragma comment(lib, "ws2_32.lib")

using namespace std;

// Hàm tính Checksum đơn giản: cộng dồn tất cả các byte lại
uint16_t calculateChecksum(const RDTPacket& pkt) {
    uint32_t sum = 0;
    sum += pkt.header.seq_num;
    sum += pkt.header.ack_num;
    sum += pkt.header.flags;
    sum += pkt.header.payload_len;
    for (int i = 0; i < pkt.header.payload_len; ++i) {
        sum += (uint8_t)pkt.payload[i];
    }
    // Gộp phần dư để vừa khít 16 bit
    while (sum >> 16) {
        sum = (sum & 0xFFFF) + (sum >> 16);
    }
    return (uint16_t)~sum;
}

SOCKET createUDPSocket() {
    SOCKET sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock == INVALID_SOCKET) {
        cerr << "[-] Tao socket UDP that bai." << endl;
    }
    return sock;
}

bool sendFileUDP(SOCKET udpSocket, const string& serverIP, int serverPort, const string& filePath) {
    ifstream file(filePath, ios::binary);
    if (!file.is_open()) return false;

    sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(serverPort);
    inet_pton(AF_INET, serverIP.c_str(), &serverAddr.sin_addr);

    // Đặt thời gian chờ Timeout là 1000ms (1 giây)
    DWORD timeout = 1000;
    setsockopt(udpSocket, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout, sizeof(timeout));

    uint32_t current_seq = 0; // Biến trạng thái: 0 hoặc 1 (Stop and Wait)
    RDTPacket packet = {0};

    while (file.peek() != EOF) {
        // Đọc 1 chunk từ file
        file.read(packet.payload, PAYLOAD_SIZE);
        packet.header.payload_len = file.gcount();
        packet.header.seq_num = current_seq;
        packet.header.flags = FLAG_DATA;
        packet.header.checksum = 0;
        packet.header.checksum = calculateChecksum(packet);

        bool ack_received = false;
        int retries = 0;

        // Vòng lặp retransmit: Gửi và chờ ACK, lỗi thì gửi lại tối đa 5 lần
        while (!ack_received && retries < 5) {
            sendto(udpSocket, (char*)&packet, sizeof(packet), 0, (sockaddr*)&serverAddr, sizeof(serverAddr));
            
            RDTPacket ack_pkt;
            sockaddr_in fromAddr;
            int fromLen = sizeof(fromAddr);
            
            int recvBytes = recvfrom(udpSocket, (char*)&ack_pkt, sizeof(ack_pkt), 0, (sockaddr*)&fromAddr, &fromLen);
            
            // Nhận thành công và đúng ACK
            if (recvBytes > 0 && (ack_pkt.header.flags & FLAG_ACK) && ack_pkt.header.ack_num == current_seq) {
                // Kiểm tra mã băm bảo vệ
                uint16_t expected_checksum = ack_pkt.header.checksum;
                ack_pkt.header.checksum = 0;
                if (calculateChecksum(ack_pkt) == expected_checksum) {
                    ack_received = true;
                    current_seq = 1 - current_seq; // Lật bit thứ tự 0 <-> 1
                }
            } else {
                cout << "[!] Timeout hoac loi goi tin. Dang gui lai (Lan " << retries + 1 << ")...\n";
                retries++;
            }
        }
        
        if (!ack_received) {
            cerr << "[-] Ngat ket noi: Khong nhan duoc phan hoi tu Server.\n";
            return false;
        }
    }

    // Gửi gói FIN báo kết thúc
    packet.header.flags = FLAG_FIN;
    packet.header.payload_len = 0;
    packet.header.checksum = 0;
    packet.header.checksum = calculateChecksum(packet);
    sendto(udpSocket, (char*)&packet, sizeof(packet), 0, (sockaddr*)&serverAddr, sizeof(serverAddr));

    file.close();
    cout << "[+] Da gui file hoan tat qua giao thuc tin cay (RDT)." << endl;
    return true;
}

// ... (Hàm receiveFileUDP mình sẽ để nguyên cấu trúc tạm thời để tránh file quá dài, khi nào Server bắt đầu ghép nối vào, mình sẽ viết tiếp logic phía hứng dữ liệu).

void closeUDPSocket(SOCKET udpSocket) {
    if (udpSocket != INVALID_SOCKET) closesocket(udpSocket);
}
