//
// Created by 郑卓铭 on 2020/9/3.
//

#ifndef WEBSERVERZZM_THREADPOOL_H
#define WEBSERVERZZM_THREADPOOL_H
//本文件用于实现线程池
#include <pthread.h>
#include <exception>
#include <vector>
#include <sys/eventfd.h>
#include <sys/epoll.h>
#include <stdlib.h>
#include <iostream>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/timerfd.h>
#include "../Lock/Locker.h"
#include "../Http/http_conn.h"
#include "../Timer/TimerWheel.h"
#include "../Utils/Utils.h"
#include "../Log/Log.h"

using std::cout;
using std::endl;
using std::vector;
//线程池并不需要是一个模板类，消息队列倒是可以是模板类
//这里还是要写成模板类，因为消息队列是模板类，ThreadPool要使用的话应该要用的模板类
namespace TinyWebServer {
    const int MAX_FD = 65536;           //最大文件描述符
    const int MAX_EVENT_NUMBER = 10000; //单个epoll的最大事件数
    const int TIMESLOT = 5;             //最下超时单位

    //主线程和工作线程之间传递信息的结构体
    struct DiverseInfo{
        int connfd;
        struct sockaddr_in client_address;
        DiverseInfo(int connfd,struct sockaddr_in client_address):connfd(connfd),client_address(client_address){}
    };

    static void *thread_cb(void *arg);

    class ThreadPool {
    private:
        //线程池应该是用一个数组来保存初始化就创建好的线程
        pthread_t *threads;                 //线程池，用数组表示即可
        int thread_number;                  //用于表示线程的数量
        vector<DiverseInfo> message_queue;          //用一个vector作为传输队列
        Locker locker;                      //用互斥锁控制并发访问消息队列
        int max_request;                    //最大的请求数

        //注意一个雷pthread需要维护一个主线程和子线程之间沟通的eventfd，因此在构建线程的过程中同时需要构建好eventfd
        int *eventfds;                      //用一个数组同线程池一样同步维护,或者将二者构建成一个结构体也可以
        int index;                          //维护一个下标用来round robin选取工作线程
        int timeout;                        //维护定时器超时时间
        //MessageQueue<T> meesage_queue;    //这个是主线程分发给IO线程的消息队列
        //因为线程回调函数必须是静态函数，所以设置线程的回调函数是静态函数
        //传递一个线程池对象，然后再其中去调用其成员函数即可
    public:
        //构造和析构
        ThreadPool(int thread_number = 4,int max_request = 10000,int timeout = 5);
        ~ThreadPool();
        //添加socket到队列中
        bool append(DiverseInfo);
        void handleWrite();
        void handleRead();
        void handleError();
        void handleWakeup();
        void wakeup(int wakeupFd);
        void thread_cb_core(int);//真正调用的run函数


    };

    //工作线程调用需要的数据构建一个结构体
    struct threadInfo{
        ThreadPool* threadPool;
        int eventfd;
        threadInfo(ThreadPool* threadPool,int eventfd):threadPool(threadPool),eventfd(eventfd){}
    };

}



















#endif //WEBSERVERZZM_THREADPOOL_H
