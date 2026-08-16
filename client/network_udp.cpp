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
        cerr << "[-] Tao socket UDP that bai. Ma loi: " << WSAGetLastError() << endl;
    }
    return sock;
}

// ==========================================================
// HÀM NHẬN FILE (DÙNG CHO LỆNH RETR HOẶC SERVER NHẬN STOR)
// ==========================================================
// 1. Đổi chữ ký hàm (Thêm serverIP và serverPort)
bool receiveFileUDP(SOCKET udpSocket, int localPort, const string& serverIP, int serverPort, const string& savePath) {
    sockaddr_in localAddr;
    localAddr.sin_family = AF_INET;
    localAddr.sin_addr.s_addr = INADDR_ANY;
    localAddr.sin_port = htons(localPort);

    // Mở port chờ dữ liệu
    if (bind(udpSocket, (sockaddr*)&localAddr, sizeof(localAddr)) == SOCKET_ERROR) {
        cerr << "[-] Bind socket UDP that bai. Ma loi: " << WSAGetLastError() << endl;
        return false;
    }

    // =========================================================
    // CODE FIX LỖI DEADLOCK: GỬI GÓI "HELLO" ĐỂ KÍCH HOẠT SERVER
    // =========================================================
    sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(serverPort);
    inet_pton(AF_INET, serverIP.c_str(), &serverAddr.sin_addr);
    
    string hello_msg = "Hello";
    sendto(udpSocket, hello_msg.c_str(), hello_msg.length(), 0, (sockaddr*)&serverAddr, sizeof(serverAddr));
    cout << "[*] Da gui goi 'Hello' de danh thuc Server..." << endl;
    // =========================================================

    ofstream file(savePath, ios::binary);
    if (!file.is_open()) {
        cerr << "[-] Khong the tao file de luu: " << savePath << endl;
        return false;
    }

    sockaddr_in senderAddr;
    int senderAddrSize = sizeof(senderAddr);
    uint32_t expected_seq = 0; // Mong đợi nhận gói số 0 đầu tiên

    cout << "[*] Dang cho nhan du lieu tren port UDP " << localPort << "..." << endl;

    // ... (Giữ nguyên toàn bộ vòng lặp while(true) phía dưới của bạn) ...

    while (true) {
        RDTPacket packet;
        int bytesReceived = recvfrom(udpSocket, (char*)&packet, sizeof(packet), 0, (sockaddr*)&senderAddr, &senderAddrSize);
        
        if (bytesReceived <= 0) continue;

        // 1. Kiểm tra tính toàn vẹn (Checksum)
        uint16_t received_checksum = packet.header.checksum;
        packet.header.checksum = 0; // Phải reset về 0 để tính toán lại
        if (calculateChecksum(packet) != received_checksum) {
            cout << "[!] Phat hien loi bit (Checksum failed). Loai bo goi tin." << endl;
            continue; // Lỗi thì ngơ đi, để bên kia tự timeout rồi gửi lại
        }

        // 2. Xử lý gói tin FIN (Kết thúc)
        if (packet.header.flags & FLAG_FIN) {
            // Phản hồi lại ACK cho FIN
            RDTPacket ack_pkt = {0};
            ack_pkt.header.flags = FLAG_ACK | FLAG_FIN;
            ack_pkt.header.ack_num = packet.header.seq_num;
            ack_pkt.header.checksum = calculateChecksum(ack_pkt);
            sendto(udpSocket, (char*)&ack_pkt, sizeof(ack_pkt), 0, (sockaddr*)&senderAddr, senderAddrSize);
            break; // Thoát vòng lặp, hoàn tất nhận file
        }

        // 3. Xử lý gói tin DATA (Dữ liệu)
        if (packet.header.flags & FLAG_DATA) {
            RDTPacket ack_pkt = {0};
            ack_pkt.header.flags = FLAG_ACK;

            if (packet.header.seq_num == expected_seq) {
                // Đúng thứ tự: Ghi vào file và lật bit chờ
                file.write(packet.payload, packet.header.payload_len);
                
                ack_pkt.header.ack_num = expected_seq;
                ack_pkt.header.checksum = calculateChecksum(ack_pkt);
                sendto(udpSocket, (char*)&ack_pkt, sizeof(ack_pkt), 0, (sockaddr*)&senderAddr, senderAddrSize);
                
                expected_seq = 1 - expected_seq; // Lật bit 0 <-> 1
            } else {
                // Nhận nhầm gói tin cũ (Duplicate): Vẫn phải gửi ACK để bên kia qua bước
                ack_pkt.header.ack_num = packet.header.seq_num; 
                ack_pkt.header.checksum = calculateChecksum(ack_pkt);
                sendto(udpSocket, (char*)&ack_pkt, sizeof(ack_pkt), 0, (sockaddr*)&senderAddr, senderAddrSize);
            }
        }
    }

    file.close();
    cout << "[+] Da nhan va luu file thanh cong qua RDT: " << savePath << endl;
    return true;
}

// ==========================================================
// HÀM GỬI FILE (DÙNG CHO LỆNH STOR HOẶC SERVER XỬ LÝ RETR)
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

    // Đặt thời gian chờ Timeout là 1000ms (1 giây)
    DWORD timeout = 1000;
    setsockopt(udpSocket, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout, sizeof(timeout));

    uint32_t current_seq = 0; // Stop and Wait
    RDTPacket packet = {0};

    while (file.peek() != EOF) {
        file.read(packet.payload, PAYLOAD_SIZE);
        packet.header.payload_len = file.gcount();
        packet.header.seq_num = current_seq;
        packet.header.flags = FLAG_DATA;
        packet.header.checksum = 0;
        packet.header.checksum = calculateChecksum(packet);

        bool ack_received = false;
        int retries = 0;

        // Gửi và chờ ACK, nếu mất thì gửi lại tối đa 5 lần
        while (!ack_received && retries < 5) {
            sendto(udpSocket, (char*)&packet, sizeof(packet), 0, (sockaddr*)&serverAddr, sizeof(serverAddr));
            
            RDTPacket ack_pkt;
            sockaddr_in fromAddr;
            int fromLen = sizeof(fromAddr);
            
            int recvBytes = recvfrom(udpSocket, (char*)&ack_pkt, sizeof(ack_pkt), 0, (sockaddr*)&fromAddr, &fromLen);
            
            if (recvBytes > 0 && (ack_pkt.header.flags & FLAG_ACK) && ack_pkt.header.ack_num == current_seq) {
                // Checksum ACK
                uint16_t expected_checksum = ack_pkt.header.checksum;
                ack_pkt.header.checksum = 0;
                if (calculateChecksum(ack_pkt) == expected_checksum) {
                    ack_received = true;
                    current_seq = 1 - current_seq; // Lật bit thứ tự
                }
            } else {
                cout << "[!] Timeout hoac loi goi tin. Dang gui lai (Lan " << retries + 1 << ")...\n";
                retries++;
            }
        }
        
        if (!ack_received) {
            cerr << "[-] Ngat ket noi: Khong nhan duoc phan hoi tu phia dich.\n";
            file.close();
            return false;
        }
    }

    // Gửi gói FIN báo kết thúc
    packet.header.flags = FLAG_FIN;
    packet.header.payload_len = 0;
    packet.header.checksum = 0;
    packet.header.checksum = calculateChecksum(packet);
    
    // Đảm bảo gói FIN cũng phải được gửi thành công (đơn giản hóa thì gửi 1 lần)
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
