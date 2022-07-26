#include <asm-generic/socket.h>
#include <cstring>
#include <iostream>
#include <fcntl.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <arpa/inet.h>
#include "epoller.hpp"
#include "sqlconnpool.hpp"
#include <unordered_map>
#include "httpconn.hpp"
#include "threadpool.hpp"
#include <functional>

using namespace std;
class Webserver{
public:
    Webserver(int trigMode=0);
    ~Webserver();
    void start();
    void InitEventMode_(int trigMode = 0);
    bool InitSocket();
    int setnonblocking(int fd);
    void handleListenEvent();
    void handleReadEvent(HttpConn* client);
    void handleWriteEvent(HttpConn* client);
    void handleErrorEvent(HttpConn* client);
    void addClient_(int fd, sockaddr_in addr);
    void onRead_(HttpConn* client);
    void onWrite_(HttpConn* client);
    void onError_(HttpConn* client);
    void onProcess(HttpConn* client);

private:
    string srcDir_;
    SqlConnPool* pool_;
    uint32_t listenEvent_;
    uint32_t connEvent_;
    int port_;
    int listenFd_;
    bool isRunning_;

    std::unique_ptr<Epoller> epoller_; // epoller指针封装
    std::unordered_map<int, HttpConn> users_; // 为每个connfd维护一个httpconn连接
    std::unique_ptr<ThreadPool<std::function<void()>>> threadpool_; // 线程池指针封装

};

inline Webserver::Webserver(int trigMode): epoller_(new Epoller(1024)), threadpool_(new ThreadPool<std::function<void()>>(10)), isRunning_(true), port_(8888){
    // getcwd 获得执行server命令的进程的当前路径
    // 相当于srcDir_.assign(char *)
    srcDir_= getcwd(NULL, 0); 
    // 如果没有找到“build”，将返回npos,即size_t类型的最大值,erase 将出错
    // srcDir_.erase(srcDir_.find("build"), string::npos);
    srcDir_ += "/resources/";
    cout << srcDir_ << endl;

    // 初始化线程池
    // threadpool_ = new ThreadPool<std::function<void()>>(10);
    pool_ = SqlConnPool::getInstance();
    pool_->init("localhost", "root", "tobeNo.1", "mydb", 3306, 10);
    cout << pool_->getFreeConnCount() << endl;
    cout << "Webserver::Webserver()" << endl;

    // 触发模式
    InitEventMode_(trigMode);
    InitSocket();
    
}

inline Webserver::~Webserver(){
    cout << "Webserver::~Webserver()" << endl;
    close(listenFd_);
    isRunning_ = false;
}

inline void Webserver::start(){
    while(isRunning_){
        int eventCount = epoller_->wait(-1);
        std::cout << "triggered once..." << eventCount << std::endl;
        for(int i=0; i<eventCount; i++){
            int fd = epoller_->getFd(i);
            uint32_t events = epoller_->getEvent(i);
            // 客户端连接事件
            if(fd == listenFd_){
                std::cout << i <<" listenfd" <<std::endl;
                handleListenEvent();
            }
            else if(events & (EPOLLERR | EPOLLHUP | EPOLLRDHUP)){
                if(events&EPOLLERR){
                    std::cout << i << " EPOLLERR" << std::endl;
                }
                if(events&EPOLLHUP){
                    std::cout<< i << " EPOLLHUP" << std::endl;
                }
                if(events&EPOLLRDHUP){
                    std::cout << i << " EPOLLRDHUP" << std::endl;
                }
                handleErrorEvent(&users_[fd]);
            }
            // 客户端读事件
            else if(events & EPOLLIN){
                std::cout<< i<< " EPOLLIN" << std::endl;
                // HttpConn conn = users_[fd];
                // std::cout << conn.getFd() << std::endl;
                handleReadEvent(&users_[fd]);
            }
           
            // 客户端写事件
            else if(events & EPOLLOUT){
                std::cout << i << " EPOLLOUT" << std::endl;
                handleWriteEvent(&users_[fd]);
            }
            
            // else{
            //     cout << "unknown event" << endl;
            // }
        }
    }
}

