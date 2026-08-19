#define _CRT_SECURE_NO_WARNINGS
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
#include <wincrypt.h> 
#include <thread>
#include <mutex>
#include <map>

#pragma comment(lib, "ws2_32.lib")

using namespace std;
namespace fs = std::filesystem;

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

// =====================================================================
// HÀM GỬI FILE QUA UDP (DÀNH CHO LỆNH RETR / LIST)
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
    RDTPacket packet = { 0 };

    while (true) {
        file.read(packet.payload, PAYLOAD_SIZE);
        int bytesRead = file.gcount();
        if (bytesRead <= 0) break; // Thoát nếu hết file

        packet.header.payload_len = bytesRead;
        packet.header.seq_num = current_seq;
        packet.header.flags = FLAG_DATA;

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
                current_seq = 1 - current_seq;
            }
            else {
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
    sendto(udpSocket, (char*)&packet, sizeof(packet), 0, (sockaddr*)&clientAddr, sizeof(clientAddr));

    Sleep(100); // CHỐNG RỚT GÓI FIN

    file.close();
    cout << "[+] Da gui file hoan tat qua RDT." << endl;
    return true;
}

// =====================================================================
// HÀM NHẬN FILE QUA UDP (DÀNH CHO LỆNH STOR, APPE, STOU)
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
                }
                else {
                    ack_pkt.header.ack_num = packet.header.seq_num;
                    sendto(udpSocket, (char*)&ack_pkt, sizeof(ack_pkt), 0, (sockaddr*)&clientAddr, sizeof(clientAddr));
                }
            }
        }
    }

    file.close();
    cout << "[+] Da nhan va luu file thanh cong." << endl;
    return true;
}

// =====================================================================
// HỆ THỐNG QUẢN LÝ ĐA LUỒNG VÀ CONNECTED-CLIENT TABLE
// =====================================================================
mutex mtx;
map<SOCKET, string> connected_clients; // Lưu Socket ID và IP của các Client

void printClientTable() {
    cout << "\n=== BANG DANH SACH CLIENT DANG KET NOI (CONNECTED CLIENT TABLE) ===\n";
    if (connected_clients.empty()) {
        cout << "[Trong]\n";
    } else {
        for (auto const& [sock, ip] : connected_clients) {
            cout << " -> Client IP: " << ip << " | Control Socket ID: " << sock << "\n";
        }
    }
    cout << "===================================================================\n\n";
}

