#include <iostream>
#include "./WebServer/WebServer.h"
using namespace TinyWebServer;
using std::cout;
using std::endl;
int main() {

    WebServer server(4,10000);
    server.init(9090);
    server.eventListen();
    server.eventLoop();
    //cout<<"1111111111"<<endl;
//
//    TimerWheel t(5);
//    t.test();
    return 0;
}