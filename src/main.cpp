#include "../include/server.h"

int main() {
    // Boot the server on Port 6379 with a 10-Thread Pool
    Server redis_server(6379, 10);
    redis_server.start();
    return 0;
}