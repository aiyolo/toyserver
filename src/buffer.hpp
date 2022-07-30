#ifndef BUFFER_HPP__
#define BUFFER_HPP__ 1

#include <iostream>
#include <vector>
#include <unistd.h>
#include <assert.h>
#include <sys/uio.h> // readv

class Buffer
{
public:
static const size_t kPrependSize = 8;
static const size_t kInitialSize = 1024;
    Buffer(size_t initBufferSize = kInitialSize);
    ~Buffer() = default;

    size_t writableBytes() const;
    size_t readableBytes() const;
    size_t prependableBytes() const;

    const char* peek() const;
    void retrieve(size_t len);
    void retrieveUntil(const char* end);
    void retrieveAll();
    std::string retrieveAsString(size_t len);
    std::string retrieveAllAsString();

    const char* beginWriteConst() const;
    char* beginWrite();

    void append(const char* data, size_t len);
    void append(const std::string& str);
    void append(const Buffer& buffer);

    void prepend(const char* data, size_t len);
    void prepend(const std::string& str);
    void shrink(size_t reserve);
    void ensureWritableBytes(size_t len);
    void swap(Buffer& rhs);
    void hasWritten(size_t len);

    const char* findCRLF() const;
    const char* findCRLF(const char* start) const;
    const char* findEOL() const;
    const char* findEOL(const char* start) const;
    const char* findEOL(const char* start, const char* end) const;
    const char* findEOL(const char* start, const char* end, const char* eol) const;

    ssize_t readFd(int fd, int* savedErrno);
    ssize_t writeFd(int fd, int* savedErrno);

private:
    void makeSpace(size_t len);
    char* begin();
    const char* begin() const;


private:
    std::vector<char> buffer_;
    size_t readIndex_;
    size_t writeIndex_;
};

// 仿照muduo 预留一部分空间
inline Buffer::Buffer(size_t initBufferSize)
    : buffer_(initBufferSize)
    , readIndex_(kPrependSize)
    , writeIndex_(kPrependSize)
{
}

inline size_t Buffer::writableBytes() const
{
    return buffer_.size() - writeIndex_;
}

inline size_t Buffer::readableBytes() const
{
    return writeIndex_ - readIndex_;
}

inline size_t Buffer::prependableBytes() const
{
    return readIndex_;
}

inline const char* Buffer::peek() const
{
    return begin() + readIndex_;
}

inline char* Buffer::begin()
{
    return &*buffer_.begin();
}

inline const char* Buffer::begin() const
{
    return &*buffer_.begin();
}

inline void Buffer::retrieve(size_t len)
{
    assert(len <= readableBytes());
    if(len <= readableBytes())
    {
        readIndex_ += len;
    }
    else
    {
        retrieveAll();
    }
}

inline void Buffer::retrieveUntil(const char* end)
{
    assert(peek()<=end);
    assert(end <= beginWrite());
    retrieve(end - peek());
}

inline void Buffer::retrieveAll()
{
    readIndex_ = kPrependSize;
    writeIndex_ = kPrependSize;
}

inline std::string Buffer::retrieveAllAsString()
{
    std::string str(peek(), readableBytes());
    retrieveAll();
    return str;
}

inline const char* Buffer::beginWriteConst() const
{
    return begin() + writeIndex_;
}
    
inline char* Buffer::beginWrite()
{
    return begin() + writeIndex_;
}

inline void Buffer::hasWritten(size_t len)
{
    assert(len <= writableBytes());
    writeIndex_ += len;
}

inline void Buffer::append(const std::string& str)
{
    append(str.data(), str.size());
}

inline void Buffer::append(const char* data, size_t len)
{
    ensureWritableBytes(len);
    std::copy(data, data + len, beginWrite());
    hasWritten(len);
}

inline void Buffer::append(const Buffer& buff)
{
    append(buff.peek(), buff.readableBytes());
}

inline void Buffer::ensureWritableBytes(size_t len)
{
    if(writableBytes() < len)
    {
        makeSpace(len);
    }
    assert(writableBytes() >= len);
}

// 仿照muduo设计
inline ssize_t Buffer::readFd(int fd, int* savedErrno)
{
    char extrabuf[65536];
    struct iovec iov[2];
    size_t writable = writableBytes();

    iov[0].iov_base = begin() + writeIndex_;
    iov[0].iov_len = writable;
    iov[1].iov_base = extrabuf;
    iov[1].iov_len = sizeof extrabuf;

    // 如果buff_的可写空间不足，则读入多出的部分到extrabuf中
    // 否则直接写入到buff_中
    const int iovcnt = (writable<sizeof extrabuf) ? 2 : 1;
    const ssize_t len = readv(fd, iov, iovcnt);
    // 读出错
    if(len < 0)
    {
        *savedErrno = errno;
    }
    // buff_空间充足
    else if(static_cast<size_t>(len)<=writable)
    {
        writeIndex_ += len;
    }
    // buff_空间不足
    else
    {
        writeIndex_ = buffer_.size();
        append(extrabuf, len - writable);// 将多出的部分写入到buff_中
    }
    return len;
}

inline ssize_t Buffer::writeFd(int fd, int* savedErrno)
{
    size_t readable= readableBytes();
    ssize_t len = write(fd, peek(), readable);
    if(len<0)
    {
        *savedErrno = errno;
    }
    else
    {
        retrieve(static_cast<size_t>(len));
    }
    return len;
}

inline void Buffer::makeSpace(size_t len)
{
    // 如果可写空间小于len，那么就扩展buff_的大小
    if(writableBytes() + prependableBytes() < len + kPrependSize)
    {
        buffer_.resize(writeIndex_+len);
    }
    // 否则，将可读的数据移动到前面，使得移位后的可读空间足够
    else
    {
        size_t readable = readableBytes();
        std::copy(begin() + readIndex_, begin() + writeIndex_, begin()+kPrependSize);
        readIndex_ = kPrependSize;
        writeIndex_ = readIndex_ + readable;
        assert(readable == readableBytes());
    }
}

#endif

