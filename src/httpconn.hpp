#include <iostream>
#include <arpa/inet.h>
#include <string>
#include <atomic>
#include <sys/socket.h>
#include <unistd.h>



class HttpConn{
public:
    HttpConn();
    ~HttpConn();
    void init(int fd, const sockaddr_in &addr);
    ssize_t read_(int* saveErrno);
    ssize_t write_(int* saveErrno);
    void close_();
    int getFd() const; // 常成员函数
    int getPort() const;
    const std::string getIP() const;
    sockaddr_in getAddr() const;
    bool precess;
    int toWriteBytes();
    bool isKeepAlive();

    static bool isET;
    static std::string srcDir_;
    static std::atomic<int> userCount;
private:
     int fd_;
     sockaddr_in addr_;
     bool isClosed_;

};


std::atomic<int> HttpConn::userCount(0);
std::string HttpConn::srcDir_;
bool HttpConn::isET;

HttpConn::HttpConn(){
    fd_ = -1;
    addr_ = {0};
    isClosed_ = true;
}

HttpConn::~HttpConn(){
    close_();
}

void HttpConn::init(int fd, const sockaddr_in &addr){
    fd_ = fd;
    addr_ = addr;
    userCount++;
    isClosed_ = false;
    std::cout << "init new connection" << std::endl;
}

void HttpConn::close_(){
    // 如果已经关闭，则不再关闭
    if(isClosed_ == false){
        isClosed_ = true;
        userCount--;
        close(fd_);
        std::cout << "close connection" << std::endl;
    }
}

ssize_t HttpConn::read_(int* saveErrno){
    ssize_t len=-1;
    char buf_[1024];
    do{
        len = read(fd_, buf_, sizeof(buf_));
        if(len <=0){
            break;
        }
    }while(isET);
    std::cout << buf_ << std::endl;
    return len;
}

int HttpConn::getFd() const{
    return fd_;
}