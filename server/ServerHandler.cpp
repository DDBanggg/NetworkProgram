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
#include <sys/stat.h>   // Dùng cho mkdir (Linux/macOS)
#include <sys/types.h>
#include <chrono>       // Dùng cho timestamp
#include <iomanip> // Put_time

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

// --- MUTEX RIÊNG CHO LOG VÀ HISTORY ---
static std::mutex logMutex;      // Bảo vệ server_log.txt
static std::mutex historyMutex;  // Bảo vệ topic_history.txt

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

// --- HÀM TIỆN ÍCH: GHI LOG HỆ THỐNG ---
static void logSystem(const string& message) {
    std::lock_guard<std::mutex> lock(logMutex); // Đảm bảo Thread-safe

    // 1. Lấy thời gian hiện tại
    auto now = std::chrono::system_clock::now();
    auto time_t_now = std::chrono::system_clock::to_time_t(now);
    
    // 2. Mở file log (Append mode)
    ofstream f("server_log.txt", ios::app);
    if (f.is_open()) {
        f << "[" << std::put_time(std::localtime(&time_t_now), "%Y-%m-%d %H:%M:%S") << "] " 
          << message << endl;
        f.close();
    }
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

// --- HÀM HELPER: BROADCAST MESSAGE ---
// Hàm này chịu trách nhiệm đóng gói và gửi tin nhắn đến danh sách subscribers
static void broadcastMessage(uint32_t topicId, const string& senderName, const string& message, int excludeSocket) {
    // 1. Đóng gói gói tin NOTIFY_MSG_TEXT
    // Cấu trúc: [TopicID 4B] + [UserLen 4B + UserStr] + [MsgLen 4B + MsgStr]
    PacketBuilder builder;
    builder.addInt(topicId);        // TopicID (Big Endian handled by addInt)
    builder.addString(senderName);  // SenderID_Len (4B) + SenderName
    builder.addString(message);     // Msg_Len (4B) + Content

    // 2. Truy cập Map subscribers an toàn với Thread
    // Sử dụng shared_lock (Read Lock) vì ta chỉ đọc danh sách, không sửa đổi
    std::shared_lock<std::shared_mutex> lock(subMutex);

    // Tìm xem topic này có ai sub không
    auto it = topicSubscribers.find(topicId);
    if (it != topicSubscribers.end()) {
        const set<int>& subscribers = it->second; // File hiện tại dùng set<int>
        
        for (int socketId : subscribers) {
            // Không gửi lại cho chính người gửi (nếu cần)
            if (socketId != excludeSocket) {
                // Gửi packet đi
                NetworkUtils::sendPacket(socketId, NOTIFY_MSG_TEXT, builder.getData(), builder.getSize());
            }
        }
    }
}

// --- HÀM HELPER: BROADCAST BINARY (FILE) ---
static void broadcastBinary(uint32_t topicId, const string& senderName, const string& ext, const string& binaryData, int excludeSocket) {
    // Cấu trúc gói tin: 
    // [TopicID 4B] + [SenderLen 4B + Sender] + [ExtLen 4B + Ext] + [DataLen 4B + Data]
    
    PacketBuilder builder;
    builder.addInt(topicId);
    builder.addString(senderName);
    builder.addString(ext);
    
    // addString tự động thêm độ dài (4 bytes) rồi đến nội dung -> Khớp với [DataLen][Data]
    // std::string trong C++ an toàn với dữ liệu nhị phân (chứa null byte vẫn OK)
    builder.addString(binaryData); 

    std::shared_lock<std::shared_mutex> lock(subMutex);

    auto it = topicSubscribers.find(topicId);
    if (it != topicSubscribers.end()) {
        const set<int>& subscribers = it->second;
        for (int socketId : subscribers) {
            if (socketId != excludeSocket) {
                NetworkUtils::sendPacket(socketId, NOTIFY_MSG_BIN, builder.getData(), builder.getSize());
            }
        }
    }
}

ServerHandler::ServerHandler(int socket) : clientSocket(socket) {
    this->isLogged = false;
    string msg = "New connection established on socket " + to_string(socket);
    cout << msg << endl;
    logSystem(msg); // Ghi log
}

ServerHandler::~ServerHandler() { 
    close(clientSocket); 
    string msg = "Closed connection on socket " + to_string(clientSocket);
    cout << msg << endl;
    logSystem(msg); // Ghi log
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
    while (true) {
        uint8_t opcode;
        vector<uint8_t> payload;

        bool success = NetworkUtils::recvPacket(clientSocket, opcode, payload);
        if (!success) {
            // Log lỗi disconnect đột ngột
            logSystem("Client disconnected unexpectedly (Socket " + to_string(clientSocket) + ")");
            break; 
        }

        // ... (Log debug in ra màn hình giữ nguyên) ...

        switch (opcode) {
            // ... (Các case cũ: REGISTER, LOGIN, CREATE_TOPIC...) ...
            case REQ_REGISTER: handleRegister(payload.data(), payload.size()); break;
            case REQ_LOGIN: handleLogin(payload.data(), payload.size()); break;
            case REQ_CREATE_TOPIC: handleCreateTopic(payload.data(), payload.size()); break;
            case REQ_DELETE_TOPIC: handleDeleteTopic(payload.data(), payload.size()); break;
            case REQ_GET_ALL_TOPICS: handleGetAllTopics(); break;
            case REQ_GET_MY_TOPICS: handleGetMyTopics(); break;
            case REQ_SUBSCRIBE: handleSubscribe(payload.data(), payload.size()); break;
            case REQ_UNSUBSCRIBE: handleUnsubscribe(payload.data(), payload.size()); break;
            
            // Cập nhật hàm xử lý Publish Text
            case REQ_PUBLISH_TEXT:
                handlePublishText(payload.data(), payload.size());
                break;

            // --- USE CASE MỚI: SPRINT 3 HISTORY ---
            case REQ_HISTORY: 
                handleGetHistory(payload.data(), payload.size());
                break;

            // ... (Giữ nguyên case REQ_PUBLISH_BIN nếu đã làm ở Sprint 2) ...
            
            default:
                string err = "Unknown OpCode: " + to_string((int)opcode);
                cout << err << endl;
                logSystem(err);
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
    
    PacketBuilder builder;

    {
        shared_lock<shared_mutex> lock(topicMutex);
        
        // 1. Đóng gói số lượng Topic (4 bytes)
        uint32_t count = topicDB.size(); 
        builder.addInt(count); // PacketBuilder tự xử lý htonl

        // 2. Loop và đóng gói từng item
        for (auto const& [key, val] : topicDB) {
            // PacketBuilder tự xử lý độ dài chuỗi + nội dung
            builder.addString(val.name);
            builder.addString(val.creator);
        }
    }

    NetworkUtils::sendPacket(clientSocket, RES_GET_ALL_TOPICS, builder.getData(), builder.getSize());
}

// Lấy Topic của tôi
void ServerHandler::handleGetMyTopics() {
    if (!isLogged) return;
    
    PacketBuilder builder;

    {
        shared_lock<shared_mutex> lock(topicMutex);
        
        // 1. Lọc trước để đếm số lượng
        uint32_t count = 0;
        for (auto const& [key, val] : topicDB) {
            if (val.creator == this->currentUser) {
                count++;
            }
        }
        
        // Đóng gói số lượng
        builder.addInt(count);

        // 2. Loop lại và đóng gói item
        for (auto const& [key, val] : topicDB) {
            if (val.creator == this->currentUser) {
                builder.addString(val.name);
                builder.addString(val.creator);
            }
        }
    }

    NetworkUtils::sendPacket(clientSocket, RES_GET_MY_TOPICS, builder.getData(), builder.getSize());
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

    PacketReader reader(payloadData, len);
    uint32_t topicId = reader.readInt();
    string msgContent = reader.readString();

    // 1. Phản hồi ACK
    uint8_t status = STATUS_SUCCESS;
    NetworkUtils::sendPacket(clientSocket, RES_PUBLISH, &status, 1);

    // 2. Broadcast (Gửi cho người khác)
    broadcastMessage(topicId, this->currentUser, msgContent, this->clientSocket);
    
    // 3. LOG LỊCH SỬ 
    {
        std::lock_guard<std::mutex> lock(historyMutex);
        ofstream f("topic_history.txt", ios::app);
        if (f.is_open()) {
            // Format: TopicID|Sender|Message
            f << topicId << "|" << this->currentUser << "|" << msgContent << endl;
            f.close();
        } else {
            logSystem("ERROR: Cannot write to topic_history.txt");
        }
    }

    cout << "[CHAT] Topic " << topicId << " | " << currentUser << ": " << msgContent << endl;
}

void ServerHandler::handlePublishBinary(const void* payloadData, uint32_t len) {
    if (!isLogged) return;

    PacketReader reader(payloadData, len);
    
    // 1. Parse Payload từ Client
    // Cấu trúc: [TopicID] [Ext (Len+Str)] [Data (Len+Str)]
    uint32_t topicId = reader.readInt();
    string extension = reader.readString(); 
    string binaryData = reader.readString(); // Đọc cả DataLen và Data vào string

    if (binaryData.empty()) {
        cout << "[ERROR] Empty file received from " << currentUser << endl;
        return; 
    }

    // 2. Lưu file vào server_storage
    // Tạo thư mục nếu chưa có (chmod 0777 để full quyền đọc ghi)
    // Lưu ý: mkdir trả về -1 nếu thư mục đã tồn tại, ta bỏ qua lỗi này
    mkdir("server_storage", 0777); 

    // Tạo tên file duy nhất: timestamp_username.ext
    auto now = std::chrono::system_clock::now().time_since_epoch();
    auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(now).count();
    
    string fileName = "server_storage/" + to_string(timestamp) + "_" + currentUser + "." + extension;

    // Ghi file với chế độ Binary (quan trọng)
    ofstream file(fileName, ios::binary);
    if (file.is_open()) {
        file.write(binaryData.data(), binaryData.size());
        file.close();
        cout << "[FILE] Saved " << fileName << " (" << binaryData.size() << " bytes) to Topic " << topicId << endl;
    } else {
        cout << "[ERROR] Cannot save file: " << fileName << endl;
    }

    // 3. Phản hồi cho người gửi (ACK)
    uint8_t status = STATUS_SUCCESS;
    NetworkUtils::sendPacket(clientSocket, RES_PUBLISH, &status, 1);

    // 4. Broadcast cho các user khác
    broadcastBinary(topicId, currentUser, extension, binaryData, clientSocket);
}

void ServerHandler::handleGetHistory(const void* payloadData, uint32_t len) {
    if (!isLogged) return;

    PacketReader reader(payloadData, len);
    uint32_t reqTopicId = reader.readInt();

    logSystem("User " + currentUser + " requested history for Topic " + to_string(reqTopicId));

    // Đọc file history và lọc tin nhắn của Topic này
    std::lock_guard<std::mutex> lock(historyMutex);
    ifstream f("topic_history.txt");
    
    if (f.is_open()) {
        string line;
        while (getline(f, line)) {
            // Format: TopicID|Sender|Message
            vector<string> parts = split(line, '|');
            if (parts.size() >= 3) {
                try {
                    uint32_t tId = stoi(parts[0]);
                    
                    // Nếu đúng Topic đang yêu cầu -> Gửi lại cho Client
                    if (tId == reqTopicId) {
                        string sender = parts[1];
                        string content = parts[2];

                        // Thêm tiền tố [HISTORY] để Client dễ nhận biết
                        string historyMsg = "[HISTORY] " + content;

                        // Đóng gói giả lập gói tin NOTIFY_MSG_TEXT
                        PacketBuilder builder;
                        builder.addInt(tId);
                        builder.addString(sender);
                        builder.addString(historyMsg);

                        // Gửi riêng cho socket này (KHÔNG broadcast)
                        NetworkUtils::sendPacket(clientSocket, NOTIFY_MSG_TEXT, builder.getData(), builder.getSize());
                    }
                } catch (...) {
                    continue; // Bỏ qua dòng lỗi
                }
            }
        }
        f.close();
    }
}