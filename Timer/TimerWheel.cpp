//
// Created by 郑卓铭 on 2020/10/14.
//


#include "TimerWheel.h"

namespace TinyWebServer{
    TimerWheel::TimerWheel(int timeout):cur(0),timeout(timeout),wheelSize(timeout+1){
        //这个不能在初始化列表初始化，获取不到最新的wheelSize值，然后出现core dump
        wheel = vector<set<TimerInfo*>>(wheelSize,set<TimerInfo*>());
    }
    //构架一个时间轮大于超时时间1个slot
    TimerWheel::~TimerWheel() {}
    void TimerWheel::add_timer(TimerInfo* timer) {
        //cout<<cur<<" "<<timeout<<" "<<wheelSize<<endl;
        timer->timerSlot = (cur + timeout)%wheelSize;
        //cout<<timer->timerSlot<<endl;
        //cout<<wheel.size()<<endl;
        wheel[(cur + timeout)%wheelSize].insert(timer);
        //cout<<"往"<<cur+timeout<<"中添加了一个timer,现在有"<<wheel[(cur+timeout)%wheelSize].size()<<"个"<<endl;

    }
    void TimerWheel::delete_timer(TimerInfo* timer){
        //删除某一个定时器
        if(wheel[timer->timerSlot].count(timer))
            wheel[timer->timerSlot].erase(timer);
    }
    void TimerWheel::delete_curSlot() {
        //当前所在的时间轮就是到期的时间轮，删除其中的所有定时器
        set<TimerInfo*>& wl = wheel[cur];
//        for(set<TimerInfo>::iterator& iter = wl.begin();iter != wl.end();){
//            //todo：执行函数回调，关闭对应的连接及删除epoll事件
//            iter = wl.erase(iter);
//        }
        //cout<<"删除前"<<cur<<"有"<<wl.size()<<"个"<<endl;
        for(auto& iter : wl){
            //todo：执行函数回调，关闭对应的连接及删除epoll事件
            //关闭对应的连接
            iter->close_conn();
            wl.erase(iter);
        }
        //cout<<"删除后"<<cur<<"有"<<wl.size()<<"个"<<endl;
    }
    void TimerWheel::adjust_timer(TimerInfo* timer) {
        //当前连接在定时器到来前活跃的话可以对其进行调整
        //先删除，再调整
        delete_timer(timer);
        add_timer(timer);

    }
    void TimerWheel::tick() {
        //一次tick就是先处理当前的一个slot，然后cur++
        delete_curSlot();
        cur = (cur + 1)%wheelSize;
    }

    void TimerWheel::test(){
        //测试定时器
        //cout<<"1111"<<endl;

        struct sockaddr_in address;
        bzero(&address,sizeof(address));
        address.sin_family = AF_INET;
        address.sin_addr.s_addr = htonl(INADDR_ANY);
        address.sin_port = htons(9999);

        //TimerInfo* timer = new TimerInfo(9999,address);
//        TimerInfo* timer = new TimerInfo();
//        cout<<"9999"<<endl;
//        add_timer(timer);

//        for(int i = 0;i < 100;i++){
//            //先给当前的slot添加定时器
//            //cout<<"5555"<<endl;
//            for(int j = 0;j < i;j++){
//                TimerInfo* timer = new TimerInfo(j,address);
//                //cout<<"6666"<<endl;
//                add_timer(timer);
//                //cout<<"3333"<<endl;
//            }
//            //tick();
//            //cout<<"4444"<<endl;
//        }
        //测试update函数
        TimerInfo* timer = new TimerInfo(9999,address);
        add_timer(timer);
        for(int i = 0;i < 10;i++) {
            cout<<"第"<<i<<"次"<<endl;
            adjust_timer(timer);
            assert(wheel[cur].size() == 0);
            assert(wheel[(cur + timeout)%wheelSize].size() == 1);
            cur = (cur + 1)%wheelSize;
        }


        //cout<<"2222"<<endl;
    }

}