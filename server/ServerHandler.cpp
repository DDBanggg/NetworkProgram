#include "ServerHandler.h"
#include "../common/NetworkUtils.h"
#include "../common/DataUtils.h"
#include "../common/protocol.h"
#include <iostream>
#include <vector>
#include <map>
#include <mutex>
#include <unistd.h> // Header chuẩn cho close() trên Linux
#include <fstream> // Thư viện đọc ghi file 

using namespace std;

// --- CẤU TRÚC DỮ LIỆU TOÀN CỤC (SHARED MEMORY) ---

// Struct lưu trữ Topic
struct Topic {
    string name;
    string creator; // Username người tạo
};

// Static để giới hạn phạm vi trong file này, giả lập DB toàn cục
static map<string, string> userDB;
static mutex dbMutex; // Khóa bảo vệ tránh Race Condition

static map<string, Topic> topicDB; // TopicName - TopicStruct
static mutex topicMutex;           // Khóa Topic

// --- HÀM HỖ TRỢ LƯU FILE (PERSISTENCE) ---
// Ghi thêm vào cuối file (Append mode)
void saveUserToFile(const string& u, const string& p) {
    ofstream f("users.txt", ios::app);
    if (f.is_open()) { 
        f << u << " " << p << endl; 
        f.close(); 
    }
}

void saveTopicToFile(const string& name, const string& creator) {
    ofstream f("topics.txt", ios::app);
    if (f.is_open()) { 
        f << name << " " << creator << endl; 
        f.close(); 
    }
}

ServerHandler::ServerHandler(int socket) : clientSocket(socket) {
    this->isLogged = false;
}

ServerHandler::~ServerHandler() { 
    close(clientSocket); 
    cout << "Closed connection on socket " << clientSocket << endl;
}

void ServerHandler::run() {
    // Vòng lặp nhận tin nhắn liên tục
    while (true) {
        uint8_t opcode;
        vector<uint8_t> payload;

        // 1. Nhận gói tin (Block chờ)
        bool success = NetworkUtils::recvPacket(clientSocket, opcode, payload);
        if (!success) {
            cout << "Client disconnected or error on socket " << clientSocket << endl;
            break; 
        }

        // 2. Điều hướng xử lý
        switch (opcode) {
            case REQ_REGISTER:
                handleRegister(payload.data(), payload.size());
                break;
            case REQ_LOGIN:
                handleLogin(payload.data(), payload.size());
                break;
            case REQ_CREATE_TOPIC:
                handleCreateTopic(payload.data(), payload.size());
                break;
            case REQ_DELETE_TOPIC:
                handleDeleteTopic(payload.data(), payload.size());
                break;
            case REQ_GET_ALL_TOPICS:
                handleGetAllTopics();
                break;
            case REQ_GET_MY_TOPICS:
                handleGetMyTopics();
                break;

            default:
                cout << "Unknown OpCode: " << (int)opcode << endl;
                break;
        }
    }
}

// Xử lý Đăng Ký
void ServerHandler::handleRegister(const void* payloadData, uint32_t len) {
    const uint8_t* ptr = static_cast<const uint8_t*>(payloadData);
    vector<uint8_t> buffer(ptr, ptr + len);
    
    size_t offset = 0;
    string username = DataUtils::unpackString(buffer, offset);
    string password = DataUtils::unpackString(buffer, offset);

    if (username.empty() || password.empty()) {
        uint8_t status = REGISTER_FAIL_INVALID;
        NetworkUtils::sendPacket(clientSocket, RES_REGISTER, &status, 1);
        return;
    }

    uint8_t status;

    // Lock DB
    {
        lock_guard<mutex> lock(dbMutex);
        
        if (userDB.find(username) != userDB.end()) {
            status = REGISTER_FAIL_EXISTS;
            cout << "[REGISTER] Fail (Exist): " << username << endl;
        } else {
            userDB[username] = password;
            saveUserToFile(username, password);
            status = REGISTER_OK;
            cout << "[REGISTER] Success: " << username << endl;
        }
    }

    // Gửi phản hồi
    NetworkUtils::sendPacket(clientSocket, RES_REGISTER, &status, 1);
}

// Xử lý Đăng Nhập
void ServerHandler::handleLogin(const void* payloadData, uint32_t len) {
    const uint8_t* ptr = static_cast<const uint8_t*>(payloadData);
    vector<uint8_t> buffer(ptr, ptr + len);
    
    size_t offset = 0;
    string username = DataUtils::unpackString(buffer, offset);
    string password = DataUtils::unpackString(buffer, offset);

    uint8_t status;

    // Lock DB
    {
        lock_guard<mutex> lock(dbMutex);

        if (userDB.find(username) == userDB.end()) {
            status = LOGIN_FAIL_NOT_FOUND;
            cout << "[LOGIN] Fail (Not Found): " << username << endl;
        } 
        else if (userDB[username] != password) {
            status = LOGIN_FAIL_WRONG_PASS;
            cout << "[LOGIN] Fail (Wrong Pass): " << username << endl;
        } 
        else {
            status = LOGIN_OK;
            this->currentUser = username;
            this->isLogged = true;
            cout << "[LOGIN] Success: " << username << endl;
        }
    }

    // Gửi phản hồi
    NetworkUtils::sendPacket(clientSocket, RES_LOGIN, &status, 1);
}

