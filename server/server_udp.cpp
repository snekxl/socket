#include "server_udp.h"
#include <iostream>
#include <fstream>

#pragma comment(lib, "ws2_32.lib")

using namespace std;

bool sendFileUDP(SOCKET udpSocket, sockaddr_in clientAddr, const string& filePath) {
    ifstream file(filePath, ios::binary);
    if (!file.is_open()) {
        cerr << "[-] Khong the mo file de gui: " << filePath << endl;
        return false;
    }

    file.seekg(0, ios::end);
    long total_size = file.tellg();
    file.seekg(0, ios::beg);
    long bytes_acked = 0;

    DWORD timeout = 1000;
    setsockopt(udpSocket, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout, sizeof(timeout));

    uint32_t current_seq = 0;
    RDTPacket packet = { 0 };

    cout << "[*] Bat dau gui file toi Client (" << total_size << " bytes)..." << endl;

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
            sendto(udpSocket, (char*)&packet, sizeof(packet), 0, (sockaddr*)&clientAddr, sizeof(clientAddr));

            RDTPacket ack_pkt;
            sockaddr_in fromAddr;
            int fromLen = sizeof(fromAddr);

            int recvBytes = recvfrom(udpSocket, (char*)&ack_pkt, sizeof(ack_pkt), 0, (sockaddr*)&fromAddr, &fromLen);

            if (recvBytes > 0) {
                if ((ack_pkt.header.flags & FLAG_ACK) && ack_pkt.header.ack_num == current_seq) {
                    ack_received = true;
                    current_seq = 1 - current_seq;

                    bytes_acked += bytesRead;
                    int percent = (total_size > 0) ? (bytes_acked * 100LL / total_size) : 100;
                    cout << "\r[>] Tien do: " << percent << "% (" << bytes_acked << "/" << total_size << " bytes)   " << flush;
                }
            }
            else { retries++; }
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
    sendto(udpSocket, (char*)&packet, sizeof(packet), 0, (sockaddr*)&clientAddr, sizeof(clientAddr));

    Sleep(100);

    file.close();
    cout << "\n[+] Da gui file hoan tat qua RDT." << endl;
    return true;
}

bool receiveFileUDP(SOCKET udpSocket, const string& savePath, bool append) {
    ofstream file;
    if (append) file.open(savePath, ios::binary | ios::app);
    else file.open(savePath, ios::binary);

    if (!file.is_open()) {
        cerr << "[-] Khong the tao hoac mo file de luu: " << savePath << endl;
        return false;
    }

    uint32_t expected_seq = 0;
    bool is_finished = false;
    long bytes_received_total = 0;

    DWORD timeout = 2000;
    setsockopt(udpSocket, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout, sizeof(timeout));

    while (!is_finished) {
        RDTPacket packet = { 0 };
        sockaddr_in clientAddr;
        int clientLen = sizeof(clientAddr);

        int bytes_received = recvfrom(udpSocket, (char*)&packet, sizeof(packet), 0, (sockaddr*)&clientAddr, &clientLen);

        if (bytes_received > 0) {
            if (packet.header.flags & FLAG_FIN) {
                RDTPacket ack_pkt = { 0 };
                ack_pkt.header.flags = FLAG_ACK | FLAG_FIN;
                ack_pkt.header.ack_num = packet.header.seq_num;
                sendto(udpSocket, (char*)&ack_pkt, sizeof(ack_pkt), 0, (sockaddr*)&clientAddr, sizeof(clientAddr));
                is_finished = true;
                break;
            }

            if (packet.header.flags & FLAG_DATA) {
                RDTPacket ack_pkt = { 0 };
                ack_pkt.header.flags = FLAG_ACK;

                if (packet.header.seq_num == expected_seq) {
                    file.write(packet.payload, packet.header.payload_len);
                    ack_pkt.header.ack_num = expected_seq;
                    sendto(udpSocket, (char*)&ack_pkt, sizeof(ack_pkt), 0, (sockaddr*)&clientAddr, sizeof(clientAddr));
                    expected_seq = 1 - expected_seq;

                    bytes_received_total += packet.header.payload_len;
                    cout << "\r[>] Da nhan duoc: " << bytes_received_total << " bytes tu Client..." << flush;
                }
                else {
                    ack_pkt.header.ack_num = packet.header.seq_num;
                    sendto(udpSocket, (char*)&ack_pkt, sizeof(ack_pkt), 0, (sockaddr*)&clientAddr, sizeof(clientAddr));
                }
            }
        }
    }

    file.close();
    cout << "\n[+] Da nhan va luu file thanh cong." << endl;
    return true;
}
