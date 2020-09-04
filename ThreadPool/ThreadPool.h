//
// Created by 郑卓铭 on 2020/9/3.
//

#ifndef WEBSERVERZZM_THREADPOOL_H
#define WEBSERVERZZM_THREADPOOL_H
//本文件用于实现线程池
#include <pthread.h>
#include <exception>
#include "../MessageQueue/MessageQueue.h"
//线程池并不需要是一个模板类，消息队列倒是可以是模板类
//这里还是要写成模板类，因为消息队列是模板类，ThreadPool要使用的话应该要用的模板类
namespace TinyWebServer {
    template<typename T>
    class ThreadPool {
    private:

        //线程池应该是用一个数组来保存初始化就创建好的线程
        pthread_t *threads;                 //线程池，用数组表示即可
        int thread_number;                  //用于表示线程的数量
        MessageQueue<T> meesage_queue;    //这个是主线程分发给IO线程的消息队列

        //因为线程回调函数必须是静态函数，所以设置线程的回调函数是静态函数
        //传递一个线程池对象，然后再其中去调用其成员函数即可
        static void *thread_cb(void *arg);
        void thread_cb_core();//真正调用的run函数

    public:

        ThreadPool(int thread_number = 4);
        ~ThreadPool();
    };

    template<typename T>
    ThreadPool::~ThreadPool() {
        //线程池的析构函数就是要消除线程数组
        delete[] threads;
    }

    template<typename T>
    ThreadPool::ThreadPool(int thread_number) :thread_number(thread_number){
        if(thread_number < 0)
            throw std::exception();
        threads = new pthread_t[thread_number];//构建一个堆数组
        if(!threads){
            //如果申请不到内存
            throw std::exception();
        }
        for(int i = 0;i < thread_number;i++){
            if(pthread_create(threads+i,NULL,worker,this) !=0 ){
                delete[] threads;
                throw std::exception();
            }
            if(pthread_detach(threads[i])){
                delete[] threads;
                throw std::exception();
            }
        }

    }

    //静态函数
    template<typename T>
    void* ThreadPool::thread_cb(void *arg) {
        ThreadPool* cur = (ThreadPool*)arg;
        cur->thread_cb_core();
        return cur;
    }
    //线程核心处理函数
    //主要任务是从消息队列中取出sockfd，然后注册事件，开始运行
    template<typename T>
    void ThreadPool::thread_cb_core() {
        T sockfd = meesage_queue.pop();//取出连接的fd，然后加入到自己的epoll循环中
    }


}



















#endif //WEBSERVERZZM_THREADPOOL_H
