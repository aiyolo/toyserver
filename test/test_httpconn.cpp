#include <iostream>
#include <arpa/inet.h>
#include <unordered_map>

#include "../src/httpconn.hpp"

int main(){
    sockaddr_in addr;
    std::unordered_map<int, HttpConn> users;
    users[0].init(0, addr);
    users[1].init(1, addr);
    std::cout << users.size() << std::endl;
    std::cout << users[0].getFd() << users[1].getFd() <<  std::endl;

    return 0;
}