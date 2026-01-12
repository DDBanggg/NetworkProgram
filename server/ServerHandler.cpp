#include "ServerHandler.h"
#include "../common/NetworkUtils.h"
#include "../common/DataUtils.h"
#include "../common/protocol.h"
#include <iostream>
#include <vector>
#include <map>
#include <mutex>
#include <unistd.h> 
#include <fstream> 
#include <sstream>
#include <shared_mutex> 
#include <set>
#include <sys/stat.h>   
#include <sys/types.h>
#include <chrono>       
#include <iomanip> 

using namespace std;

// =========================================================
// 1. LIÊN KẾT BIẾN TOÀN CỤC TỪ SERVERAPP (FIX LỖI LINKER)
// =========================================================
extern std::map<int, std::string> onlineUsers; // Map: SocketID -> Username
extern std::shared_mutex clientsMutex;         // Mutex bảo vệ map trên

// --- CẤU TRÚC DỮ LIỆU NỘI BỘ SERVERHANDLER ---

struct Topic {
    uint32_t id;
    string name;
    string description;
    string creator;
};

// Database giả lập (Lưu trong RAM + File)
static map<string, string> userDB;
static shared_mutex dbMutex;

static map<string, Topic> topicDB; 
static shared_mutex topicMutex; 
static uint32_t globalTopicIdCounter = 0;

// Map: TopicID -> Set<Socket>
static map<uint32_t, set<int>> topicSubscribers;
static shared_mutex subMutex; 

// Mutex cho file log
static std::mutex logMutex;      
static std::mutex historyMutex;  

// --- CÁC HÀM TIỆN ÍCH (HELPER) ---

vector<string> split(const string& s, char delimiter) {
    vector<string> tokens;
    string token;
    istringstream tokenStream(s);
    while (getline(tokenStream, token, delimiter)) {
        tokens.push_back(token);
    }
    return tokens;
}

static void logSystem(const string& message) {
    std::lock_guard<std::mutex> lock(logMutex);
    auto now = std::chrono::system_clock::now();
    auto time_t_now = std::chrono::system_clock::to_time_t(now);
    
    ofstream f("server_log.txt", ios::app);
    if (f.is_open()) {
        f << "[" << std::put_time(std::localtime(&time_t_now), "%Y-%m-%d %H:%M:%S") << "] " 
          << message << endl;
        f.close();
    }
}

void saveUserToFile(const string& u, const string& p) {
    ofstream f("data/users.txt", ios::app);
    if (f.is_open()) { 
        f << u << "|" << p << endl; 
        f.close(); 
    }
}

void saveTopicToFile(uint32_t id, const string& name, const string& desc, const string& creator) {
    ofstream f("data/topics.txt", ios::app);
    if (f.is_open()) { 
        f << id << "|" << name << "|" << desc << "|" << creator << endl; 
        f.close(); 
    }
}

static void broadcastMessage(uint32_t topicId, const string& senderName, const string& message, int excludeSocket) {
    PacketBuilder builder;
    builder.addInt(topicId);       
    builder.addString(senderName); 
    builder.addString(message);    

    std::shared_lock<std::shared_mutex> lock(subMutex); 
    auto it = topicSubscribers.find(topicId);
    if (it != topicSubscribers.end()) {
        const set<int>& subscribers = it->second;
        for (int socketId : subscribers) {
            if (socketId != excludeSocket) {
                NetworkUtils::sendPacket(socketId, NOTIFY_MSG_TEXT, builder.getData(), builder.getSize());
            }
        }
    }
}

