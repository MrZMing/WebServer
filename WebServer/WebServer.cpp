//
// Created by 郑卓铭 on 2020/9/2.
//
#include "WebServer.h"
//#include "../Http/http_conn.h"



namespace TinyWebServer{

    WebServer::WebServer(int thread_number, int max_request_number,int timeout) {
        this->mthreadPool = new ThreadPool(thread_number,max_request_number,timeout);
    }
    WebServer::~WebServer() {}
    void WebServer::init(int port) {
        this->port = port;
        //http_conn::m_user_count = 0;
    }
    void WebServer::eventListen() {
        //监听
        listenfd = socket(PF_INET,SOCK_STREAM,0);
        assert(listenfd >= 0);

        //没有优雅关闭
        struct sockaddr_in address;
        bzero(&address,sizeof(address));
        address.sin_family = AF_INET;
        address.sin_addr.s_addr = htonl(INADDR_ANY);
        address.sin_port = htons(port);

        //设置取消time_wait状态，方便服务器快速重启
        int ret = 0;
        int flag = 1;
        setsockopt(listenfd,SOL_SOCKET,SO_REUSEADDR,&flag,sizeof(flag));
        //将socket绑定到对应的ip及端口上
        ret = bind(listenfd,(struct sockaddr*)&address,sizeof(address));
        assert(ret >= 0);
        //服务器开始监听端口
        //提供建议内部的队列长度和为5
        ret = listen(listenfd,5);
        assert(ret >= 0);


        //初始化epoll
        //5只是给一个建议而已，操作系统会自己去生成对应大小的红黑树
        epollfd = epoll_create(5);
        assert(epollfd != -1);
        addfd(listenfd,false);//这里为什么false
    }
    void WebServer::eventLoop() {
        //单独写成一个事件循环
        bool stop_server = false;
        while(!stop_server){
            int number = epoll_wait(epollfd,events,MAX_EVENT_NUMBER,-1);
            if(number < 0 && errno != EINTR){
                std::cout<<"error failure"<<std::endl;
                break;
            }
            for(int i = 0;i < number;i++){
                int sockfd = events[i].data.fd;
                //处理信道的客户连接
                if(sockfd == listenfd){
                    cout<<"有接收到新的连接"<<endl;
                    struct sockaddr_in client_address;
                    socklen_t client_addrlength = sizeof(client_address);
                    while(1){
                        int connfd = accept(listenfd,(struct sockaddr *)&client_address,&client_addrlength);
                        if(connfd < 0){
                            //出错
                            std::cout<<"errno is "<<errno<<std::endl;
                            break;
                        }
                        //分配给工作线程
                        //addfd(connfd,true);//注册可读事件
                        DiverseInfo message(connfd,client_address);
                        mthreadPool->append(message);
                    }
                }else{
                    cout<<"other"<<endl;
                }
            }
        }
    }
    //epoll
    int WebServer::setnonblocking(int fd) {
        int old_option = fcntl(fd,F_GETFL);
        int new_option = old_option | O_NONBLOCK;
        fcntl(fd,F_SETFL,new_option);
        return old_option;
    }

    void WebServer::addfd(int fd, bool one_shot) {
        epoll_event event;
        event.data.fd = fd;

        event.events = EPOLLIN | EPOLLET | EPOLLRDHUP;

        if(one_shot)
            event.events |= EPOLLONESHOT;
        epoll_ctl(epollfd,EPOLL_CTL_ADD,fd,&event);
        setnonblocking(fd);//ET模式一定要对文件描述符设置为非阻塞才行，否则会出错

    }

}

