#ifndef HTTPRESPONSE_HPP__
#define HTTPRESPONSE_HPP__ 1

#include <unordered_map>
#include <string>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include "buffer.hpp"


class HttpResponse {
public:
  HttpResponse();
  ~HttpResponse();

  void init(const std::string &srcDie, std::string &path,
            bool isKeepAline = false, int code = -1);
  void makeResponse(Buffer& buff);
  void unmapFile();
  char* mapFile();
  size_t fileLen() const;
  void errorContent(Buffer& buff, std::string msg);
  int code() const;
private:
    void addStateLine_(Buffer &buff);
    void addHeader_(Buffer &buff);
    void addContent_(Buffer &buff);

    void errorHtml_();
    std::string getFileType_();

    int code_;
    bool isKeepAlive_;

    std::string path_;
    std::string srcDir_;

    char* mmFile_;
    struct stat mmFileStat_;

    static const std::unordered_map<std::string, std::string> SUFFIX_TYPE;
    static const std::unordered_map<int, std::string> CODE_STATUS;
    static const std::unordered_map<int, std::string> CODE_PATH;

};

inline const std::unordered_map<std::string, std::string> HttpResponse::SUFFIX_TYPE = {
        { ".html",  "text/html" },
    { ".xml",   "text/xml" },
    { ".xhtml", "application/xhtml+xml" },
    { ".txt",   "text/plain" },
    { ".rtf",   "application/rtf" },
    { ".pdf",   "application/pdf" },
    { ".word",  "application/nsword" },
    { ".png",   "image/png" },
    { ".gif",   "image/gif" },
    { ".jpg",   "image/jpeg" },
    { ".jpeg",  "image/jpeg" },
    { ".au",    "audio/basic" },
    { ".mpeg",  "video/mpeg" },
    { ".mpg",   "video/mpeg" },
    { ".avi",   "video/x-msvideo" },
    { ".gz",    "application/x-gzip" },
    { ".tar",   "application/x-tar" },
    { ".css",   "text/css "},
    { ".js",    "text/javascript "},
};

// 支持一个正常码，和3个错误码
inline const std::unordered_map<int, std::string> HttpResponse::CODE_STATUS = {
    { 200, "OK" },
    { 400, "Bad Request" },
    { 403, "Forbidden" },
    { 404, "Not Found" },
};

// 错误码对应的页面
inline const std::unordered_map<int, std::string> HttpResponse::CODE_PATH = {
    { 400, "/400.html" },
    { 403, "/403.html" },
    { 404, "/404.html" },
};

inline HttpResponse::HttpResponse()
{
    code_ = -1;
    path_ = srcDir_ = "";
    isKeepAlive_ =false;
    mmFile_ = nullptr;
    mmFileStat_ = {0};
}

inline HttpResponse::~HttpResponse()
{
    unmapFile();
}

inline void HttpResponse::init(const std::string &srcDir, std::string& path, bool isKeepAlive, int code)
{
    if(mmFile_) unmapFile();
    code_ = code;
    isKeepAlive_ = isKeepAlive;
    srcDir_ = srcDir;
    path_ = path;
    mmFile_ = nullptr;
    mmFileStat_ = {0};
}

inline void HttpResponse::makeResponse(Buffer& buff)
{
    std::string filePath = srcDir_ + path_;
    std::cout << filePath << std::endl;
    if(stat(filePath.c_str(), &mmFileStat_) < 0)
    {
        std::cout << "stat error" << std::endl;
    }
    if(S_ISDIR(mmFileStat_.st_mode))
    {
        code_ = 404;
    }
    else if(!(mmFileStat_.st_mode & S_IRUSR))
    {
        code_ = 403;
    }
    else 
    {
        code_ = 200;
    }
    errorHtml_();
    addStateLine_(buff);
    addHeader_(buff);
    addContent_(buff);
}

inline char* HttpResponse::mapFile()
{
    return mmFile_;
}

inline size_t HttpResponse::fileLen() const
{
    return mmFileStat_.st_size;
}

inline void HttpResponse::errorHtml_()
{
    // 如果code_在错误码页面中,读取错误码页面文件内容
    if(CODE_PATH.count(code_))
    {
        path_ = CODE_PATH.find(code_)->second;
        stat((srcDir_+path_).c_str(), &mmFileStat_);
    }

}

inline void HttpResponse::addStateLine_(Buffer& buff)
{
    std::string status;
    if(CODE_STATUS.count(code_))
    {
        status = CODE_STATUS.find(code_)->second;
    }
    else // 未知类型的错误码统一返回400，bad request
    {
        code_ = 400;
        status = CODE_STATUS.find(400)->second;
    }
    buff.append("HTTP/1.1 "+ std::to_string(code_)+" "+ status+"\r\n");
}

inline void HttpResponse::addHeader_(Buffer& buff)
{
    buff.append("Connection: ");
    if(isKeepAlive_)
    {
        buff.append("Keep-Alive\r\n");
        buff.append("Keep-Alive: max=6, timeout=120\r\n");
    }
    else{
        buff.append("close\r\n");
    }
    buff.append("Content-Type: " + getFileType_()+"\r\n");
}

inline void HttpResponse::addContent_(Buffer& buff)
{
    int fd = open((srcDir_+path_).c_str(), O_RDONLY);
    if(fd<0)
    {
        errorContent(buff, "File NotFound!");
        return;
    }
    char* mmFile = (char*)mmap(0, mmFileStat_.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
    if(!mmFile)
    {
        errorContent(buff, "File NotFound!");
        return;
    }
    mmFile_ = mmFile;
    close(fd);
    buff.append("Content-Length: " + std::to_string(mmFileStat_.st_size) + "\r\n\r\n");
}

inline void HttpResponse::unmapFile()
{
    if(mmFile_)
    {
        munmap(mmFile_, mmFileStat_.st_size);
        mmFile_ = nullptr;
    }
}

std::string HttpResponse::getFileType_()
{
    std::string::size_type idx = path_.find_last_of(".");
    if(idx != std::string::npos)
    {
        std::string suffix = path_.substr(idx); 
        if(SUFFIX_TYPE.count(suffix))
        {
            return SUFFIX_TYPE.find(suffix)->second;
        }
    }
    return "text/plain";
}

inline void HttpResponse::errorContent(Buffer& buff, std::string msg)
{
    std::string body, status;
    body += "<html><title>Error</title>";
    body += "<body bgcolor=\"ffffff\">";
    if(CODE_STATUS.count(code_))
    {
        status = CODE_STATUS.find(code_)->second;
    }
    else{
        status = "Bad Request";
    }
    body += std::to_string(code_) + " : " + status + "\n";
    body += "<p>" + msg + "</p>";
    buff.append("Content-length: " + std::to_string(body.size()) + "\r\n\r\n");
    buff.append(body);
}
#endif // HTTPRESPONSE_HPP__