# Compiler Settings
CXX = g++
CXXFLAGS = -std=c++17 -Wall -pthread -Iinclude

# Directories
SRC_DIR = src
OBJ_DIR = obj

# Files
SOURCES = $(wildcard $(SRC_DIR)/*.cpp)
OBJECTS = $(patsubst $(SRC_DIR)/%.cpp, $(OBJ_DIR)/%.o, $(SOURCES))
EXECUTABLE = mini_redis_server

# Default Target
all: directories $(EXECUTABLE)

directories:
	@mkdir -p $(OBJ_DIR)

$(EXECUTABLE): $(OBJECTS)
	$(CXX) $(CXXFLAGS) -o $@ $^
	@echo "✅ Build Complete! Run with ./mini_redis_server"

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -rf $(OBJ_DIR) $(EXECUTABLE)
	@echo "🧹 Workspace cleaned!"