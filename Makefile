CXX = g++
CXXFLAGS = -std=c++17 -Wall -pthread -Iinclude
SRC_DIR = src
OBJ_DIR = obj

SERVER_SRCS = $(filter-out src/cli.cpp, $(wildcard $(SRC_DIR)/*.cpp))
CLI_SRCS = src/cli.cpp

SERVER_OBJS = $(patsubst $(SRC_DIR)/%.cpp, $(OBJ_DIR)/%.o, $(SERVER_SRCS))
CLI_OBJS = $(patsubst $(SRC_DIR)/%.cpp, $(OBJ_DIR)/%.o, $(CLI_SRCS))

all: directories mini_redis_server mini_redis_cli

directories:
	@mkdir -p $(OBJ_DIR)

mini_redis_server: $(SERVER_OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^
	@echo "✅ Server Build Complete!"

mini_redis_cli: $(CLI_OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^
	@echo "✅ CLI Build Complete!"

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -rf $(OBJ_DIR) mini_redis_server mini_redis_cli
	@echo "🧹 Workspace cleaned!"