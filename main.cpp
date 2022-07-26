#include "./src/webserver.hpp"


int main()
{
    Webserver* server = new Webserver(0);
    server->start();
    return 0;
}