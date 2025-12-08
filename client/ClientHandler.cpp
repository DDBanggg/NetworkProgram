// client/ClientHandler.cpp
#include "ClientHandler.h"
#include "../common/NetworkUtils.h"
#include "../common/DataUtils.h"
#include "../common/protocol.h"
#include <iostream>
#include <cstring>
using namespace std;

ClientHandler::ClientHandler(int socket) : serverSocket(socket) {}

ClientHandler::~ClientHandler() {
    // Không đóng socket ở đây vì socket do ClientApp quản lý vòng đời
}

bool ClientHandler::requestRegister(const string& username, const string& password) {
    // 1. Đóng gói Payload: [user_len][user][pass_len][pass]
    // DataUtils::packString đã tự động thêm 4 bytes độ dài (Big Endian) ở đầu chuỗi
    vector<uint8_t> payload = DataUtils::packString(username);
    vector<uint8_t> passBytes = DataUtils::packString(password);
    
    // Nối password vào sau username
    payload.insert(payload.end(), passBytes.begin(), passBytes.end());

    // 2. Gửi Request (REQ_REGISTER)
    if (!NetworkUtils::sendPacket(serverSocket, REQ_REGISTER, payload.data(), payload.size())) {
        cerr << "Error: Failed to send Register request." << endl;
        return false;
    }

    // 3. Chờ phản hồi NGAY LẬP TỨC (Block chờ) [cite: 152]
    uint8_t resOpcode;
    vector<uint8_t> resPayload;
    if (!NetworkUtils::recvPacket(serverSocket, resOpcode, resPayload)) {
        cerr << "Error: Connection lost while waiting for response." << endl;
        return false;
    }

    // 4. Xử lý kết quả
    if (resOpcode == RES_REGISTER) {
        if (resPayload.empty()) return false;
        uint8_t status = resPayload[0];

        if (status == REGISTER_OK) {
            cout << ">>> DANG KY THANH CONG! <<<" << endl;
            return true;
        } else if (status == REGISTER_FAIL_EXISTS) {
            cout << ">>> THAT BAI: Tai khoan da ton tai." << endl;
            return false;
        } else {
            cout << ">>> THAT BAI: Loi khong xac dinh." << endl;
            return false;
        }
    }

    return false;
}

bool ClientHandler::requestLogin(const string& username, const string& password) {
    // 1. Đóng gói Payload (Giống hệt Register) [cite: 160]
    vector<uint8_t> payload = DataUtils::packString(username);
    vector<uint8_t> passBytes = DataUtils::packString(password);
    payload.insert(payload.end(), passBytes.begin(), passBytes.end());

    // 2. Gửi Request (REQ_LOGIN)
    if (!NetworkUtils::sendPacket(serverSocket, REQ_LOGIN, payload.data(), payload.size())) {
        cerr << "Error: Failed to send Login request." << endl;
        return false;
    }

    // 3. Chờ phản hồi
    uint8_t resOpcode;
    vector<uint8_t> resPayload;
    if (!NetworkUtils::recvPacket(serverSocket, resOpcode, resPayload)) {
        cerr << "Error: Connection lost." << endl;
        return false;
    }

    // 4. Xử lý kết quả
    if (resOpcode == RES_LOGIN) {
        if (resPayload.empty()) return false;
        uint8_t status = resPayload[0];

        if (status == LOGIN_OK) {
            cout << ">>> DANG NHAP THANH CONG! <<<" << endl;
            return true;
        } else if (status == LOGIN_FAIL_NOT_FOUND) {
            cout << ">>> THAT BAI: Tai khoan khong ton tai." << endl;
            return false;
        } else if (status == LOGIN_FAIL_WRONG_PASS) {
            cout << ">>> THAT BAI: Sai mat khau." << endl;
            return false;
        }
    }
    }

    // 1. Hàm Tạo Topic
