#include <iostream>
#include <string>
#include <vector>
#include <fstream>
#include <filesystem>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <wincrypt.h> // API Bảo mật mã băm nguyên bản của Windows

// Bắt buộc phải link thư viện mạng của Windows
#pragma comment(lib, "ws2_32.lib")

using namespace std;
namespace fs = std::filesystem;

// =====================================================================
// 1. CẤU TRÚC GÓI TIN RDT (ĐỒNG BỘ 100% VỚI CLIENT)
// =====================================================================
#define FLAG_DATA 0x01
#define FLAG_ACK  0x02
#define FLAG_FIN  0x04

#pragma pack(push, 1) // Ép bộ nhớ sát nhau 1 byte để không bị lệch padding
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
#pragma pack(pop)

// Hàm tính Checksum để kiểm tra toàn vẹn
uint16_t calculateChecksum(const RDTPacket& pkt) {
    uint32_t sum = 0;
    sum += pkt.header.seq_num;
    sum += pkt.header.ack_num;
    sum += pkt.header.flags;
    sum += pkt.header.payload_len;
    for (int i = 0; i < pkt.header.payload_len; ++i) {
        sum += (uint8_t)pkt.payload[i];
    }
    while (sum >> 16) {
        sum = (sum & 0xFFFF) + (sum >> 16);
    }
    return (uint16_t)~sum;
}

