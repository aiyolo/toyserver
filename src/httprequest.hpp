#ifndef HTTPREQUEST_HPP__
#define HTTPREQUEST_HPP__ 1

#include "buffer.hpp"
#include <iostream>
#include <string>
#include <unordered_map>
#include <vector>
#include <unordered_set>
#include <algorithm>
#include <regex>
#include <mysql.h>
#include "sqlconnpool.hpp"

class HttpRequest {
public:
    enum PARSE_STATE{
        REQUEST_LINE,
        HEADERS,
        BODY,
        FINISH
    };
    enum HTTPCODE{
        NO_REQUEST,
        GET_REQUEST,
        POST_REQUEST,
        BAD_REQUEST,
        FORBIDEND_REQUEST,
        NO_RESOURCE,
        FILE_REQUEST,
        INTERNAL_ERROR,
        CLOSED_CONNECTION,
    };
    HttpRequest();
    ~HttpRequest() = default;

    void init();
    bool parse(Buffer& buff);

    std::string path() const;
    std::string& path();
    std::string method() const;
    std::string version() const;
    std::string getPost(const std::string& key) const;
    std::string getPost(const char* key) const;
    int convertHex(char ch);
    bool isKeepAlive() const;

// private:
    bool parseRequestLine_(const std::string& line);
    void parseHeader_(const std::string& line);
    void parseBody_(const std::string& line);

    void parsePath_();
    void parsePost_();

    void parseFromUrlencoded_();
    static bool userVerify(const std::string &name, const std::string& pwd, bool isLogin);
    
    PARSE_STATE state_;
    std::string method_, path_, version_, body_;
    std::unordered_map<std::string, std::string> headers_;
    std::unordered_map<std::string, std::string> post_;
    static const std::unordered_set<std::string> DEFAULT_HTML;
    static const std::unordered_map<std::string, int> DEFAULT_HTML_TAG;
    static int converHex(char ch);
};

// inline变量是c++17的扩展
inline const std::unordered_set<std::string> HttpRequest::DEFAULT_HTML = {
    "/index",
    "/register",
    "/login",
    "/welcome",
    "/video",
    "/picture",
};

inline const std::unordered_map<std::string, int>
HttpRequest::DEFAULT_HTML_TAG = {
    {"/register.html", 0},
    {"/login.html", 1},
};

inline HttpRequest::HttpRequest()
{
    init();
}

inline void HttpRequest::init()
{
    state_ = REQUEST_LINE;
    method_ = "";
    path_ = "";
    version_ = "";
    body_ = "";
    headers_.clear();
    post_.clear();
}

// 如果请求头包含connection:keep-alive，则返回true，否则返回false
inline bool HttpRequest::isKeepAlive() const
{
    return headers_.find("Connection") != headers_.end() &&
           headers_.find("Connection")->second == "keep-alive";
}

inline bool HttpRequest::parse(Buffer& buff)
{
    const char CRLF[] = "\r\n";
    if(buff.readableBytes()<=0){
        return false;
    }
    while(buff.readableBytes() && state_ != FINISH)
    {
        // search的参数都是const char*, 不能是 char*
        const char* lineEnd = std::search(buff.peek(), buff.beginWriteConst(), CRLF, CRLF+2);
        std::string line(buff.peek(), lineEnd);
        switch(state_)
        {
        case REQUEST_LINE:
            if(!parseRequestLine_(line))
            {
                return false;
            }
            parsePath_();
            break;
        case HEADERS:
            parseHeader_(line);
            // 如果匹配到了空行-》finish
            if(buff.readableBytes()<=2)
            {
                state_=FINISH;
            }
            break;
        case BODY:
            parseBody_(line);
            break;
        default:
            break;
        }
        if(lineEnd==buff.beginWrite())
        {
            break;
        }
        buff.retrieveUntil(lineEnd+2);
    }
    return true;
}

inline bool HttpRequest::parseRequestLine_(const std::string& line)
{
    // \w字母数字下划线
    // \S非空白字符
    std::regex pattern("^(\\w+) (\\S+) (\\S+)$");
    std::smatch submatch;
    if(std::regex_match(line, submatch, pattern))
    {
        method_ = submatch[1];
        path_ = submatch[2];
        version_ = submatch[3];
        state_ = HEADERS;
        return true;
    }
    return false;
}

// 将请求路径映射成 .html文件
// "/" -> "/index.html"
// "/index" -> "/index.html"
inline void HttpRequest::parsePath_()
{
    if(path_ == "/")
    {
        path_ = "/index.html";
    }
    else{
        for(auto &item: DEFAULT_HTML)
        {
            if(item==path_)
            {
                path_+=".html";
                break;
            }
        }
    }
}
inline void HttpRequest::parseHeader_(const std::string &line)
{   // 非：开头的行为请求头
    std::regex pattern("^([^:]+): (.+)$");
    std::smatch submatch;
    if(std::regex_match(line, submatch, pattern))
    {
        headers_[submatch[1]] = submatch[2];
    }
    else 
    {
        state_ = BODY;
    }
}

