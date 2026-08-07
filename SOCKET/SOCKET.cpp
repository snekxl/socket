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
            // --- ĐỊNH TUYẾN 28 LỆNH FTP TRONG VÒNG LẶP RECV CỦA SERVER ---

string cmd = request.substr(0, request.find(' ')); // Tách lấy từ khóa lệnh (vd: PWD, STOR)
string arg = "";
if (request.find(' ') != string::npos) {
    arg = request.substr(request.find(' ') + 1);
    arg.erase(arg.find_last_not_of(" \n\r\t") + 1); // Xóa khoảng trắng thừa
}

// 1. NHÓM QUẢN LÝ PHIÊN (Đã xong)
if (cmd == "QUIT") {
    response = "221 Goodbye. Hen gap lai!\r\n";
    send(client_sock, response.c_str(), response.length(), 0);
    break; // Thoát vòng lặp
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

// 2. NHÓM ĐIỀU HƯỚNG THƯ MỤC
else if (cmd == "PWD") {
    // Dùng thư viện filesystem của C++17 để lấy thư mục hiện tại
    string current_path = std::filesystem::current_path().string();
    response = "257 \"" + current_path + "\" is the current directory.\r\n";
    send(client_sock, response.c_str(), response.length(), 0);
}
else if (cmd == "CWD") {
    // Logic: Kiểm tra xem thư mục arg có tồn tại không, nếu có thì chdir() tới đó
    response = "250 Requested file action okay, completed.\r\n"; // Tạm thời trả về OK
    send(client_sock, response.c_str(), response.length(), 0);
}
else if (cmd == "CDUP") { /* Chuyển lên thư mục cha */ }
else if (cmd == "MKD")  { /* Tạo thư mục mới dựa trên arg */ }
else if (cmd == "RMD")  { /* Xóa thư mục rỗng */ }

// 3. NHÓM THAO TÁC FILE CƠ BẢN
else if (cmd == "DELE") { /* Xóa file arg */ }
else if (cmd == "RNFR") { /* Lưu lại tên file cũ cần đổi tên */ }
else if (cmd == "RNTO") { /* Đổi tên file cũ thành tên arg mới */ }

// 4. NHÓM TRUYỀN TẢI FILE (KÊNH UDP)
else if (cmd == "RETR") {
    // Logic hiện tại của bạn đã làm rất tốt phần này
    response = "150 File okay. Dang mo kenh UDP...\r\n";
    send(client_sock, response.c_str(), response.length(), 0);
    // ... (Gọi hàm sendFileUDP như code cũ) ...
}
else if (cmd == "STOR") {
    response = "150 Khach hang san sang gui file, mo kenh UDP...\r\n";
    send(client_sock, response.c_str(), response.length(), 0);
    // Logic: Gọi hàm receiveFileUDP (Cần code thêm hàm này ở Server)
}
else if (cmd == "STOU") { /* Upload và Server tự sinh tên file độc nhất */ }
else if (cmd == "APPE") { /* Ghi nối tiếp dữ liệu vào file đã có */ }
else if (cmd == "LIST" || cmd == "NLST") {
    response = "150 Mo kenh UDP de gui danh sach thu muc...\r\n";
    send(client_sock, response.c_str(), response.length(), 0);
    // Logic: Đọc danh sách file trong thư mục, đóng gói thành chuỗi văn bản, gửi qua UDP
}

// 5. NHÓM THÔNG TIN & CẤU HÌNH TRẠNG THÁI
else if (cmd == "SIZE") { /* Lấy kích thước file arg tính bằng byte */ }
else if (cmd == "MDTM") { /* Lấy thời gian chỉnh sửa cuối của file arg */ }
else if (cmd == "TYPE") { /* Set mode I (Nhị phân) hoặc A (ASCII) */ }
else if (cmd == "MODE") { /* Set mode S, B, C */ }
else if (cmd == "STAT") { /* Trả về trạng thái Server */ }
else if (cmd == "HELP") { /* Trả về text hướng dẫn sử dụng */ }
else if (cmd == "PORT") { /* Xử lý Active Mode: Phân tích tham số h1,h2,h3,h4,p1,p2 */ }
else if (cmd == "PASV") { /* Xử lý Passive Mode: Trả về IP và Port ngẫu nhiên cho Client */ }
else if (cmd == "HASH") { /* Code hàm MD5/SHA-256 để băm file */ }
else if (cmd == "ABOR") { /* Cắt đứt tiến trình UDP đang chạy */ }

// NẾU GÕ SAI LỆNH HOẶC CHƯA HỖ TRỢ
else {
    response = "502 Command not implemented.\r\n";
    send(client_sock, response.c_str(), response.length(), 0);
}
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
