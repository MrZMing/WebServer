#include "./WebServer/WebServer.h"
using namespace TinyWebServer;
int main() {

    WebServer server(4,10000,5);
    //开启日志功能
    //server.openLogFunc("a");//传入路径，其他默认
    server.init(9090);
    server.eventListen();
    server.eventLoop();
    return 0;

}