inline void HttpRequest::parseBody_(const std::string &line)
{
    body_ += line;
    parsePost_();
    state_ = FINISH;
}

// todo: 特殊ascii hex编码解码
inline int HttpRequest::converHex(char ch)
{   
    if(ch>='0' && ch<='9')
    {
        return ch-'0';
    }
    else if(ch>='a' && ch<='f')
    {
        return ch-'a'+10;
    }
    else if(ch>='A' && ch<='F')
    {
        return ch-'A'+10;
    }
    else
    {
        return -1;
    }
}

// 如果是post请求，那么根据path_的路径，检查是否是注册、登录、视频、图片等请求，相应地进行用户验证,再根据验证结果跳转到相应的页面
inline void HttpRequest::parsePost_()
{
    if(method_ == "POST")
    {
        if(headers_.count("Content-Type") && headers_["Content-Type"] == "application/x-www-form-urlencoded")
        {
            parseFromUrlencoded_();
            if(DEFAULT_HTML_TAG.count(path_))
            {
                int tag = DEFAULT_HTML_TAG.find(path_)->second;
                // register.html || login.html
               if(tag==0 || tag == 1)
               {
                    bool isLogin = (tag==1);
                    if(userVerify(post_["username"], post_["password"], isLogin))
                    {
                        path_ = "/welcome.html";
                    }
                    else {
                        path_ = "/error.html";
                    }

               }
            }
        }
    }
}

inline void HttpRequest::parseFromUrlencoded_()
{
    if(body_.empty())
    {
        return;
    }
    std::string key, value;
    int num = 0;
    int n = body_.size();
    int i=0, j=0;
    for(; i<n; i++)
    {
        char ch = body_[i];
        switch(ch)
        {
            case '=':
            key = body_.substr(j, i-j);
            j = i+1;
            break;

            case '+':
            body_[i] = ' ';
            break;

            case '%':
            num = converHex(body_[i+1])*16+converHex(body_[i+2]);
            body_[i+2] = num%10+'0';
            body_[i+1] = num/10+'0';
            i+=2;
            break;

            case '&':
            value = body_.substr(j, i-j);
            j = i+1;
            post_[key] = value;
            break;

            default:
            break;
        }
    }
    if(post_.count(key) == 0 && j<i)
    {
        value = body_.substr(j, i-j);
        post_[key] = value;
    }
    

}

// fixme
inline bool HttpRequest::userVerify(const std::string& name, const std::string& pwd, bool isLogin)
{
    bool loginned = false;
    if(name == "" || pwd == "")
    {
        return false;
    }
    MYSQL* sql;
    SqlConnRAII sqlconn(&sql, SqlConnPool::getInstance());
    assert(sql);

    // char cmd[256] = {0};
    std::string cmd = "select username, password from user where username='"+name+"'";
    int ret = mysql_query(sql, cmd.c_str());
    if(ret!=0)
    {
        std::cout << mysql_error(sql);
        return false;
    }
    MYSQL_RES* resPtr = mysql_store_result(sql);
    // unsigned int numFields = mysql_num_fields(resPtr);
    MYSQL_ROW row;
    while((row = mysql_fetch_row(resPtr))!=nullptr)
    {
        // 登录行为，验证密码是否正确
        if(isLogin)
        {
            if(pwd == row[1])
            {
                loginned = true;
            }
        }
        // 注册行为，用户名已被占用
        else
        {
            std::cout<<"username is already used"<<std::endl;
        }
        
    }
    // 注册行为，用户名可用
    if(!isLogin && !loginned)
    {
        cmd = "insert into user(username, password) values("+name+", "+pwd+")";
        ret = mysql_query(sql, cmd.c_str());
        if(ret!=0)
        {
            std::cout << "insert error" << std::endl;
        }
        else loginned = true;
    }
    std::cout << "loginned: " << loginned << std::endl;
    return loginned;
}

inline std::string HttpRequest::path() const
{
    return path_;
}

inline std::string& HttpRequest::path()
{
    return path_;
}

inline std::string HttpRequest::method() const
{
    return method_;
}

inline std::string HttpRequest::version() const
{
    return version_;
} 

inline std::string HttpRequest::getPost(const std::string& key) const
{
    assert(key!="");
    if(post_.count(key)){
        return post_.find(key)->second;
    }
    return "";
}

inline std::string HttpRequest::getPost(const char* key) const
{
    assert(key!= nullptr);
    if(post_.count(key))
    {
        // 在const函数内，不能使用pointers作为key
        return post_.find(key)->second; 
    }
    return "";
}
#endif