#include <iostream>
#include <thread>
#include <vector>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <signal.h>
#include "ServerHandler.h"

using namespace std;

#define PORT 8080

// Hàm trung gian để chạy Handler trong luồng mới
void connectionHandler(int clientSock) {
    // Tạo đối tượng Handler trên stack của luồng này
    ServerHandler handler(clientSock);
    handler.run(); 
    // Khi run() kết thúc (client disconnect), handler tự hủy -> đóng socket
}

int main() {
    signal(SIGPIPE, SIG_IGN);

    int serverSocket, newSocket;
    struct sockaddr_in address;
    int addrlen = sizeof(address);

    // 1. Tạo Socket
    if ((serverSocket = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("Socket failed");
        return -1;
    }

    // Tùy chọn để tái sử dụng cổng ngay sau khi tắt server (tránh lỗi Address already in use)
    int opt = 1;
    setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    // 2. Gán địa chỉ IP/Port
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY; // Lắng nghe mọi IP
    address.sin_port = htons(PORT);

    // 3. Bind
    if (bind(serverSocket, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("Bind failed");
        return -1;
    }

    // 4. Listen
    if (listen(serverSocket, 10) < 0) {
        perror("Listen failed");
        return -1;
    }

    cout << "Truong Anh dang lang nghe tai port " << PORT << "...\n";

    // 5. Vòng lặp chấp nhận kết nối
    while (true) {
        if ((newSocket = accept(serverSocket, (struct sockaddr *)&address, (socklen_t*)&addrlen)) < 0) {
            perror("Accept failed");
            continue;
        }

        cout << "Ket noi moi! Socket ID: " << newSocket << endl;

        // 6. Tạo luồng mới cho Client (Detach để chạy ngầm)
        // Truyền hàm connectionHandler và tham số newSocket vào thread
        thread t(connectionHandler, newSocket);
        t.detach();
    }

    return 0;
}