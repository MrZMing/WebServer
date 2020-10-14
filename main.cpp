#include <iostream>
#include "./WebServer/WebServer.h"
using namespace TinyWebServer;
using std::cout;
using std::endl;
int main() {

    WebServer server(4,10000,5);
    server.init(9090);
    server.eventListen();
    server.eventLoop();
    return 0;

}