//
// Created by 郑卓铭 on 2020/9/2.
//

#ifndef WEBSERVERZZM_WEBSERVER_H
#define WEBSERVERZZM_WEBSERVER_H

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <cassert>
#include <sys/epoll.h>
#include <iostream>
#include <string.h>
#include "../ThreadPool/ThreadPool.h"

using std::cout;
using std::endl;

namespace TinyWebServer {

    //定义常量
    //const int MAX_FD = 65536;
    //const int MAX_EVENT_NUMBER = 10000;
    //主线程和子线程传递的结构体


    class WebServer {
    private:
        std::string ip;
        int port;
        int listenfd;
        int epollfd;
        epoll_event events[MAX_EVENT_NUMBER];

        //线程池对象
        ThreadPool* mthreadPool;


    public:
        WebServer(int thread_number,int max_request_number,int timeout = 5);
        ~WebServer();
        void init(int port);
        void eventListen();
        void eventLoop();

        //epoll专用

        //向epollfd注册读时间，ET模式，选择开启EPOLL_ONESHOT模式
        void addfd(int fd,bool one_shot);
        //对文件描述符设置为非阻塞
        int setnonblocking(int fd);

    };

}


#endif //WEBSERVERZZM_WEBSERVER_H
