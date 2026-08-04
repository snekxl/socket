#ifndef NETWORK_TCP_H
#define NETWORK_TCP_H

#include <string>
#include <sys/socket.h>
#include <netinet/in.h>

class TCPSocket {
private:
    int sock_fd;
    struct sockaddr_in address;

public:
    TCPSocket();                 // Khởi tạo socket mới (dùng cho Server listening hoặc Client)
    TCPSocket(int existing_fd);  // Khởi tạo từ một file descriptor có sẵn (dùng cho accept)
    ~TCPSocket();

    // CÁC HÀM BỔ SUNG CHO SERVER
    bool bindPort(int port);
    bool listenForClients(int backlog = 5);
    TCPSocket acceptClient();

    bool connectTo(const std::string& ip, int port);
    bool sendData(const std::string& data);
    std::string recvData(int buffer_size = 1024);
    void closeSocket();
    bool isValid() const;
};

#endif
