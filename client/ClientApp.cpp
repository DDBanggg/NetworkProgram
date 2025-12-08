/*#include <iostream>
#include <string>
#include <thread>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include "../common/NetworkUtils.h"
#include "../common/DataUtils.h"
#include "../common/protocol.h"

using namespace std;

// Hàm hỗ trợ gửi yêu cầu đăng ký
void doRegister(int sock) {
    string user, pass;
    cout << "Nhap Username: "; cin >> user;
    cout << "Nhap Password: "; cin >> pass;

    // TODO: Đóng gói dữ liệu
    // std::vector<uint8_t> payload = DataUtils::packString(user);
    // std::vector<uint8_t> passBytes = DataUtils::packString(pass);
    // payload.insert(payload.end(), passBytes.begin(), passBytes.end());

    // TODO: Gửi đi dùng NetworkUtils::sendPacket(sock, REQ_REGISTER, ...)
    // TODO: Nhận phản hồi ngay lập tức
}

void doLogin(int sock) {
    // TODO: Làm tương tự doRegister
}

int main() {
    // TODO: Kết nối socket tới 127.0.0.1 port 8080
    // TODO: Hiện menu switch case
    return 0;
}*/



// client/ClientApp.cpp
#include <iostream>
#include <string>
#include <thread>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include "ClientHandler.h" // Include handler vừa tạo

using namespace std;

void showMenu() {
    cout << "\n=== BANG TIN DIEN TU (SPRINT 1) ===" << endl;
    cout << "1. Dang Ky (Register)" << endl;
    cout << "2. Dang Nhap (Login)" << endl;
    cout << "3. Thoat (Exit)" << endl;
    cout << "Nhap lua chon: ";
}

int main() {
    // 1. Kết nối tới Server
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        cerr << "Socket creation error" << endl;
        return -1;
    }

    struct sockaddr_in serv_addr;
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(8080); // Port mặc định theo tài liệu

    // Chuyển đổi IP (Giả sử chạy localhost)
    if (inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr) <= 0) {
        cerr << "Invalid address/ Address not supported" << endl;
        return -1;
    }

    cout << "Connecting to server 127.0.0.1:8080..." << endl;
    if (connect(sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
        cerr << "Connection Failed. Is server running?" << endl;
        return -1;
    }
    cout << "Connected successfully!" << endl;

    // 2. Khởi tạo Handler
    ClientHandler handler(sock);

    // 3. Vòng lặp Menu (Main Thread)
    bool isRunning = true;
    while (isRunning) {
        showMenu();
        int choice;
        cin >> choice;

        // Xóa bộ đệm bàn phím để tránh lỗi trôi lệnh khi nhập chuỗi sau này
        cin.ignore(); 

        string u, p;
        switch (choice) {
            case 1: // Đăng ký
                cout << "--- DANG KY ---" << endl;
                cout << "Username: "; getline(cin, u);
                cout << "Password: "; getline(cin, p);
                handler.requestRegister(u, p);
                break;

            case 2: // Đăng nhập
                cout << "--- DANG NHAP ---" << endl;
                cout << "Username: "; getline(cin, u);
                cout << "Password: "; getline(cin, p);
                handler.requestLogin(u, p);
                break;

            case 3: // Thoát
                isRunning = false;
                break;

            default:
                cout << "Lua chon khong hop le!" << endl;
        }
    }

    close(sock);
    return 0;
}