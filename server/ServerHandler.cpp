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
#include <sstream>
#include <shared_mutex> 
#include <set>

using namespace std;

// --- CẤU TRÚC DỮ LIỆU TOÀN CỤC (SHARED MEMORY) ---

// Struct lưu trữ Topic
struct Topic {
    uint32_t id;
    string name;
    string creator; // Username người tạo
};

// Static để giới hạn phạm vi trong file này, giả lập DB toàn cục
static map<string, string> userDB;
static shared_mutex dbMutex; // Khóa bảo vệ tránh Race Condition

static map<string, Topic> topicDB; 
static shared_mutex topicMutex; 
static uint32_t globalTopicIdCounter = 0;

// Map lưu danh sách socket đang subscribe topic: TopicID -> Set<Socket>
static map<uint32_t, set<int>> topicSubscribers;
static shared_mutex subMutex; // Bảo vệ map subscribers

// Hàm tách chuỗi theo ký tự ngăn cách (delimiter)
vector<string> split(const string& s, char delimiter) {
    vector<string> tokens;
    string token;
    istringstream tokenStream(s);
    while (getline(tokenStream, token, delimiter)) {
        tokens.push_back(token);
    }
    return tokens;
}

// --- HÀM HỖ TRỢ LƯU FILE (PERSISTENCE) ---
// Ghi thêm vào cuối file (Append mode)
void saveUserToFile(const string& u, const string& p) {
    // Mở file ở chế độ Append (ios::app) -> Ghi nối đuôi
    ofstream f("data/users.txt", ios::app);
    if (f.is_open()) { 
        f << u << "|" << p << endl; 
        f.close(); 
    } else {
        cerr << "[ERROR] Cannot open data/users.txt. Make sure 'data' folder exists!" << endl;
    }
}

void saveTopicToFile(uint32_t id, const string& name, const string& creator) {
    ofstream f("data/topics.txt", ios::app);
    if (f.is_open()) { 
        f << id << "|" << name << "|" << creator << endl; 
        f.close(); 
    } else {
        cerr << "[ERROR] Cannot open data/topics.txt" << endl;
    }
}

ServerHandler::ServerHandler(int socket) : clientSocket(socket) {
    this->isLogged = false;
}

ServerHandler::~ServerHandler() { 
    close(clientSocket); 
    cout << "Closed connection on socket " << clientSocket << endl;
}

// --- HÀM LOAD DỮ LIỆU TỪ FILE ---
void ServerHandler::loadData() {
    // 1. Load Users
    ifstream fUsers("data/users.txt");
    if (fUsers.is_open()) {
        string line;
        int count = 0;
        while (getline(fUsers, line)) {
            // Tách chuỗi bằng dấu |
            vector<string> parts = split(line, '|');
            if (parts.size() >= 2) {
                string u = parts[0];
                string p = parts[1];
                userDB[u] = p;
                count++;
            }
        }
        fUsers.close();
        cout << "[SYSTEM] Loaded " << count << " users from data/users.txt" << endl;
    } else {
        cout << "[WARNING] data/users.txt not found. Starting with empty User DB." << endl;
    }

    // 2. Load Topics (data/topics.txt)
    ifstream fTopics("data/topics.txt");
    if (fTopics.is_open()) {
        string line;
        int count = 0;
        while (getline(fTopics, line)) {
            // Format: id|name|creator
            vector<string> parts = split(line, '|');
            if (parts.size() >= 3) {
                Topic t;
                t.id = stoi(parts[0]); // Chuyển chuỗi sang số
                t.name = parts[1];
                t.creator = parts[2];
                
                topicDB[t.name] = t;
                
                // Cập nhật ID lớn nhất để không bị trùng khi tạo mới
                if (t.id > globalTopicIdCounter) {
                    globalTopicIdCounter = t.id;
                }
                count++;
            }
        }
        fTopics.close();
        cout << "[SYSTEM] Loaded " << count << " topics. Max ID: " << globalTopicIdCounter << endl;
    } else {
        cout << "[WARNING] data/topics.txt not found. Starting with empty Topic DB." << endl;
    }
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

        string s(payload.begin(),payload.end());
        cout << s << endl;

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
            case REQ_SUBSCRIBE:
                handleSubscribe(payload.data(), payload.size());
                break;
            case REQ_UNSUBSCRIBE:
                handleUnsubscribe(payload.data(), payload.size());
                break;
            case REQ_PUBLISH_TEXT:
                handlePublishText(payload.data(), payload.size());
                break;

            default:
                cout << "Unknown OpCode: " << (int)opcode << endl;
                break;
        }
    }
}