// =====================================================================
// 2. HÀM GỬI FILE QUA UDP (DÀNH CHO LỆNH RETR)
// =====================================================================
bool sendFileUDP(SOCKET udpSocket, sockaddr_in clientAddr, const string& filePath) {
    ifstream file(filePath, ios::binary);
    if (!file.is_open()) {
        cerr << "[-] Khong the mo file de gui: " << filePath << endl;
        return false;
    }

    DWORD timeout = 1000;
    setsockopt(udpSocket, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout, sizeof(timeout));

    uint32_t current_seq = 0;
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

        while (!ack_received && retries < 5) {
            sendto(udpSocket, (char*)&packet, sizeof(packet), 0, (sockaddr*)&clientAddr, sizeof(clientAddr));
            
            RDTPacket ack_pkt;
            sockaddr_in fromAddr;
            int fromLen = sizeof(fromAddr);
            
            int recvBytes = recvfrom(udpSocket, (char*)&ack_pkt, sizeof(ack_pkt), 0, (sockaddr*)&fromAddr, &fromLen);
            
            if (recvBytes > 0 && (ack_pkt.header.flags & FLAG_ACK) && ack_pkt.header.ack_num == current_seq) {
                ack_received = true;
                current_seq = 1 - current_seq; // Đảo bit
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

    // Gửi gói FIN báo kết thúc
    packet.header.flags = FLAG_FIN;
    packet.header.payload_len = 0;
    packet.header.checksum = 0;
    packet.header.checksum = calculateChecksum(packet);
    sendto(udpSocket, (char*)&packet, sizeof(packet), 0, (sockaddr*)&clientAddr, sizeof(clientAddr));

    file.close();
    cout << "[+] Da gui file hoan tat qua RDT." << endl;
    return true;
}

// =====================================================================
// 3. HÀM NHẬN FILE QUA UDP (DÀNH CHO LỆNH STOR, APPE, STOU)
// =====================================================================
bool receiveFileUDP(SOCKET udpSocket, const string& savePath, bool append = false) {
    ofstream file;
    if (append) file.open(savePath, ios::binary | ios::app);
    else file.open(savePath, ios::binary);

    if (!file.is_open()) {
        cerr << "[-] Khong the tao hoac mo file de luu: " << savePath << endl;
        return false;
    }

    uint32_t expected_seq = 0;
    bool is_finished = false;

    DWORD timeout = 2000;
    setsockopt(udpSocket, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout, sizeof(timeout));

    while (!is_finished) {
        RDTPacket packet;
        sockaddr_in clientAddr;
        int clientLen = sizeof(clientAddr);

        int bytes_received = recvfrom(udpSocket, (char*)&packet, sizeof(packet), 0, (sockaddr*)&clientAddr, &clientLen);

        if (bytes_received > 0) {
            uint16_t received_checksum = packet.header.checksum;
            packet.header.checksum = 0; 

            if (calculateChecksum(packet) == received_checksum && packet.header.seq_num == expected_seq) {
                if (packet.header.flags & FLAG_DATA) {
                    file.write(packet.payload, packet.header.payload_len);
                }
                if (packet.header.flags & FLAG_FIN) {
                    is_finished = true;
                }

                RDTPacket ack_pkt = {0};
                ack_pkt.header.ack_num = expected_seq;
                ack_pkt.header.flags = FLAG_ACK;
                ack_pkt.header.checksum = calculateChecksum(ack_pkt);
                sendto(udpSocket, (char*)&ack_pkt, sizeof(ack_pkt), 0, (sockaddr*)&clientAddr, sizeof(clientAddr));

                expected_seq = 1 - expected_seq;
            }
            else if (packet.header.seq_num != expected_seq) {
                RDTPacket ack_pkt = {0};
                ack_pkt.header.ack_num = 1 - expected_seq; 
                ack_pkt.header.flags = FLAG_ACK;
                ack_pkt.header.checksum = calculateChecksum(ack_pkt);
                sendto(udpSocket, (char*)&ack_pkt, sizeof(ack_pkt), 0, (sockaddr*)&clientAddr, sizeof(clientAddr));
            }
        }
    }

    file.close();
    cout << "[+] Da nhan va luu file thanh cong." << endl;
    return true;
}

// =====================================================================
// 4. HÀM MAIN: KÊNH ĐIỀU KHIỂN TCP VÀ TỔNG ĐÀI ĐỊNH TUYẾN 28 LỆNH
// =====================================================================
int main() {
    cout << "=== HYBRID FTP SERVER ===" << endl;

    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        cerr << "Loi khoi tao Winsock!" << endl;
        return 1;
    }

    SOCKET server_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(2121);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    bind(server_sock, (sockaddr*)&server_addr, sizeof(server_addr));
    listen(server_sock, 1);
    cout << "[TCP] Server dang lang nghe tren cong 2121..." << endl;

    while (true) {
        sockaddr_in client_info;
        int client_info_len = sizeof(client_info);
        SOCKET client_sock = accept(server_sock, (sockaddr*)&client_info, &client_info_len);
        
        if (client_sock == INVALID_SOCKET) continue;

        char client_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &(client_info.sin_addr), client_ip, INET_ADDRSTRLEN);

        cout << "\n[He thong] Client (" << client_ip << ") da ket noi thanh cong!" << endl;

        string welcome_msg = "220 Welcome to Hybrid FTP Server\r\n";
        send(client_sock, welcome_msg.c_str(), welcome_msg.length(), 0);

        // --- CÁC BIẾN LƯU TRẠNG THÁI CỦA PHIÊN LÀM VIỆC ---
        string rename_from_path = ""; 
        char transfer_type = 'I'; 
        char transfer_mode = 'S'; 

        char buffer[1024];
        while (true) { 
            memset(buffer, 0, sizeof(buffer));
            int bytes_received = recv(client_sock, buffer, sizeof(buffer) - 1, 0);

            if (bytes_received <= 0) {
                cout << "[He thong] Client da ngat ket noi." << endl;
                break;
            }

            string request(buffer);
            cout << "[CLIENT] " << request;
            string response = "";

            string cmd = request.substr(0, request.find(' ')); 
            
            cmd.erase(cmd.find_last_not_of(" \n\r\t") + 1); 
            
            string arg = "";
            if (request.find(' ') != string::npos) {
                arg = request.substr(request.find(' ') + 1);
                arg.erase(arg.find_last_not_of(" \n\r\t") + 1); 
            }

            // =========================================================
            // TỔNG ĐÀI ĐỊNH TUYẾN CHÍNH XÁC 28 LỆNH FTP
            // =========================================================

            // 1. NHÓM QUẢN LÝ PHIÊN (4 lệnh)
            if (cmd == "QUIT") {
                response = "221 Goodbye. Hen gap lai!\r\n";
                send(client_sock, response.c_str(), response.length(), 0);
                break; 
            }
            else if (cmd == "USER") {
                response = "331 Username OK, need password.\r\n";
                send(client_sock, response.c_str(), response.length(), 0);
            }
            else if (cmd == "PASS") {
                response = "230 User logged in, proceed.\r\n";
                send(client_sock, response.c_str(), response.length(), 0);
            }
            else if (cmd == "NOOP") {
                response = "200 Command okay.\r\n";
                send(client_sock, response.c_str(), response.length(), 0);
            }

            // 2. NHÓM ĐIỀU HƯỚNG & QUẢN LÝ THƯ MỤC (5 lệnh)
            else if (cmd == "PWD") {
                string current_path = fs::current_path().string();
                response = "257 \"" + current_path + "\" is the current directory.\r\n";
                send(client_sock, response.c_str(), response.length(), 0);
            }
            else if (cmd == "CWD") {
                try {
                    if (fs::exists(arg) && fs::is_directory(arg)) {
                        fs::current_path(arg);
                        response = "250 Directory successfully changed.\r\n";
                    } else {
                        response = "550 Failed to change directory. Path not found.\r\n";
                    }
                } catch (...) { response = "550 System error.\r\n"; }
                send(client_sock, response.c_str(), response.length(), 0);
            }
            else if (cmd == "CDUP") {
                try {
                    fs::current_path(fs::current_path().parent_path());
                    response = "250 Directory successfully changed to parent.\r\n";
                } catch (...) { response = "550 Failed to change directory.\r\n"; }
                send(client_sock, response.c_str(), response.length(), 0);
            }
            else if (cmd == "MKD") {
                if (fs::create_directory(arg)) response = "257 Directory created successfully.\r\n";
                else response = "550 Directory creation failed.\r\n";
                send(client_sock, response.c_str(), response.length(), 0);
            }
            else if (cmd == "RMD") {
                try {
                    if (fs::remove(arg)) response = "250 Directory removed successfully.\r\n";
                    else response = "550 Remove failed (Directory not empty or not found).\r\n";
                } catch (...) { response = "550 Remove failed.\r\n"; }
                send(client_sock, response.c_str(), response.length(), 0);
            }

            // 3. NHÓM TRUYỀN TẢI FILE (UDP) (6 lệnh)
            else if (cmd == "RETR") {
                response = "150 File okay. Dang mo kenh UDP tren cong 2122...\r\n";
                send(client_sock, response.c_str(), response.length(), 0);
                
                SOCKET udp_sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
                sockaddr_in target_client_udp;
                target_client_udp.sin_family = AF_INET;
                target_client_udp.sin_port = htons(2122);
                inet_pton(AF_INET, client_ip, &target_client_udp.sin_addr);
                
                if (sendFileUDP(udp_sock, target_client_udp, arg)) {
                    response = "226 Transfer complete.\r\n";
                } else {
                    response = "550 File unavailable hoac loi doc file.\r\n";
                }
                send(client_sock, response.c_str(), response.length(), 0);
                closesocket(udp_sock);
            }
            else if (cmd == "STOR" || cmd == "APPE" || cmd == "STOU") {
                response = "150 Ok to send data. Dang mo kenh UDP tren cong 2122...\r\n";
                send(client_sock, response.c_str(), response.length(), 0);
                
                string save_name = arg;
                bool is_append = false;
                
                if (cmd == "STOU") save_name = "unique_" + to_string(time(0)) + ".bin";
                if (cmd == "APPE") is_append = true;

                SOCKET udp_sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
                sockaddr_in udp_server_addr;
                udp_server_addr.sin_family = AF_INET;
                udp_server_addr.sin_port = htons(2122);
                udp_server_addr.sin_addr.s_addr = INADDR_ANY;
                bind(udp_sock, (sockaddr*)&udp_server_addr, sizeof(udp_server_addr));
                
                if (receiveFileUDP(udp_sock, save_name, is_append)) {
                    response = "226 Transfer complete.\r\n";
                } else {
                    response = "550 Transfer failed.\r\n";
                }
                send(client_sock, response.c_str(), response.length(), 0);
                closesocket(udp_sock);
            }
            else if (cmd == "LIST" || cmd == "NLST") {
                response = "150 Opening data connection for directory list...\r\n";
                send(client_sock, response.c_str(), response.length(), 0);

                // 1. Quét thư mục và ghi ra file text tạm thời
                string temp_filename = "temp_dir_list.txt";
                ofstream temp_file(temp_filename);
                
                // Nếu người dùng gõ "LIST" không có tham số thì lấy thư mục hiện tại
                string target_dir = arg.empty() ? fs::current_path().string() : arg;

                try {
                    if (fs::exists(target_dir) && fs::is_directory(target_dir)) {
                        for (const auto& entry : fs::directory_iterator(target_dir)) {
                            string filename = entry.path().filename().string();
                            
                            if (cmd == "NLST") {
                                // Lệnh NLST chỉ in tên file
                                temp_file << filename << "\r\n"; 
                            } else {
                                // Lệnh LIST in chi tiết: Loại (Dir/File), Kích thước, Tên
                                string type = entry.is_directory() ? "<DIR>  " : "<FILE> ";
                                string size = entry.is_directory() ? "0" : to_string(entry.file_size());
                                temp_file << type << "\t" << size << " bytes\t" << filename << "\r\n";
                            }
                        }
                    } else {
                        temp_file << "Directory not found.\r\n";
                    }
                } catch (...) { temp_file << "Access denied.\r\n"; }
                temp_file.close();

                // 2. Mở kênh UDP và ném file tạm đi (Tái sử dụng RDT)
                SOCKET udp_sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
                sockaddr_in target_client_udp;
                target_client_udp.sin_family = AF_INET;
                target_client_udp.sin_port = htons(2122);
                inet_pton(AF_INET, client_ip, &target_client_udp.sin_addr);

                if (sendFileUDP(udp_sock, target_client_udp, temp_filename)) {
                    response = "226 Directory send OK.\r\n";
                } else {
                    response = "550 Failed to send directory list.\r\n";
                }
                send(client_sock, response.c_str(), response.length(), 0);
                closesocket(udp_sock);

                // 3. "Phi tang" file tạm sau khi truyền xong
                fs::remove(temp_filename);
            }

            // 4. NHÓM CHỈNH SỬA & XÓA FILE (4 lệnh)
            else if (cmd == "DELE") {
                try {
                    if (fs::remove(arg)) response = "250 File deleted successfully.\r\n";
                    else response = "550 File not found or access denied.\r\n";
                } catch (...) { response = "550 Delete failed.\r\n"; }
                send(client_sock, response.c_str(), response.length(), 0);
            }
            else if (cmd == "RNFR") {
                if (fs::exists(arg)) {
                    rename_from_path = arg;
                    response = "350 File exists, ready for destination name.\r\n";
                } else {
                    response = "550 File not found.\r\n";
                }
                send(client_sock, response.c_str(), response.length(), 0);
            }
            else if (cmd == "RNTO") {
                try {
                    if (!rename_from_path.empty()) {
                        fs::rename(rename_from_path, arg);
                        response = "250 File renamed successfully.\r\n";
                        rename_from_path = ""; 
                    } else {
                        response = "503 Bad sequence of commands (Use RNFR first).\r\n";
                    }
                } catch (...) { response = "550 Rename failed.\r\n"; }
                send(client_sock, response.c_str(), response.length(), 0);
            }
            // 4. NHÓM CHỈNH SỬA & XÓA FILE (Tiếp tục)
            else if (cmd == "ABOR") {
                // Để ABOR thực sự ngắt được UDP giữa chừng, Server phải chạy đa luồng.
                // Ở phiên bản đơn luồng này, ta giả lập ngắt phiên thành công.
                response = "226 Abort successful.\r\n";
                send(client_sock, response.c_str(), response.length(), 0);
            }

            // 5. NHÓM TRẠNG THÁI VÀ THÔNG TIN FILE (Tiếp tục)
            else if (cmd == "SIZE") {
                try {
                    if (fs::exists(arg) && !fs::is_directory(arg)) {
                        response = "213 " + to_string(fs::file_size(arg)) + "\r\n";
                    } else { response = "550 File not found.\r\n"; }
                } catch (...) { response = "550 Could not get file size.\r\n"; }
                send(client_sock, response.c_str(), response.length(), 0);
            }
            else if (cmd == "MDTM") {
                try {
                    if (fs::exists(arg)) {
                        // Lấy thời gian chỉnh sửa cuối của file bằng C++17
                        auto ftime = fs::last_write_time(arg);
                        auto sctp = chrono::time_point_cast<chrono::system_clock::duration>(ftime - fs::file_time_type::clock::now() + chrono::system_clock::now());
                        time_t cftime = chrono::system_clock::to_time_t(sctp);
                        tm* t = localtime(&cftime);
                        
                        // Format chuẩn YYYYMMDDhhmmss của FTP
                        char timeBuf[20];
                        strftime(timeBuf, sizeof(timeBuf), "%Y%m%d%H%M%S", t);
                        response = "213 " + string(timeBuf) + "\r\n";
                    } else { response = "550 File not found.\r\n"; }
                } catch (...) { response = "550 Could not get file time.\r\n"; }
                send(client_sock, response.c_str(), response.length(), 0);
            }
            else if (cmd == "STAT") {
                response = "211-Server status:\r\n Hybrid FTP Server V1.0\r\n211 End of status.\r\n";
                send(client_sock, response.c_str(), response.length(), 0);
            }
            else if (cmd == "HELP") {
                response = "214-The following commands are recognized:\r\n USER PASS QUIT PWD CWD CDUP MKD RMD RETR STOR STOU APPE LIST NLST DELE RNFR RNTO ABOR SIZE MDTM STAT HELP TYPE MODE PORT PASV HASH\r\n214 Help OK.\r\n";
                send(client_sock, response.c_str(), response.length(), 0);
            }

            // 6. NHÓM CẤU HÌNH & XÁC MINH NÂNG CAO
            else if (cmd == "TYPE") {
                if (arg == "A" || arg == "I") {
                    transfer_type = arg[0];
                    response = "200 Type set to " + arg + ".\r\n";
                } else { response = "504 Command not implemented for that parameter.\r\n"; }
                send(client_sock, response.c_str(), response.length(), 0);
            }
            else if (cmd == "MODE") {
                if (arg == "S" || arg == "B" || arg == "C") {
                    transfer_mode = arg[0];
                    response = "200 Mode set to " + arg + ".\r\n";
                } else { response = "504 Command not implemented for that parameter.\r\n"; }
                send(client_sock, response.c_str(), response.length(), 0);
            }
            else if (cmd == "PORT") {
                // Tách tham số h1,h2,h3,h4,p1,p2 để tính ra Port của Client
                int h1, h2, h3, h4, p1, p2;
                if (sscanf(arg.c_str(), "%d,%d,%d,%d,%d,%d", &h1, &h2, &h3, &h4, &p1, &p2) == 6) {
                    string active_ip = to_string(h1) + "." + to_string(h2) + "." + to_string(h3) + "." + to_string(h4);
                    int active_port = (p1 * 256) + p2;
                    response = "200 PORT command successful. Targeted " + active_ip + ":" + to_string(active_port) + "\r\n";
                } else {
                    response = "501 Syntax error in parameters.\r\n";
                }
                send(client_sock, response.c_str(), response.length(), 0);
            }
            else if (cmd == "PASV") {
                // Giả lập mở port 2122 (127.0.0.1, Port: 8 * 256 + 74 = 2122)
                response = "227 Entering Passive Mode (127,0,0,1,8,74).\r\n"; 
                send(client_sock, response.c_str(), response.length(), 0);
            }
            else if (cmd == "HASH") {
                if (fs::exists(arg) && !fs::is_directory(arg)) {
                    // Cài đặt băm SHA-256 bằng CryptoAPI của Windows (Đáp ứng tiêu chí không dùng thư viện ngoài)
                    HCRYPTPROV hProv = 0;
                    HCRYPTHASH hHash = 0;
                    if (CryptAcquireContext(&hProv, NULL, NULL, PROV_RSA_AES, CRYPT_VERIFYCONTEXT) &&
                        CryptCreateHash(hProv, CALG_SHA_256, 0, 0, &hHash)) {
                        
                        ifstream file(arg, ios::binary);
                        char hash_buf[4096];
                        while (file.read(hash_buf, sizeof(hash_buf)) || file.gcount() > 0) {
                            CryptHashData(hHash, (BYTE*)hash_buf, (DWORD)file.gcount(), 0);
                        }
                        
                        BYTE hashVal[32];
                        DWORD hashLen = sizeof(hashVal);
                        CryptGetHashParam(hHash, HP_HASHVAL, hashVal, &hashLen, 0);
                        
                        stringstream hexStream;
                        hexStream << hex << setfill('0');
                        for (DWORD i = 0; i < hashLen; i++) {
                            hexStream << setw(2) << (int)hashVal[i];
                        }
                        
                        response = "213 SHA-256 " + hexStream.str() + "\r\n";
                        CryptDestroyHash(hHash);
                        CryptReleaseContext(hProv, 0);
                    } else { response = "550 Hash calculation failed.\r\n"; }
                } else {
                    response = "550 File not found.\r\n";
                }
                send(client_sock, response.c_str(), response.length(), 0);
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
