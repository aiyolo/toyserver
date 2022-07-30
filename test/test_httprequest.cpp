#include "../src/httprequest.hpp"
using namespace std;
int main()
{
    
    SqlConnPool::getInstance()->init("localhost", "root", "tobeNo.1", "mydb", 3306, 10);
    HttpRequest req;
    Buffer buff;
    req.init();
    string requestLine = "POST /login.html HTTP/1.1";
    // req.parseRequestLine_(requestLine);

    string requseHeader = "Host: www.baidu.com";
    string header2 = "Content-Type: application/x-www-form-urlencoded";
    string body = "username=root&password=root";
    req.parseHeader_(requseHeader);
    buff.append(requestLine+"\r\n");
    buff.append(requseHeader+"\r\n");
    buff.append(header2+"\r\n");
    buff.append("\r\n");
    buff.append(body);
    req.parse(buff);
    cout << req.method() << req.path() << req.version()<< endl;
    cout << req.headers_["Host"] << endl;
    return 0;
}