inline void Webserver::InitEventMode_(int trigMode){
    listenEvent_ = EPOLLRDHUP;
    connEvent_ = EPOLLRDHUP | EPOLLONESHOT;
    switch(trigMode){
        case 0:
            break;
        case 1:
            connEvent_ |= EPOLLET;
            break;
        case 2:
            listenEvent_ |= EPOLLET;
            break;
        case 3:
            listenEvent_ |= EPOLLET;
            connEvent_ |= EPOLLET;
            break;
        default:
            listenEvent_ |= EPOLLET;
            connEvent_ |= EPOLLET;
            break;
    }
    HttpConn::isET = (connEvent_ & EPOLLET); 
}

inline bool Webserver::InitSocket(){
    int ret;
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(port_);
    listenFd_ = socket(AF_INET, SOCK_STREAM, 0);
    if(listenFd_ < 0){
        cout << "socket error" << endl;
        exit(1);
    }
    int reuse = 1;
    ret = setsockopt(listenFd_, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
    // todo: linger作用:优雅退出
    // struct linger optlinger = {0};
    // optlinger.l_onoff = 1;
    // optlinger.l_linger = 1;
    // ret = setsockopt(listenFd_, SOL_SOCKET, SO_LINGER, &optlinger, sizeof(optlinger));
    ret = bind(listenFd_, (struct sockaddr*)&addr, sizeof(addr));
    listen(listenFd_, 5);
    // 将监听套接字加入epoll中
    epoller_->addFd(listenFd_, listenEvent_ | EPOLLIN); //主线程注册listen读事件
    setnonblocking(listenFd_);
    return true;

}

inline int Webserver::setnonblocking(int fd){
    int old_option = fcntl(fd, F_GETFL);
    int new_option = old_option | O_NONBLOCK;
    fcntl(fd, F_SETFL, new_option);
    return old_option;
}

// 处理连接事件
inline void Webserver::handleListenEvent(){
    do {
    struct sockaddr_in clientAddr;
    socklen_t clientAddrLen = sizeof(clientAddr);
    int connFd = accept(listenFd_, (struct sockaddr*)&clientAddr, &clientAddrLen);
    if(connFd < 0){
        cout << "accept error" << endl;
        return;
    }
    addClient_(connFd, clientAddr);

    // epoller_->addFd(connFd, connEvent_ | EPOLLIN);
    // setnonblocking(connFd);
    // char buf[20];
    // inet_ntop(AF_INET, &clientAddr.sin_addr, buf, sizeof(buf));
    // std::cout << "accept a new connection:" << connFd << " " << buf << " "<< htons(clientAddr.sin_port)<<std::endl;


    } while(listenEvent_ & EPOLLET); // do while至少执行一次， 如果是epolllt，那么连接完后退出，如果是epollet那么进入循环，所以这里不能用while，否则lt的那一次连接都连接不上
}

inline void Webserver::handleReadEvent(HttpConn* client){
    std::cout << "ddd" << std::endl;
    // int errno_ =0;
    // client->read_(&errno_);
    threadpool_->append(std::bind(&Webserver::onRead_, this, client));

}

inline void Webserver::handleWriteEvent(HttpConn* client){


}

inline void Webserver::handleErrorEvent(HttpConn* client){
    epoller_->delFd(client->getFd());
    client->close_();
}

void Webserver::addClient_(int fd, sockaddr_in addr){
    users_[fd].init(fd, addr);
    epoller_->addFd(fd, connEvent_| EPOLLIN);
    setnonblocking(fd);
}

void Webserver::onRead_(HttpConn* client){
    int readErrno = 0;
    client->read_(&readErrno);
    onProcess(client);

}

void Webserver::onProcess(HttpConn* client){
    if(0){
        epoller_->modFd(client->getFd(), connEvent_ | EPOLLOUT);
    }
    else{
        epoller_->modFd(client->getFd(), connEvent_ | EPOLLIN);
    }
}