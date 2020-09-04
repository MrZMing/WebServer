#include "./WebServer/WebServer.h"
using namespace TinyWebServer;
int main() {

    WebServer server;
    server.init(9090);

    server.eventListen();
    server.eventLoop();

    return 0;
}