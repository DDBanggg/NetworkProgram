# 1. Trình biên dịch và Cờ (Flags)
CXX      = g++
# [QUAN TRỌNG]: Bắt buộc dùng c++17 để hỗ trợ shared_mutex và structured bindings
# Nếu để c++11 sẽ bị lỗi biên dịch server
CXXFLAGS = -std=c++17 -pthread -Wall

# 2. Tên file chạy đầu ra (Executable)
CLIENT_BIN = client_app
SERVER_BIN = server_app

# 3. Định nghĩa các file nguồn (Source files)

# Common: Các file dùng chung cho cả 2 bên
COMMON_SRC = common/NetworkUtils.cpp

# Client: Bao gồm Main App + Handler + Common
CLIENT_SRC = client/ClientApp.cpp client/ClientHandler.cpp $(COMMON_SRC)

# Server: Bao gồm Main App + Handler + Common
SERVER_SRC = server/ServerApp.cpp server/ServerHandler.cpp $(COMMON_SRC)

# 4. Quy tắc biên dịch (Targets)

# Gõ 'make' sẽ chạy target này -> build cả Client và Server
all: $(CLIENT_BIN) $(SERVER_BIN)

# Quy tắc build Client
$(CLIENT_BIN): $(CLIENT_SRC)
	$(CXX) $(CXXFLAGS) -o $(CLIENT_BIN) $(CLIENT_SRC)

# Quy tắc build Server
$(SERVER_BIN): $(SERVER_SRC)
	$(CXX) $(CXXFLAGS) -o $(SERVER_BIN) $(SERVER_SRC)

# 5. Lệnh dọn dẹp file build (Gõ 'make clean' để xóa file cũ đi build lại)
clean:
	rm -f $(CLIENT_BIN) $(SERVER_BIN)