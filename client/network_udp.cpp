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
    else {
        // --- NỚI RỘNG BỘ ĐỆM LÊN 1MB ĐỂ TRÁNH TRÀN KHI GỬI FILE NẶNG ---
        int bufferSize = 1024 * 1024; // 1 MB
        setsockopt(sock, SOL_SOCKET, SO_RCVBUF, (const char*)&bufferSize, sizeof(bufferSize));
        setsockopt(sock, SOL_SOCKET, SO_SNDBUF, (const char*)&bufferSize, sizeof(bufferSize));
    }
    return sock;
}

bool receiveFileUDP(SOCKET udpSocket, int localPort, const string& serverIP, int serverPort, const string& savePath) {
    sockaddr_in localAddr;
    localAddr.sin_family = AF_INET;
    localAddr.sin_addr.s_addr = INADDR_ANY;
    localAddr.sin_port = htons(localPort);

    if (bind(udpSocket, (sockaddr*)&localAddr, sizeof(localAddr)) == SOCKET_ERROR) {
        cerr << "[-] Bind socket UDP that bai. Ma loi: " << WSAGetLastError() << endl;
        return false;
    }

    ofstream file(savePath, ios::binary);
    if (!file.is_open()) {
        cerr << "[-] Khong the tao file de luu: " << savePath << endl;
        return false;
    }

    sockaddr_in senderAddr;
    int senderAddrSize = sizeof(senderAddr);
    uint32_t expected_seq = 0;
    long bytes_received_total = 0; // Biến đếm dung lượng đã nhận

    while (true) {
        RDTPacket packet = { 0 };
        int bytesReceived = recvfrom(udpSocket, (char*)&packet, sizeof(packet), 0, (sockaddr*)&senderAddr, &senderAddrSize);

        if (bytesReceived <= 0) continue;

        if (packet.header.flags & FLAG_FIN) {
            RDTPacket ack_pkt = { 0 };
            ack_pkt.header.flags = FLAG_ACK | FLAG_FIN;
            ack_pkt.header.ack_num = packet.header.seq_num;
            sendto(udpSocket, (char*)&ack_pkt, sizeof(ack_pkt), 0, (sockaddr*)&senderAddr, senderAddrSize);
            break;
        }

        if (packet.header.flags & FLAG_DATA) {
            RDTPacket ack_pkt = { 0 };
            ack_pkt.header.flags = FLAG_ACK;

            if (packet.header.seq_num == expected_seq) {
                file.write(packet.payload, packet.header.payload_len);
                ack_pkt.header.ack_num = expected_seq;
                sendto(udpSocket, (char*)&ack_pkt, sizeof(ack_pkt), 0, (sockaddr*)&senderAddr, senderAddrSize);
                expected_seq = 1 - expected_seq;

                // IN SỐ BYTE ĐÃ NHẬN
                bytes_received_total += packet.header.payload_len;
                cout << "\r[>] Da nhan duoc: " << bytes_received_total << " bytes..." << flush;
            }
            else {
                ack_pkt.header.ack_num = packet.header.seq_num;
                sendto(udpSocket, (char*)&ack_pkt, sizeof(ack_pkt), 0, (sockaddr*)&senderAddr, senderAddrSize);
            }
        }
    }

    file.close();
    cout << "\n[+] Da nhan va luu file thanh cong." << endl;
    return true;
}

bool sendFileUDP(SOCKET udpSocket, const string& serverIP, int serverPort, const string& filePath) {
    ifstream file(filePath, ios::binary);
    if (!file.is_open()) {
        cerr << "[-] Khong the mo file de gui: " << filePath << endl;
        return false;
    }

    // TÍNH TỔNG KÍCH THƯỚC FILE
    file.seekg(0, ios::end);
    long total_size = file.tellg();
    file.seekg(0, ios::beg);
    long bytes_acked = 0;

    sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(serverPort);
    inet_pton(AF_INET, serverIP.c_str(), &serverAddr.sin_addr);

    DWORD timeout = 1000;
    setsockopt(udpSocket, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout, sizeof(timeout));

    uint32_t current_seq = 0;
    RDTPacket packet = { 0 };

    cout << "[*] Bat dau gui file (" << total_size << " bytes)..." << endl;

    while (true) {
        file.read(packet.payload, PAYLOAD_SIZE);
        int bytesRead = file.gcount();
        if (bytesRead <= 0) break;

        packet.header.payload_len = bytesRead;
        packet.header.seq_num = current_seq;
        packet.header.flags = FLAG_DATA;

        bool ack_received = false;
        int retries = 0;

        while (!ack_received && retries < 20) {
            sendto(udpSocket, (char*)&packet, sizeof(packet), 0, (sockaddr*)&serverAddr, sizeof(serverAddr));

            RDTPacket ack_pkt;
            sockaddr_in fromAddr;
            int fromLen = sizeof(fromAddr);

            int recvBytes = recvfrom(udpSocket, (char*)&ack_pkt, sizeof(ack_pkt), 0, (sockaddr*)&fromAddr, &fromLen);

            if (recvBytes > 0) {
                if ((ack_pkt.header.flags & FLAG_ACK) && ack_pkt.header.ack_num == current_seq) {
                    ack_received = true;
                    current_seq = 1 - current_seq;

                    // TÍNH TOÁN VÀ IN TIẾN ĐỘ % (Dùng \r để ghi đè dòng)
                    bytes_acked += bytesRead;
                    int percent = (total_size > 0) ? (bytes_acked * 100LL / total_size) : 100;
                    cout << "\r[>] Tien do: " << percent << "% (" << bytes_acked << "/" << total_size << " bytes)   " << flush;
                }
            }
            else {
                retries++;
            }
        }

        if (!ack_received) {
            cerr << "\n[-] Ngat ket noi do Timeout qua 20 lan.\n";
            file.close();
            return false;
        }
    }

    packet.header.seq_num = current_seq;
    packet.header.flags = FLAG_FIN;
    packet.header.payload_len = 0;
    sendto(udpSocket, (char*)&packet, sizeof(packet), 0, (sockaddr*)&serverAddr, sizeof(serverAddr));

    Sleep(100); // CHỐNG RỚT GÓI FIN

    file.close();
    cout << "\n[+] Da gui file hoan tat qua RDT." << endl;
    return true;
}

// BỔ SUNG HÀM BỊ THIẾU
void closeUDPSocket(SOCKET udpSocket) {
    if (udpSocket != INVALID_SOCKET) {
        closesocket(udpSocket);
    }
}
