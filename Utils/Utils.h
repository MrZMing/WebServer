//
// Created by 郑卓铭 on 2020/10/14.
//

#ifndef WEBSERVERZZM_UTIL_H
#define WEBSERVERZZM_UTIL_H

#include <fcntl.h>
#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <sys/timerfd.h>

namespace TinyWebServer{
    //工具类，主要用来封装一些fd操作
    class Utils{
    public:
        static int setnonblocking(int fd){
            int old_option = fcntl(fd,F_GETFL);
            int new_option = old_option | O_NONBLOCK;
            fcntl(fd,F_SETFL,new_option);
            return old_option;
        }
        static void addfd(int epollfd,int fd,bool one_shot,bool et_mode){
            epoll_event event;
            event.data.fd = fd;
            if(et_mode){
                event.events = EPOLLIN | EPOLLET | EPOLLRDHUP;
            }else{
                event.events = EPOLLIN | EPOLLRDHUP;
            }
            if(one_shot)
                event.events |= EPOLLONESHOT;
            epoll_ctl(epollfd,EPOLL_CTL_ADD,fd,&event);
            setnonblocking(fd);
        }
        static int create_timerfd(const int& timeout){
            int timerfd = timerfd_create(CLOCK_MONOTONIC,TFD_NONBLOCK | TFD_CLOEXEC);
            struct itimerspec timespend;
            timespend.it_interval.tv_sec = timeout;
            timespend.it_interval.tv_nsec = 0;      //间隔时间
            timespend.it_value.tv_sec = timeout;
            timespend.it_value.tv_nsec = 0;         //开始时间
            timerfd_settime(timerfd,0,&timespend,0);//设置超时时间
            return timerfd;
        }
        static void removefd(int epollfd, int fd) {
            epoll_ctl(epollfd, EPOLL_CTL_DEL, fd, 0);
            close(fd);
        }
        static void modfd(int epollfd, int fd, int ev) {
            epoll_event event;
            event.data.fd = fd;
            event.events = ev | EPOLLET | EPOLLONESHOT | EPOLLRDHUP;
            epoll_ctl(epollfd, EPOLL_CTL_MOD, fd, &event);
        }
        static int createEventfd(){
            //构建一个雷eventfd
            int evfd = eventfd(0,EFD_NONBLOCK | EFD_CLOEXEC);
            if(evfd < 0){
                cout<<"创建eventfd失败"<<endl;
                abort();
            }
            return evfd;
        }
    };

}


#endif //WEBSERVERZZM_UTIL_H
