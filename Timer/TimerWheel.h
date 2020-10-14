//
// Created by 郑卓铭 on 2020/10/14.
//
#ifndef WEBSERVERZZM_TIMERWHEEL_H
#define WEBSERVERZZM_TIMERWHEEL_H

#include <iostream>
#include <vector>
#include <set>
#include <assert.h>
#include "../Http/http_conn.h"

using std::cout;
using std::endl;
using std::vector;
using std::set;

namespace TinyWebServer {

//时间轮定时器

    //在TImerInfo存放http_conn对象，封装http_conn
    struct TimerInfo {

        int connfd;                         //连接的fd
        struct sockaddr_in address;         //客户端地址
        int timerSlot;                      //对应的timerSlot,该字段在添加到时间轮的时候确定
        http_conn* conn;                    //http_conn对象
        //void (*cb_func)(TimerInfo*);  //定时器到期的回调函数，在其中关闭连接以及删除epoll上注册的事件
        //直接调用conn对象的closeconn即可


        TimerInfo(){}
        TimerInfo(int connfd,struct sockaddr_in address) : connfd(connfd),address(address){}
        //初始化函数
        void init(int connfd,struct sockaddr_in address,http_conn* conn){
            this->connfd = connfd;
            this->address = address;
            this->conn = conn;
        }

        //封装一下http_conn的读写方法
        bool read(){
            return conn->read();
        }
        bool write(){
            return conn->write();
        }
        void process(){
            conn->process();
        }
        void close_conn(bool real_close = true){
            conn->close_conn(real_close);
        }
    };

    class TimerWheel {
    private:
        vector<set<TimerInfo*>> wheel;                  //主时间轮
        int wheelSize;                                  //时间轮的大小与定义的超时时间有关系
        int timeout;
        int cur;                                        //当前处在时间轮的哪个slot上
    public:
        TimerWheel(int timeout = 5);                    //默认构建5s的时间轮
        ~TimerWheel();
        void add_timer(TimerInfo* timer);               //添加定时器
        void delete_curSlot();                          //删除定时器
        void delete_timer(TimerInfo* timer);            //删除指定的定时器
        void adjust_timer(TimerInfo* timer);            //调整定时器
        void tick();                                    //一秒滴答
        void test();
    };


}
#endif //WEBSERVERZZM_TIMERWHEEL_H
