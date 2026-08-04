#include <iostream>
#include <fstream>
#include <vector>
#include <winsock2.h>

using namespace std;

// Hàm truyền file an toàn qua UDP (Reliable Data Transfer)
bool sendFileUDP(SOCKET udp_sock, sockaddr_in client_addr, const string& filename) {
    // Mở file dưới dạng nhị phân (binary) để truyền được cả ảnh, video
    ifstream file(filename, ios::binary);
    if (!file.is_open()) {
        cerr << "[UDP Lỗi] Khong the mo file: " << filename << endl;
        return false;
    }

    int seq_num = 0; // Khởi tạo Sequence Number là 0
    int client_len = sizeof(client_addr);
    char buffer[1022]; // Dành 2 byte đầu cho Header, nên Payload tối đa là 1022 bytes

    while (!file.eof()) {
        file.read(buffer, sizeof(buffer));
        int bytes_read = file.gcount(); // Số byte thực tế vừa đọc được

        if (bytes_read == 0) break;

        // Đánh dấu cờ EOF (End of File) nếu đã đọc đến cuối file
        char is_eof = file.eof() ? 1 : 0;

        // Đóng gói Header tự chế: [SeqNum (1 byte)] + [EOF (1 byte)] + [Data]
        vector<char> packet;
        packet.push_back((char)seq_num);
        packet.push_back(is_eof);
        packet.insert(packet.end(), buffer, buffer + bytes_read);

        bool ack_received = false;

        // VÒNG LẶP STOP-AND-WAIT
        while (!ack_received) {
            // 1. Gửi gói tin đi
            sendto(udp_sock, packet.data(), packet.size(), 0, (sockaddr*)&client_addr, client_len);
            cout << "[UDP] Da gui goi tin Seq=" << seq_num << ". Dang cho ACK..." << endl;

            // 2. Cài đặt đồng hồ bấm giờ (Timeout = 2 giây) bằng hàm select()
            fd_set read_fds;
            FD_ZERO(&read_fds);
            FD_SET(udp_sock, &read_fds);

            timeval timeout;
            timeout.tv_sec = 2; // Đợi tối đa 2 giây
            timeout.tv_usec = 0;

            // Kiểm tra xem socket có tín hiệu nhận về không
            int activity = select(0, &read_fds, NULL, NULL, &timeout);

            if (activity == 0) {
                // Hết 2 giây mà không có phản hồi -> Bị lặp lại vòng while (Gửi lại)
                cout << "[UDP] TIMEOUT! Khong thay ACK. Dang gui lai goi Seq=" << seq_num << "..." << endl;
            } 
            else if (activity > 0) {
                // Có tín hiệu gửi về, tiến hành đọc ACK
                char ack_buffer[10];
                sockaddr_in sender_addr;
                int sender_len = sizeof(sender_addr);
                
                recvfrom(udp_sock, ack_buffer, sizeof(ack_buffer), 0, (sockaddr*)&sender_addr, &sender_len);
                
                int ack_num = ack_buffer[0];
                
                // Nếu ACK đúng với gói vừa gửi
                if (ack_num == seq_num) {
                    cout << "[UDP] Nhận thành công ACK=" << ack_num << ". Chuyển gói tiếp theo." << endl;
                    seq_num = 1 - seq_num; // Đảo bit (0 thành 1, 1 thành 0)
                    ack_received = true;   // Phá vòng lặp Stop-and-Wait để đọc chunk file tiếp theo
                } else {
                    cout << "[UDP] Nhận ACK sai (trùng lặp). Bỏ qua và đợi ACK đúng." << endl;
                }
            }
        }
    }

    file.close();
    cout << "[UDP] HOÀN TẤT TRUYỀN FILE!" << endl;
    return true;
}
