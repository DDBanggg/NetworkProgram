#include <iostream>
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
}