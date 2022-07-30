#ifndef HTTPCONN_HTPP__
#define HTTPCONN_HTPP__ 1

#include <arpa/inet.h>
#include <atomic>
#include <iostream>
#include <string>
#include <sys/socket.h>
#include <unistd.h>
#include "buffer.hpp"
#include "httprequest.hpp"
#include "httpresponse.hpp"


class HttpConn
{
public:
    HttpConn();
    ~HttpConn();
    void init(int fd, const sockaddr_in &addr);
    ssize_t handleRead(int *savedErrno);
    ssize_t handleWrite(int *savedErrno);
    void handleClose();
    int getFd() const; // 常成员函数
    int getPort() const;
    std::string getIP() const;
    sockaddr_in getAddr() const;
    bool process();
    int toWriteBytes();
    bool isKeepAlive();

    static bool isET;
    static std::string srcDir;
    static std::atomic<int> userCount;

private:
    int fd_;
    sockaddr_in addr_;
    bool isClosed_;

    int iovcnt_;
    struct iovec iov_[2];

    Buffer inputBuffer_;
    Buffer outputBuffer_;

    HttpRequest request_;
    HttpResponse response_;
};

inline std::atomic<int> HttpConn::userCount(0);
inline std::string HttpConn::srcDir;
inline bool HttpConn::isET;

inline HttpConn::HttpConn()
{
    fd_ = -1;
    addr_ = {0};
    isClosed_ = true;
}

inline HttpConn::~HttpConn()
{
    handleClose();
}

inline void HttpConn::init(int fd, const sockaddr_in &addr)
{
    fd_ = fd;
    addr_ = addr;
    userCount++;
    outputBuffer_.retrieveAll();
    inputBuffer_.retrieveAll();
    isClosed_ = false;
    std::cout << "init new connection" << std::endl;
}

inline void HttpConn::handleClose()
{
    // response_.unmapFile();
    // 如果已经关闭，则不再关闭
    if (isClosed_ == false)
    {
        isClosed_ = true;
        userCount--;
        close(fd_);
        std::cout << "close connection" << std::endl;
    }
}


inline int HttpConn::getFd() const
{
    return fd_;
}

inline sockaddr_in HttpConn::getAddr() const
{
    return addr_;
}

inline std::string HttpConn::getIP() const
{
    char ip_[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &addr_.sin_addr, ip_, sizeof(ip_));
    return ip_;
    // return inet_ntoa(addr_.sin_addr);
}

inline int HttpConn::getPort() const
{
    return ntohs(addr_.sin_port);
}

inline ssize_t HttpConn::handleRead(int *savedErrno)
{
    ssize_t len  = -1;
    do{
        // todo:et模式下，读到出错，这里没体现
        // 如何判断结束fin？
        len = inputBuffer_.readFd(fd_, savedErrno);
        if(len <=0){
            *savedErrno = errno;
            break;
        }
    } while(isET);
    return len;
}

// 不能只使用buffer.writeFd(), 因为还要传输文件部分，怎么优化这个过程
inline ssize_t HttpConn::handleWrite(int* savedErrno)
{
    ssize_t len=-1;
    do{
        len = writev(fd_, iov_, iovcnt_);
        if(len <=0)
        {
            *savedErrno = errno;
            break;
        }
        // 单次传输数据，传输了 文件 部分数据
        if(static_cast<size_t>(len) > iov_[0].iov_len)
        {
            iov_[1].iov_base = static_cast<uint8_t*>(iov_[1].iov_base) +(len - iov_[0].iov_len);
            iov_[1].iov_len -= (len - iov_[0].iov_len);
            if(iov_[0].iov_len)
            {
                outputBuffer_.retrieveAll();
                iov_[0].iov_len = 0;
            }
        }
        else{
            iov_[0].iov_base = (uint8_t*)iov_[0].iov_base + len;
            iov_[0].iov_len -= len;
            outputBuffer_.retrieve(len);

        }
    }while(isET || toWriteBytes()>10240); // lt模式下一次传输大量数据，也要进入循环
    return len;
}

inline bool HttpConn::process()
{
    request_.init();
    // 没有请求数据
    if(inputBuffer_.readableBytes()<=0)
    {
        return false;
    }
    else if(request_.parse((inputBuffer_)))
    {
        std::cout << "1111" << srcDir << std::endl;
        response_.init(srcDir, request_.path(), request_.isKeepAlive(), 200);
    }
    else
    {
        std::cout << "0000" << srcDir << std::endl;
        response_.init(srcDir, request_.path(), false, 400);
    }
    // 响应头
    response_.makeResponse(outputBuffer_);
    iov_[0].iov_base = const_cast<char*>(outputBuffer_.peek());
    iov_[0].iov_len = outputBuffer_.readableBytes();
    iovcnt_=1;
    // 文件数据
    if(response_.fileLen()>0 && response_.mapFile())
    {
        iov_[1].iov_base = response_.mapFile();
        iov_[1].iov_len = response_.fileLen();
        iovcnt_ = 2;
    }
    return true;
}

inline int HttpConn::toWriteBytes()
{
    return iov_[0].iov_len + iov_[1].iov_len;
}

inline bool HttpConn::isKeepAlive()
{
    return request_.isKeepAlive();
}
#endif // HTTPCONN_HTPP__
