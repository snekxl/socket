#include <iostream>
#include <string>
#include <vector>
#include <fstream>
#include <winsock2.h>
#include <ws2tcpip.h>

// Link với thư viện Winsock của Windows
#pragma comment(lib, "ws2_32.lib")

using namespace std;

// =====================================================================
// HÀM XỬ LÝ TRUYỀN FILE UDP (THUẬT TOÁN STOP-AND-WAIT)
// =====================================================================
bool sendFileUDP(SOCKET udp_sock, sockaddr_in client_addr, const string& filename) {
    ifstream file(filename, ios::binary);
    if (!file.is_open()) {
        cerr << "[UDP] Loi: Khong the mo file " << filename << endl;
        return false;
    }

    int seq_num = 0;
    int client_len = sizeof(client_addr);
    char buffer[1022]; // Dành 2 byte cho Header: [SeqNum][EOF]

    while (!file.eof()) {
        file.read(buffer, sizeof(buffer));
        int bytes_read = file.gcount();

        if (bytes_read == 0) break;

        char is_eof = file.eof() ? 1 : 0;

        // Đóng gói Header + Data
        vector<char> packet;
        packet.push_back((char)seq_num);
        packet.push_back(is_eof);
        packet.insert(packet.end(), buffer, buffer + bytes_read);

        bool ack_received = false;

        // Vòng lặp Dừng-và-Chờ
        while (!ack_received) {
            sendto(udp_sock, packet.data(), packet.size(), 0, (sockaddr*)&client_addr, client_len);
            cout << "[UDP] Da gui goi Seq=" << seq_num << " (Kich thuoc: " << packet.size() << " bytes)." << endl;

            // Timeout = 2 giây
            fd_set read_fds;
            FD_ZERO(&read_fds);
            FD_SET(udp_sock, &read_fds);

            timeval timeout;
            timeout.tv_sec = 2;
            timeout.tv_usec = 0;

            int activity = select(0, &read_fds, NULL, NULL, &timeout);

            if (activity == 0) {
                cout << "[UDP] TIMEOUT! Dang gui lai goi Seq=" << seq_num << "..." << endl;
            }
            else if (activity > 0) {
                char ack_buffer[10];
                sockaddr_in sender_addr;
                int sender_len = sizeof(sender_addr);

                recvfrom(udp_sock, ack_buffer, sizeof(ack_buffer), 0, (sockaddr*)&sender_addr, &sender_len);

                int ack_num = ack_buffer[0];
                if (ack_num == seq_num) {
                    cout << "[UDP] Nhan ACK=" << ack_num << " thanh cong." << endl;
                    seq_num = 1 - seq_num; // Đảo bit 0 <-> 1
                    ack_received = true;
                }
                else {
                    cout << "[UDP] Nhan ACK sai. Bo qua." << endl;
                }
            }
        }
    }

    file.close();
    cout << "[UDP] HOAN TAT TRUYEN FILE!" << endl;
    return true;
}


// =====================================================================
// HÀM MAIN: KÊNH ĐIỀU KHIỂN TCP VÀ ĐỊNH TUYẾN LỆNH
// =====================================================================
int main() {
    cout << "=== HYBRID FTP SERVER ===" << endl;

    // 1. Khởi tạo Winsock
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        cerr << "Loi khoi tao Winsock!" << endl;
        return 1;
    }

    // 2. Tạo TCP Socket cho Cửa Chính (Control Channel)
    SOCKET server_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(2121);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    bind(server_sock, (sockaddr*)&server_addr, sizeof(server_addr));
    listen(server_sock, 1);
    cout << "[TCP] Server dang lang nghe tren cong 2121..." << endl;

    while (true) { // Vòng lặp chờ Client kết nối
        SOCKET client_sock = accept(server_sock, NULL, NULL);
        if (client_sock == INVALID_SOCKET) continue;

        cout << "\n[He thong] Client da ket noi thanh cong!" << endl;

        string welcome_msg = "220 Welcome to Hybrid FTP Server\r\n";
        send(client_sock, welcome_msg.c_str(), welcome_msg.length(), 0);

        char buffer[1024];
        while (true) { // Vòng lặp nhận lệnh từ Client
            memset(buffer, 0, sizeof(buffer));
            int bytes_received = recv(client_sock, buffer, sizeof(buffer) - 1, 0);

            if (bytes_received <= 0) {
                cout << "[He thong] Client da ngat ket noi." << endl;
                break;
            }

            string request(buffer);
            cout << "[CLIENT] " << request;

            string response = "";

            // --- ĐỊNH TUYẾN CÁC LỆNH ---
            if (request.find("QUIT") == 0) {
                response = "221 Goodbye. Hen gap lai!\r\n";
                send(client_sock, response.c_str(), response.length(), 0);
                break;
            }
            else if (request.find("USER") == 0) {
                response = "331 Username OK, need password.\r\n";
                send(client_sock, response.c_str(), response.length(), 0);
            }
            else if (request.find("PASS") == 0) {
                response = "230 User logged in, proceed.\r\n";
                send(client_sock, response.c_str(), response.length(), 0);
            }
            else if (request.find("RETR") == 0) {
                // Tách tên file
                string filename = request.substr(5);
                filename.erase(filename.find_last_not_of(" \n\r\t") + 1);

                response = "150 File okay. Dang mo kenh UDP tren cong 2020...\r\n";
                send(client_sock, response.c_str(), response.length(), 0);

                // --- MỞ KÊNH DỮ LIỆU UDP ---
                SOCKET udp_sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
                sockaddr_in udp_server_addr;
                udp_server_addr.sin_family = AF_INET;
                udp_server_addr.sin_port = htons(2020);
                udp_server_addr.sin_addr.s_addr = INADDR_ANY;
                bind(udp_sock, (sockaddr*)&udp_server_addr, sizeof(udp_server_addr));

                cout << "[UDP] Doi Client gui goi 'Hello' de xac nhan IP/Port..." << endl;

                // Hứng gói tin "chào sân" từ Client
                char dummy_buffer[10];
                sockaddr_in client_udp_addr;
                int client_udp_len = sizeof(client_udp_addr);
                recvfrom(udp_sock, dummy_buffer, sizeof(dummy_buffer), 0, (sockaddr*)&client_udp_addr, &client_udp_len);

                // Gọi hàm truyền file bằng Stop-and-Wait
                if (sendFileUDP(udp_sock, client_udp_addr, filename)) {
                    string success_msg = "226 Transfer complete.\r\n";
                    send(client_sock, success_msg.c_str(), success_msg.length(), 0);
                }
                else {
                    string fail_msg = "550 File unavailable hoac loi doc file.\r\n";
                    send(client_sock, fail_msg.c_str(), fail_msg.length(), 0);
                }
                closesocket(udp_sock);
            }
            else {
                response = "502 Command not implemented.\r\n";
                send(client_sock, response.c_str(), response.length(), 0);
            }
        }
        closesocket(client_sock);
    }

    closesocket(server_sock);
    WSACleanup();
    return 0;
}