// Xử lý Tạo Topic
void ServerHandler::handleCreateTopic(const void* payloadData, uint32_t len) {
    if (!isLogged) return; // Chưa login thì không cho tạo

    const uint8_t* ptr = static_cast<const uint8_t*>(payloadData);
    vector<uint8_t> buffer(ptr, ptr + len);
    size_t offset = 0;
    string topicName = DataUtils::unpackString(buffer, offset);

    uint8_t status;
    {
        lock_guard<mutex> lock(topicMutex);
        if (topicDB.find(topicName) != topicDB.end()) {
            status = TOPIC_FAIL_EXISTS;
        } else {
            // Tạo mới trong RAM
            Topic t; 
            t.name = topicName; 
            t.creator = this->currentUser;
            topicDB[topicName] = t;
            
            // Lưu xuống file
            saveTopicToFile(topicName, this->currentUser);
            
            status = TOPIC_CREATE_OK;
            cout << "[TOPIC] New topic created: " << topicName << " by " << currentUser << endl;
        }
    }
    NetworkUtils::sendPacket(clientSocket, RES_CREATE_TOPIC, &status, 1);
}

// Xử lý Xóa Topic
void ServerHandler::handleDeleteTopic(const void* payloadData, uint32_t len) {
    if (!isLogged) return;

    const uint8_t* ptr = static_cast<const uint8_t*>(payloadData);
    vector<uint8_t> buffer(ptr, ptr + len);
    size_t offset = 0;
    string topicName = DataUtils::unpackString(buffer, offset);

    uint8_t status;
    {
        lock_guard<mutex> lock(topicMutex);
        auto it = topicDB.find(topicName);
        if (it == topicDB.end()) {
            status = TOPIC_FAIL_NOT_FOUND;
        } else {
            // CHECK QUYỀN: Chỉ chủ topic mới được xóa
            if (it->second.creator != this->currentUser) {
                status = TOPIC_FAIL_DENIED;
            } else {
                topicDB.erase(it); // Xóa khỏi RAM
                status = TOPIC_DELETE_OK;
                cout << "[TOPIC] Deleted: " << topicName << " by " << currentUser << endl;
            }
        }
    }
    NetworkUtils::sendPacket(clientSocket, RES_DELETE_TOPIC, &status, 1);
}

// Lấy toàn bộ Topic
void ServerHandler::handleGetAllTopics() {
    if (!isLogged) return;
    
    vector<uint8_t> payload;
    lock_guard<mutex> lock(topicMutex);
    
    // Đếm số lượng topic
    uint32_t count = topicDB.size(); 
    
    // Đóng gói Count (4 byte)
    vector<uint8_t> countBytes = DataUtils::packInt(count);
    payload.insert(payload.end(), countBytes.begin(), countBytes.end());

    // Loop và đóng gói từng item
    for (auto const& [key, val] : topicDB) {
        vector<uint8_t> nameB = DataUtils::packString(val.name);
        vector<uint8_t> creatorB = DataUtils::packString(val.creator);
        
        payload.insert(payload.end(), nameB.begin(), nameB.end());
        payload.insert(payload.end(), creatorB.begin(), creatorB.end());
    }

    NetworkUtils::sendPacket(clientSocket, RES_GET_ALL_TOPICS, payload.data(), payload.size());
}

// Lấy Topic của tôi
void ServerHandler::handleGetMyTopics() {
    if (!isLogged) return;
    
    vector<uint8_t> payload;
    lock_guard<mutex> lock(topicMutex);
    
    // 1. Lọc trước để đếm số lượng
    uint32_t count = 0;
    for (auto const& [key, val] : topicDB) {
        if (val.creator == this->currentUser) {
            count++;
        }
    }
    
    // Đóng gói Count (4 byte)
    vector<uint8_t> countBytes = DataUtils::packInt(count);
    payload.insert(payload.end(), countBytes.begin(), countBytes.end());

    // 2. Loop lại và đóng gói item
    for (auto const& [key, val] : topicDB) {
        if (val.creator == this->currentUser) {
            vector<uint8_t> nameB = DataUtils::packString(val.name);
            vector<uint8_t> creatorB = DataUtils::packString(val.creator);
            
            payload.insert(payload.end(), nameB.begin(), nameB.end());
            payload.insert(payload.end(), creatorB.begin(), creatorB.end());
        }
    }

    NetworkUtils::sendPacket(clientSocket, RES_GET_MY_TOPICS, payload.data(), payload.size());
}