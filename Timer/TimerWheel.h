//
// Created by 郑卓铭 on 2020/10/14.
//

#ifndef WEBSERVERZZM_TIMERWHEEL_H
#define WEBSERVERZZM_TIMERWHEEL_H

#include <iostream>
#include <queue>
#include <list>
#include <vector>
#include <set>
#include <assert.h>
#include "../Http/http_conn.h"

using std::cout;
using std::endl;
using std::queue;
using std::list;
using std::vector;
using std::set;

namespace TinyWebServer {

//时间轮定时器
    struct TimerInfo {
        TimerInfo(){}
        TimerInfo(int connfd,struct sockaddr_in address) : connfd(connfd),address(address){}

        int connfd;         //连接的fd
        struct sockaddr_in address;//客户端地址
        int timerSlot;      //对应的timerSlot
        void (*cb_func)();  //定时器到期的回调函数，在其中关闭连接以及删除epoll上注册的事件
    };

    class TimerWheel {
    private:
        vector<set<TimerInfo*>> wheel; //主时间轮
        int wheelSize;                  //时间轮的大小与定义的超时时间有关系
        int timeout;
        int cur;                        //当前处在时间轮的哪个slot上
    public:
        TimerWheel(int timeout = 5);    //默认构建5s的时间轮
        ~TimerWheel();
        void add_timer(TimerInfo* timer);               //添加定时器
        void delete_curSlot();          //删除定时器
        void delete_timer(TimerInfo* timer);
        void adjust_timer(TimerInfo* timer);            //调整定时器
        void tick();                    //一秒滴答
        void test();
    };




}
#endif //WEBSERVERZZM_TIMERWHEEL_H
