//
// Created by 郑卓铭 on 2020/9/3.
//

#ifndef WEBSERVERZZM_MESSAGEQUEUE_H
#define WEBSERVERZZM_MESSAGEQUEUE_H
//本文件用于创建消息队列类
#include <queue>
#include "../Lock/Locker.h"
namespace TinyWebServer{
    template <typename T>
    class MessageQueue{
    private:
        //底层用什么实现好,就用队列吧
        queue<T> message_queue;
        //还需要保证线程同步的信号量和互斥锁
        Locker locker;//维护队列安全的互斥锁
        Sem sem;      //维护线程同步的信号量
    public:
        MessageQueue();
        ~MessageQueue();
        T pop();
        void push(T message);
    };
    template <typename T>
    T MessageQueue::pop() {
        T ret;
        sem.p();
        locker.lock();
        ret = message_queue.front();
        message_queue.pop();
        locker.unlock();
        return ret;
    }
    template <typename T>
    void MessageQueue::push(T message) {
        locker.lock();
        message_queue.push(message);
        locker.unlock();
        sem.v();

    }
}
















#endif //WEBSERVERZZM_MESSAGEQUEUE_H