bool ClientHandler::requestCreateTopic(string topicName) {
    // Đóng gói tên topic
    vector<uint8_t> payload = DataUtils::packString(topicName);
    
    // Gửi REQ_CREATE_TOPIC
    if (!NetworkUtils::sendPacket(serverSocket, REQ_CREATE_TOPIC, payload.data(), payload.size())) {
        return false;
    }

    // Chờ phản hồi
    uint8_t op; 
    vector<uint8_t> res;
    if (!NetworkUtils::recvPacket(serverSocket, op, res)) return false;

    // Xử lý kết quả
    if (op == RES_CREATE_TOPIC && !res.empty()) {
        uint8_t status = res[0];
        if (status == TOPIC_CREATE_OK) return true;
        if (status == TOPIC_FAIL_EXISTS) {
            cout << ">> Loi: Ten Topic da ton tai!" << endl;
        } else {
            cout << ">> Loi tao topic (Status code: " << (int)status << ")" << endl;
        }
    }
    return false;
}

// 2. Hàm Xóa Topic
bool ClientHandler::requestDeleteTopic(string topicName) {
    // Logic tương tự tạo topic (Đóng gói tên -> Gửi -> Chờ KQ)
    vector<uint8_t> payload = DataUtils::packString(topicName);

    if (!NetworkUtils::sendPacket(serverSocket, REQ_DELETE_TOPIC, payload.data(), payload.size())) {
        return false;
    }

    uint8_t op; 
    vector<uint8_t> res;
    if (!NetworkUtils::recvPacket(serverSocket, op, res)) return false;

    if (op == RES_DELETE_TOPIC && !res.empty()) {
        uint8_t status = res[0];
        if (status == TOPIC_DELETE_OK) return true;
        
        // Xử lý các mã lỗi đặc thù
        if (status == TOPIC_FAIL_DENIED) {
            cout << ">> BAN KHONG PHAI CHU TOPIC NAY NEN KHONG THE XOA!" << endl;
        } else if (status == TOPIC_FAIL_NOT_FOUND) {
            cout << ">> Loi: Khong tim thay Topic nay." << endl;
        } else {
            cout << ">> Loi xoa topic khac." << endl;
        }
    }
    return false;
}

// 3. Hàm Lấy Danh Sách (Chung cho cả All và My)
void ClientHandler::requestGetList(bool isMyTopic) {
    uint8_t reqOp = isMyTopic ? REQ_GET_MY_TOPICS : REQ_GET_ALL_TOPICS;
    uint8_t resOp = isMyTopic ? RES_GET_MY_TOPICS : RES_GET_ALL_TOPICS;

    // 1. Gửi Request rỗng (Payload size = 0)
    if (!NetworkUtils::sendPacket(serverSocket, reqOp, nullptr, 0)) return;

    // 2. Nhận Response
    uint8_t op; 
    vector<uint8_t> res;
    if (!NetworkUtils::recvPacket(serverSocket, op, res)) return;

    if (op == resOp) {
        size_t offset = 0;
        
        // 3. Parse số lượng (4 byte đầu)
        if (res.size() < 4) {
            cout << ">> Danh sach rong hoac loi packet." << endl;
            return;
        }

        uint32_t netCount;
        memcpy(&netCount, &res[offset], 4);
        uint32_t count = ntohl(netCount); // Chuyển Big Endian -> Little Endian
        offset += 4;

        cout << "\n----------------------------------------" << endl;
        if (isMyTopic) cout << " DANH SACH TOPIC CUA TOI (Tong: " << count << ")" << endl;
        else           cout << " DANH SACH TAT CA TOPIC (Tong: " << count << ")" << endl;
        cout << "----------------------------------------" << endl;
        
        // 4. Loop để đọc từng Topic
        // Cấu trúc response: [Count] + List { [NameLen][Name][CreatorLen][Creator] }
        for (uint32_t i = 0; i < count; ++i) {
            string name = DataUtils::unpackString(res, offset);
            string creator = DataUtils::unpackString(res, offset);
            
            cout << "#" << (i + 1) << ". [" << name << "] - Chu thot: " << creator << endl;
        }
        cout << "----------------------------------------" << endl;
    }
}
    