static void broadcastBinary(uint32_t topicId, const string& senderName, const string& ext, const string& binaryData, int excludeSocket) {
    PacketBuilder builder;
    builder.addInt(topicId);
    builder.addString(senderName);
    builder.addString(ext);
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

// --- CONSTRUCTOR / DESTRUCTOR ---

ServerHandler::ServerHandler(int socket) : clientSocket(socket) {
    this->isLogged = false;
    string msg = "New connection established on socket " + to_string(socket);
    cout << msg << endl;
    logSystem(msg);
}

ServerHandler::~ServerHandler() { 
    if (isLogged) {
        std::unique_lock<std::shared_mutex> lock(clientsMutex);
        onlineUsers.erase(clientSocket);
    }

    close(clientSocket); 
    string msg = "Closed connection on socket " + to_string(clientSocket);
    cout << msg << endl;
    logSystem(msg);
}

// --- LOAD DATA ---
void ServerHandler::loadData() {
    ifstream fUsers("data/users.txt");
    if (fUsers.is_open()) {
        string line;
        while (getline(fUsers, line)) {
            vector<string> parts = split(line, '|');
            if (parts.size() >= 2) {
                userDB[parts[0]] = parts[1];
            }
        }
        fUsers.close();
    }

    ifstream fTopics("data/topics.txt");
    if (fTopics.is_open()) {
        string line;
        while (getline(fTopics, line)) {
            vector<string> parts = split(line, '|');
            if (parts.size() >= 4) { 
                Topic t;
                t.id = stoi(parts[0]);
                t.name = parts[1];
                t.description = parts[2]; 
                t.creator = parts[3];
                topicDB[t.name] = t;
                if (t.id > globalTopicIdCounter) globalTopicIdCounter = t.id;
            }
        }
        fTopics.close();
    }
}

// --- MAIN LOOP ---
void ServerHandler::run() {
    while (true) {
        uint8_t opcode;
        vector<uint8_t> payload;

        if (!NetworkUtils::recvPacket(clientSocket, opcode, payload)) {
            logSystem("Client disconnected unexpectedly (Socket " + to_string(clientSocket) + ")");
            break; 
        }

        switch (opcode) {
            case REQ_REGISTER: handleRegister(payload.data(), payload.size()); break;
            case REQ_LOGIN: handleLogin(payload.data(), payload.size()); break;
            case REQ_CREATE_TOPIC: handleCreateTopic(payload.data(), payload.size()); break;
            case REQ_DELETE_TOPIC: handleDeleteTopic(payload.data(), payload.size()); break;
            case REQ_GET_ALL_TOPICS: handleGetAllTopics(); break;
            case REQ_GET_MY_TOPICS: handleGetMyTopics(); break;
            case REQ_SUBSCRIBE: handleSubscribe(payload.data(), payload.size()); break;
            case REQ_UNSUBSCRIBE: handleUnsubscribe(payload.data(), payload.size()); break;
            case REQ_PUBLISH_TEXT: handlePublishText(payload.data(), payload.size()); break;
            case REQ_PUBLISH_BIN: handlePublishBinary(payload.data(), payload.size()); break;
            case REQ_HISTORY: handleGetHistory(payload.data(), payload.size()); break;
            case REQ_TOPIC_INFO: {
                PacketReader reader(payload.data(), payload.size());
                uint32_t tId = reader.readInt();
                handleGetTopicInfo(tId); 
                break;
            }
            case REQ_TOPIC_SUBS: {
                PacketReader reader(payload.data(), payload.size());
                uint32_t tId = reader.readInt();
                handleGetTopicSubs(tId);
                break;
            }
            default:
                string err = "Unknown OpCode: " + to_string((int)opcode);
                cout << err << endl;
                break;
        }
    }
}

// --- HANDLERS ---

void ServerHandler::handleRegister(const void* payloadData, uint32_t len) {
    PacketReader reader(payloadData, len);
    string username = reader.readString();
    string password = reader.readString();

    uint8_t status;
    {
        unique_lock<shared_mutex> lock(dbMutex);
        if (userDB.find(username) != userDB.end()) {
            status = REGISTER_FAIL_EXISTS;
        } else {
            userDB[username] = password;
            saveUserToFile(username, password);
            status = REGISTER_OK;
        }
    }
    NetworkUtils::sendPacket(clientSocket, RES_REGISTER, &status, 1);
}

void ServerHandler::handleLogin(const void* payloadData, uint32_t len) {
    PacketReader reader(payloadData, len);
    string username = reader.readString();
    string password = reader.readString();

    uint8_t status;
    {
        shared_lock<shared_mutex> lock(dbMutex);
        if (userDB.find(username) == userDB.end()) status = LOGIN_FAIL_NOT_FOUND;
        else if (userDB[username] != password) status = LOGIN_FAIL_WRONG_PASS;
        else {
            status = LOGIN_OK;
            this->currentUser = username;
            this->isLogged = true;
            
            {
                unique_lock<shared_mutex> uLock(clientsMutex);
                onlineUsers[clientSocket] = username;
            }
            cout << "[LOGIN] User " << username << " is online." << endl;
        }
    }
    NetworkUtils::sendPacket(clientSocket, RES_LOGIN, &status, 1);
}

void ServerHandler::handleCreateTopic(const void* payloadData, uint32_t len) {
    if (!isLogged) return; 
    PacketReader reader(payloadData, len);
    string topicName = reader.readString();
    string topicDesc = reader.readString();

    uint8_t status;
    uint32_t newTopicId = 0;
    {
        unique_lock<shared_mutex> lock(topicMutex);
        if (topicDB.find(topicName) != topicDB.end()) {
            status = TOPIC_FAIL_EXISTS;
        } else {
            globalTopicIdCounter++;
            newTopicId = globalTopicIdCounter;
            Topic t = {newTopicId, topicName, topicDesc, this->currentUser};
            topicDB[topicName] = t;
            saveTopicToFile(newTopicId, topicName, topicDesc, this->currentUser);
            status = TOPIC_CREATE_OK;
        }
    }
    PacketBuilder builder;
    builder.addBlob(&status, 1); 
    if (status == TOPIC_CREATE_OK) builder.addInt(newTopicId);
    NetworkUtils::sendPacket(clientSocket, RES_CREATE_TOPIC, builder.getData(), builder.getSize());
}

void ServerHandler::handleDeleteTopic(const void* payloadData, uint32_t len) {
    if (!isLogged) return;
    PacketReader reader(payloadData, len);
    string topicName = reader.readString(); 
    
    uint8_t status;
    {
        unique_lock<shared_mutex> lock(topicMutex);
        auto it = topicDB.find(topicName);
        if (it == topicDB.end()) status = TOPIC_FAIL_NOT_FOUND;
        else if (it->second.creator != this->currentUser) status = TOPIC_FAIL_DENIED;
        else {
            topicDB.erase(it);
            status = TOPIC_DELETE_OK;
        }
    }
    NetworkUtils::sendPacket(clientSocket, RES_DELETE_TOPIC, &status, 1);
}

void ServerHandler::handleGetAllTopics() {
    if (!isLogged) return;
    PacketBuilder builder;
    {
        shared_lock<shared_mutex> lock(topicMutex);
        builder.addInt((uint32_t)topicDB.size()); 
        for (auto const& [key, val] : topicDB) {
            builder.addInt(val.id);
            builder.addString(val.name);
            builder.addString(val.description);
            builder.addString(val.creator);
        }
    }
    NetworkUtils::sendPacket(clientSocket, RES_GET_ALL_TOPICS, builder.getData(), builder.getSize());
}

void ServerHandler::handleGetMyTopics() {
    if (!isLogged) return;
    PacketBuilder builder;
    {
        shared_lock<shared_mutex> lock(topicMutex);
        uint32_t count = 0;
        for (auto const& [key, val] : topicDB) if (val.creator == this->currentUser) count++;
        builder.addInt(count);
        for (auto const& [key, val] : topicDB) {
            if (val.creator == this->currentUser) {
                builder.addInt(val.id);
                builder.addString(val.name);
                builder.addString(val.description);
                builder.addString(val.creator);
            }
        }
    }
    NetworkUtils::sendPacket(clientSocket, RES_GET_MY_TOPICS, builder.getData(), builder.getSize());
}

void ServerHandler::handleSubscribe(const void* payloadData, uint32_t len) {
    if (!isLogged) return;
    PacketReader reader(payloadData, len);
    uint32_t topicId = reader.readInt();
    {
        unique_lock<shared_mutex> lock(subMutex);
        topicSubscribers[topicId].insert(this->clientSocket);
    }
    uint8_t status = SUB_OK;
    NetworkUtils::sendPacket(clientSocket, RES_SUBSCRIBE, &status, 1);
}

void ServerHandler::handleUnsubscribe(const void* payloadData, uint32_t len) {
    if (!isLogged) return;
    PacketReader reader(payloadData, len);
    uint32_t topicId = reader.readInt();
    {
        unique_lock<shared_mutex> lock(subMutex);
        topicSubscribers[topicId].erase(this->clientSocket);
    }
    uint8_t status = UNSUB_OK;
    NetworkUtils::sendPacket(clientSocket, RES_UNSUBSCRIBE, &status, 1);
}

void ServerHandler::handlePublishText(const void* payloadData, uint32_t len) {
    if (!isLogged) return;
    PacketReader reader(payloadData, len);
    uint32_t topicId = reader.readInt();
    string msgContent = reader.readString();

    uint8_t status = STATUS_SUCCESS;
    NetworkUtils::sendPacket(clientSocket, RES_PUBLISH, &status, 1);

    broadcastMessage(topicId, this->currentUser, msgContent, this->clientSocket);
    
    {
        std::lock_guard<std::mutex> lock(historyMutex);
        ofstream f("topic_history.txt", ios::app);
        if (f.is_open()) {
            f << topicId << "|" << this->currentUser << "|" << msgContent << endl;
            f.close();
        }
    }
}

void ServerHandler::handlePublishBinary(const void* payloadData, uint32_t len) {
    if (!isLogged) return;
    PacketReader reader(payloadData, len);
    uint32_t topicId = reader.readInt();
    string extension = reader.readString(); 
    string binaryData = reader.readString();

    #ifdef _WIN32
        _mkdir("server_storage");
    #else
        mkdir("server_storage", 0777); 
    #endif

    auto now = std::chrono::system_clock::now().time_since_epoch();
    auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(now).count();
    string fileName = "server_storage/" + to_string(timestamp) + "_" + currentUser + extension;

    ofstream file(fileName, ios::binary);
    if (file.is_open()) {
        file.write(binaryData.data(), binaryData.size());
        file.close();
    }
    
    uint8_t status = STATUS_SUCCESS;
    NetworkUtils::sendPacket(clientSocket, RES_PUBLISH, &status, 1);
    broadcastBinary(topicId, currentUser, extension, binaryData, clientSocket);
}

void ServerHandler::handleGetHistory(const void* payloadData, uint32_t len) {
    if (!isLogged) return;
    PacketReader reader(payloadData, len);
    uint32_t reqTopicId = reader.readInt();

    vector<pair<string, string>> historyItems;
    {
        std::lock_guard<std::mutex> lock(historyMutex);
        ifstream f("topic_history.txt");
        if (f.is_open()) {
            string line;
            while (getline(f, line)) {
                vector<string> parts = split(line, '|');
                if (parts.size() >= 3 && (uint32_t)stoi(parts[0]) == reqTopicId) {
                    historyItems.push_back({parts[1], parts[2]});
                }
            }
        }
    }

    // 1. Gửi RES_HISTORY_START (OpCode 31)
    PacketBuilder startBuilder;
    startBuilder.addInt((uint32_t)historyItems.size());
    NetworkUtils::sendPacket(clientSocket, RES_HISTORY_START, startBuilder.getData(), startBuilder.getSize());

    // 2. Gửi từng RES_HISTORY_ITEM (OpCode 32)
    for (auto const& item : historyItems) {
        PacketBuilder itemBuilder;
        uint8_t msgType = 1; //1 là Text
        itemBuilder.addBlob(&msgType, 1); 
        // Payload của item: [SenderLen][Sender][MsgLen][Msg]
        itemBuilder.addString(item.first);  // Sender
        itemBuilder.addString("[HISTORY] " + item.second); // Nội dung
        NetworkUtils::sendPacket(clientSocket, RES_HISTORY_ITEM, itemBuilder.getData(), itemBuilder.getSize());
    }

    // 3. Gửi RES_HISTORY_END (OpCode 33)
    NetworkUtils::sendPacket(clientSocket, RES_HISTORY_END, nullptr, 0);
}

void ServerHandler::handleGetTopicInfo(uint32_t topicId) {
    if (!isLogged) return;
    string creator = "Unknown";
    uint32_t subCount = 0;
    {
        shared_lock<shared_mutex> lock(topicMutex); 
        for (auto const& [name, t] : topicDB) {
            if (t.id == topicId) {
                creator = t.creator;
                break;
            }
        }
    }
    {
        shared_lock<shared_mutex> lock(subMutex);
        if (topicSubscribers.find(topicId) != topicSubscribers.end()) {
            subCount = topicSubscribers[topicId].size();
        }
    }
    PacketBuilder builder;
    builder.addString(creator);
    builder.addInt(subCount);
    NetworkUtils::sendPacket(clientSocket, RES_TOPIC_INFO, builder.getData(), builder.getSize());
}

void ServerHandler::handleGetTopicSubs(uint32_t topicId) {
    if (!isLogged) return;
    vector<string> subscriberNames;
    vector<int> socketIds;
    {
        shared_lock<shared_mutex> lock(subMutex);
        if (topicSubscribers.find(topicId) != topicSubscribers.end()) {
            socketIds.assign(topicSubscribers[topicId].begin(), topicSubscribers[topicId].end());
        }
    }

    {
        shared_lock<shared_mutex> lock(clientsMutex); 
        for (int sock : socketIds) {
            if (onlineUsers.count(sock)) {
                subscriberNames.push_back(onlineUsers[sock]);
            }
        }
    }

    PacketBuilder builder;
    builder.addInt((uint32_t)subscriberNames.size()); 
    for (const string& name : subscriberNames) {
        builder.addString(name); 
    }
    NetworkUtils::sendPacket(clientSocket, RES_TOPIC_SUBS, builder.getData(), builder.getSize());
}