// Hàm chạy riêng cho từng Client (Đa luồng)
void handleClientSession(SOCKET client_sock, string client_ip) {
    string welcome_msg = "220 Welcome to Hybrid FTP Server\r\n";
    send(client_sock, welcome_msg.c_str(), welcome_msg.length(), 0);

    string rename_from_path = ""; 
    char transfer_type = 'I'; 
    char transfer_mode = 'S'; 

    char buffer[1024];
    while (true) { 
        memset(buffer, 0, sizeof(buffer));
        int bytes_received = recv(client_sock, buffer, sizeof(buffer) - 1, 0);

        if (bytes_received <= 0) break; // Client ngắt kết nối đột ngột

        string request(buffer);
        cout << "[CLIENT " << client_ip << ":" << client_sock << "] " << request;
        string response = "";

        string cmd = request.substr(0, request.find(' ')); 
        cmd.erase(cmd.find_last_not_of(" \n\r\t") + 1); 
        
        string arg = "";
        if (request.find(' ') != string::npos) {
            arg = request.substr(request.find(' ') + 1);
            arg.erase(arg.find_last_not_of(" \n\r\t") + 1); 
        }

        // --- TỔNG ĐÀI ĐỊNH TUYẾN 28 LỆNH ---
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
                } else { response = "550 Failed to change directory. Path not found.\r\n"; }
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
        else if (cmd == "RETR") {
            response = "150 File okay. Dang mo kenh UDP tren cong 2122...\r\n";
            send(client_sock, response.c_str(), response.length(), 0);
            
            SOCKET udp_sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
            sockaddr_in target_client_udp;
            target_client_udp.sin_family = AF_INET;
            target_client_udp.sin_port = htons(2122);
            inet_pton(AF_INET, client_ip.c_str(), &target_client_udp.sin_addr);
            
            if (sendFileUDP(udp_sock, target_client_udp, arg)) {
                response = "226 Transfer complete.\r\n";
            } else { response = "550 File unavailable hoac loi doc file.\r\n"; }
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
            } else { response = "550 Transfer failed.\r\n"; }
            send(client_sock, response.c_str(), response.length(), 0);
            closesocket(udp_sock);
        }
        else if (cmd == "LIST" || cmd == "NLST") {
            response = "150 Opening data connection for directory list...\r\n";
            send(client_sock, response.c_str(), response.length(), 0);

            string temp_filename = "temp_dir_list.txt";
            ofstream temp_file(temp_filename);
            string target_dir = arg.empty() ? fs::current_path().string() : arg;

            try {
                if (fs::exists(target_dir) && fs::is_directory(target_dir)) {
                    for (const auto& entry : fs::directory_iterator(target_dir)) {
                        string filename = entry.path().filename().string();
                        if (cmd == "NLST") { temp_file << filename << "\r\n"; } 
                        else {
                            string type = entry.is_directory() ? "<DIR>  " : "<FILE> ";
                            string size = entry.is_directory() ? "0" : to_string(entry.file_size());
                            temp_file << type << "\t" << size << " bytes\t" << filename << "\r\n";
                        }
                    }
                } else { temp_file << "Directory not found.\r\n"; }
            } catch (...) { temp_file << "Access denied.\r\n"; }
            temp_file.close();

            SOCKET udp_sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
            sockaddr_in target_client_udp;
            target_client_udp.sin_family = AF_INET;
            target_client_udp.sin_port = htons(2122);
            inet_pton(AF_INET, client_ip.c_str(), &target_client_udp.sin_addr);

            if (sendFileUDP(udp_sock, target_client_udp, temp_filename)) {
                response = "226 Directory send OK.\r\n";
            } else { response = "550 Failed to send directory list.\r\n"; }
            send(client_sock, response.c_str(), response.length(), 0);
            closesocket(udp_sock);
            fs::remove(temp_filename);
        }
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
            } else { response = "550 File not found.\r\n"; }
            send(client_sock, response.c_str(), response.length(), 0);
        }
        else if (cmd == "RNTO") {
            try {
                if (!rename_from_path.empty()) {
                    fs::rename(rename_from_path, arg);
                    response = "250 File renamed successfully.\r\n";
                    rename_from_path = ""; 
                } else { response = "503 Bad sequence of commands (Use RNFR first).\r\n"; }
            } catch (...) { response = "550 Rename failed.\r\n"; }
            send(client_sock, response.c_str(), response.length(), 0);
        }
        else if (cmd == "ABOR") {
            response = "226 Abort successful.\r\n";
            send(client_sock, response.c_str(), response.length(), 0);
        }
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
                    auto ftime = fs::last_write_time(arg);
                    auto sctp = chrono::time_point_cast<chrono::system_clock::duration>(ftime - fs::file_time_type::clock::now() + chrono::system_clock::now());
                    time_t cftime = chrono::system_clock::to_time_t(sctp);
                    tm* t = localtime(&cftime);
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
        else if (cmd == "TYPE" || cmd == "MODE") {
            response = "200 Command parameter set successfully.\r\n";
            send(client_sock, response.c_str(), response.length(), 0);
        }
        else if (cmd == "HASH") {
            if (fs::exists(arg) && !fs::is_directory(arg)) {
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
            } else { response = "550 File not found.\r\n"; }
            send(client_sock, response.c_str(), response.length(), 0);
        }
        else {
            response = "502 Command not implemented.\r\n";
            send(client_sock, response.c_str(), response.length(), 0);
        }
    } 

    // Khi vòng lặp bị phá vỡ (Client thoát), xóa khỏi danh sách và in lại bảng
    mtx.lock();
    connected_clients.erase(client_sock);
    cout << "\n[He thong] Client (" << client_ip << ") da ngat ket noi." << endl;
    printClientTable();
    mtx.unlock();

    closesocket(client_sock); 
}

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
    listen(server_sock, 5); // Tăng hàng đợi kết nối lên 5
    cout << "[TCP] Server dang lang nghe tren cong 2121..." << endl;

    while (true) {
        sockaddr_in client_info;
        int client_info_len = sizeof(client_info);
        SOCKET client_sock = accept(server_sock, (sockaddr*)&client_info, &client_info_len);
        
        if (client_sock == INVALID_SOCKET) continue;

        char client_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &(client_info.sin_addr), client_ip, INET_ADDRSTRLEN);

        // Khóa Mutex để cập nhật bảng danh sách an toàn giữa các luồng
        mtx.lock();
        connected_clients[client_sock] = string(client_ip);
        cout << "\n[He thong] Phat hien ket noi moi tu (" << client_ip << ")!" << endl;
        printClientTable(); 
        mtx.unlock();

        // KÍCH HOẠT ĐA LUỒNG: Tạo một luồng (thread) mới phục vụ riêng cho Client này
        thread(handleClientSession, client_sock, string(client_ip)).detach();
    }

    closesocket(server_sock);
    WSACleanup();
    return 0;
}
