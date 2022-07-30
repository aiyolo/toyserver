#ifndef EPOLLER_HPP__
#define EPOLLER_HPP__ 1

#include <sys/socket.h>
#include <sys/epoll.h>
#include <unistd.h>
#include <vector>
#include <assert.h>

class Epoller{
public:
    Epoller(int maxEvent);
    ~Epoller();
    bool addFd(int fd, uint32_t events);
    bool delFd(int fd);
    bool modFd(int fd, uint32_t events);
    int wait(int timeout);
    int getFd(int index);
    uint32_t getEvent(int index);
    int getEventCount();
private:
    int epollFd_;
    std::vector<struct epoll_event> events_; // 元素类型为epoll_event的数组
};

inline Epoller::Epoller(int maxEvent){
    epollFd_ = epoll_create(1024);
    events_ = std::vector<struct epoll_event>(maxEvent);
    assert(epollFd_ != -1 && events_.size()!=0);
}

inline Epoller::~Epoller(){
    close(epollFd_);
}

inline bool Epoller::addFd(int fd, uint32_t events){
    struct epoll_event ev;
    ev.data.fd = fd;
    ev.events = events; // 可能同时加入多个事件
    int ret = epoll_ctl(epollFd_, EPOLL_CTL_ADD, fd, &ev);
    return ret != -1;
}

inline bool Epoller::modFd(int fd, uint32_t events){
    struct epoll_event ev;
    ev.data.fd = fd;
    ev.events = events;
    int ret = epoll_ctl(epollFd_, EPOLL_CTL_MOD, fd, &ev);
    return ret != -1;
}

inline bool Epoller::delFd(int fd){
    struct epoll_event ev = {0};
    int ret = epoll_ctl(epollFd_, EPOLL_CTL_DEL, fd, &ev); // 或者把ev换成NULL
    return ret != -1;
}

// ret 返回事件的数量，内核将发生的事件写到events_数组里,events_数组的大小是maxEvent
inline int Epoller::wait(int timeout){
    int ret = epoll_wait(epollFd_, &events_[0], events_.size(), timeout); // 和&events_有什么区别
    return ret;
}

inline int Epoller::getFd(int index){
    return events_[index].data.fd;
}

inline uint32_t Epoller::getEvent(int index){
    return events_[index].events;
}

#endif