// Xử lý Đăng Ký
void ServerHandler::handleRegister(const void* payloadData, uint32_t len) {
    PacketReader reader(payloadData, len);
    string username = reader.readString();
    string password = reader.readString();

    if (username.empty() || password.empty()) {
        uint8_t status = REGISTER_FAIL_INVALID;
        NetworkUtils::sendPacket(clientSocket, RES_REGISTER, &status, 1);
        return;
    }

    uint8_t status;
    {
        // Write Lock: Chỉ 1 người được đăng ký tại 1 thời điểm để tránh trùng lặp
        unique_lock<shared_mutex> lock(dbMutex);
        
        if (userDB.find(username) != userDB.end()) {
            status = REGISTER_FAIL_EXISTS;
            cout << "[REGISTER] Fail (Exist): " << username << endl;
        } else {
            userDB[username] = password;
            saveUserToFile(username, password); // Lưu ngay xuống file
            status = REGISTER_OK;
            cout << "[REGISTER] Success: " << username << endl;
        }
    }

    NetworkUtils::sendPacket(clientSocket, RES_REGISTER, &status, 1);
}

// Xử lý Đăng Nhập
void ServerHandler::handleLogin(const void* payloadData, uint32_t len) {
    PacketReader reader(payloadData, len);
    string username = reader.readString();
    string password = reader.readString();

    uint8_t status;
    {
        shared_lock<shared_mutex> lock(dbMutex);

        if (userDB.find(username) == userDB.end()) {
            status = LOGIN_FAIL_NOT_FOUND;
        } else if (userDB[username] != password) {
            status = LOGIN_FAIL_WRONG_PASS;
        } else {
            status = LOGIN_OK;
            this->currentUser = username;
            this->isLogged = true;
            cout << "[LOGIN] Success: " << username << endl;
        }
    }
    NetworkUtils::sendPacket(clientSocket, RES_LOGIN, &status, 1);
}

// Xử lý Tạo Topic
void ServerHandler::handleCreateTopic(const void* payloadData, uint32_t len) {
    if (!isLogged) return; 

    // Dùng PacketReader
    PacketReader reader(payloadData, len);
    string topicName = reader.readString();

    if (topicName.empty()) return;

    uint8_t status;
    uint32_t newTopicId = 0;
    {
        // Write Lock: Khóa để tăng ID và thêm topic an toàn
        unique_lock<shared_mutex> lock(topicMutex);
        
        if (topicDB.find(topicName) != topicDB.end()) {
            status = TOPIC_FAIL_EXISTS;
        } else {
            globalTopicIdCounter++;
            newTopicId = globalTopicIdCounter;

            Topic t; 
            t.id = newTopicId; // Lưu lại ID
            t.name = topicName; 
            t.creator = this->currentUser;
            topicDB[topicName] = t;
            
            saveTopicToFile(newTopicId, topicName, this->currentUser);
            
            status = TOPIC_CREATE_OK;
            cout << "[TOPIC] Created: " << topicName << " (ID: " << newTopicId << ") by " << currentUser << endl;
        }
    }

    // Đóng gói phản hồi: [Status 1B] + [TopicID 4B]
    PacketBuilder builder;
    builder.addBlob(&status, 1); // Thêm status (byte thô)
    if (status == TOPIC_CREATE_OK) {
        builder.addInt(newTopicId);
    }

    NetworkUtils::sendPacket(clientSocket, RES_CREATE_TOPIC, builder.getData(), builder.getSize());
}

