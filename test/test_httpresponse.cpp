#include "../src/httpresponse.hpp"

using namespace std;
int main(){
    HttpResponse res;
    string path = "/index.html";
    Buffer buff;
    string src = "/home/aiyolo/toyserver/resources";
    res.init(src, path, true, -1);
    res.makeResponse(buff);
    cout << buff.retrieveAllAsString();
    return 0;
}