// Xử lý Xóa Topic (Ghi DB Topic -> Dùng unique_lock)
void ServerHandler::handleDeleteTopic(const void* payloadData, uint32_t len) {
    if (!isLogged) return;

    PacketReader reader(payloadData, len);
    string topicName = reader.readString();

    uint8_t status;
    {
        //  Write Lock
        std::unique_lock<std::shared_mutex> lock(topicMutex);
        
        auto it = topicDB.find(topicName);
        if (it == topicDB.end()) {
            status = TOPIC_FAIL_NOT_FOUND;
        } else {
            // CHECK QUYỀN: Chỉ chủ topic mới được xóa
            if (it->second.creator != this->currentUser) {
                status = TOPIC_FAIL_DENIED;
            } else {
                topicDB.erase(it);
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

    shared_lock<shared_mutex> lock(topicMutex);
    
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

    shared_lock<shared_mutex> lock(topicMutex);
    
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

void ServerHandler::handleSubscribe(const void* payloadData, uint32_t len) {
    if (!isLogged) return;

    PacketReader reader(payloadData, len);
    uint32_t topicId = reader.readInt(); // Đọc ID topic client muốn sub

    {
        std::unique_lock<std::shared_mutex> lock(subMutex);
        topicSubscribers[topicId].insert(this->clientSocket);
    }
    
    // Phản hồi OK
    uint8_t status = SUB_OK;
    NetworkUtils::sendPacket(clientSocket, RES_SUBSCRIBE, &status, 1);
    cout << "[SUB] User " << currentUser << " subscribed topic " << topicId << endl;
}

// Xử lý Hủy theo dõi (Unsubscribe)
void ServerHandler::handleUnsubscribe(const void* payloadData, uint32_t len) {
    if (!isLogged) return;

    PacketReader reader(payloadData, len);
    uint32_t topicId = reader.readInt();

    {
        std::unique_lock<std::shared_mutex> lock(subMutex);
        topicSubscribers[topicId].erase(this->clientSocket);
    }

    uint8_t status = UNSUB_OK;
    NetworkUtils::sendPacket(clientSocket, RES_UNSUBSCRIBE, &status, 1);
    cout << "[UNSUB] User " << currentUser << " unsubscribed topic " << topicId << endl;
}

// Xử lý Chat Text (Publish)
void ServerHandler::handlePublishText(const void* payloadData, uint32_t len) {
    if (!isLogged) return;

    // B1: Đọc dữ liệu gửi lên
    PacketReader reader(payloadData, len);
    uint32_t topicId = reader.readInt();
    string msgContent = reader.readString();

    // B2: Phản hồi cho người gửi biết server đã nhận
    uint8_t status = STATUS_SUCCESS;
    NetworkUtils::sendPacket(clientSocket, RES_PUBLISH, &status, 1);

    // B3: Broadcast cho tất cả người đang sub topic này
    // Cấu trúc gói tin NOTIFY: [topic_id][user_len][user][msg_len][msg]
    PacketBuilder builder;
    builder.addInt(topicId);
    builder.addString(this->currentUser); // Người gửi là người hiện tại
    builder.addString(msgContent);

    {
        std::shared_lock<std::shared_mutex> lock(subMutex);
        // Lấy danh sách socket đang sub topic này
        if (topicSubscribers.count(topicId)) {
            const auto& subs = topicSubscribers[topicId];
            for (int sock : subs) {
                // Không gửi lại cho chính người nói (optional, tùy logic app)
                if (sock != this->clientSocket) {
                     NetworkUtils::sendPacket(sock, NOTIFY_MSG_TEXT, builder.getData(), builder.getSize());
                }
            }
        }
    }
    
    // (Optional) Lưu vào History log file theo yêu cầu Phase 2 tại đây
    // saveChatLog(topicId, currentUser, msgContent);
    
    cout << "[CHAT] Topic " << topicId << " | " << currentUser << ": " << msgContent << endl;
}