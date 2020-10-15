//
// Created by 郑卓铭 on 2020/10/14.
//

#ifndef WEBSERVERZZM_LOG_H
#define WEBSERVERZZM_LOG_H

#include <queue>
#include <time.h>
#include <sys/time.h>
#include <string.h>
#include <stdarg.h>
#include <string>
#include <iostream>
#include <fcntl.h>
#include <unistd.h>
#include <arpa/inet.h>
#include "../Lock/Locker.h"

using std::queue;
using std::string;
using std::cout;
using std::endl;


namespace TinyWebServer{
    //单例日志类
    class Block_Queue{
    private:
        queue<string> q;//阻塞队列，将消息放入其中
        int max_message_size;   //最大消息数量
        Locker locker;
        Sem sem;
    public:
        Block_Queue(int max_size):max_message_size(max_size),locker(),sem(){}
        //提供接口,需要实现阻塞等待
        bool push(string input){
            locker.lock();
            if(q.size() >= max_message_size){
                locker.unlock();
                return false;
            }
            q.push(input);
            locker.unlock();
            sem.v();//唤醒
            return true;
        }

        string pop(){
            string ret;
            sem.p();//先阻塞获取
            locker.lock();
//            if(q.empty()){
//                locker.unlock();
//                return "";
//            }
            ret = q.front();
            q.pop();
            locker.unlock();
            return ret;
        }
    };
    class Log{
        //C++11后局部静态变量不用加锁了，保证是线程安全的
    private:
        //维护一个阻塞队列
        Block_Queue *q;
        char mdir_name[128];//路径名
        char mfile_name[128];//文件名
        int max_split_lines;//最大行数
        int buffsize;//缓冲区的大小
        long long cur_lines;//当前日志的一个行数
        int today;//因为要按照行和天来区分日志，所以要缓存当天时间

        int fd;//打开文件描述符
        int fileNum;//当前文件数量的编号,从1开始
        char* buff;//写入缓冲区
        bool is_async;//是否是异步


        Locker locker;//互斥锁,为什么要互斥锁，因为本日志类是一个单例模式，对同一个文件进行写入
        //供多个线程调用，因此日志类相当于是一个共享对象，因此要用互斥锁同步控制并发


        Log();
        ~Log();
        void *async_write_log(){
            //异步线程的工作就是从阻塞队列中获取，然后写入文件中去
            string log_str;
            while(true){
                log_str = q->pop();
                struct timeval now = {0,0};
                gettimeofday(&now,NULL);
                //转化成time_t->tm
                struct tm curTime_tm = *(localtime(&(now.tv_sec)));
                judge_create_newFile(curTime_tm);
                locker.lock();
                cout<<log_str.c_str()<<endl;
                cout<<sizeof(log_str.c_str())<<endl;
                int ret = write(fd,log_str.c_str(),strlen(log_str.c_str()));
                cur_lines++;//写入的时候才加1
                cout<<"写入的字节数是"<<ret<<endl;
                locker.unlock();
            }

        }
    public:

        bool is_close;//日志是否关闭

        static Log* get_instance(){
            static Log instance;
            return &instance;
        }

        static void *flush_log_thread(void *args){
            Log::get_instance()->async_write_log();
        }

        bool init(const char *file_name,int log_buf_size = 8192,int split_lines = 50000000,int max_queue_size = 0);
        void write_log(int level,const char* format,...);
        void judge_create_newFile(struct tm curTime_tm);
        //void flush(void);
        void test();
    };


    //定义一组打印日志的宏
    //如果日志关闭则不打印
    //##__VA_ARGS__用于将可变参数转化成string
#define LOG_DEBUG(format,...) if(0 == Log::get_instance()->is_close) Log::get_instance()->write_log(0,format,##__VA_ARGS__)
#define LOG_INFO(format,...) if(0 == Log::get_instance()->is_close) Log::get_instance()->write_log(1,format,##__VA_ARGS__)
#define LOG_WARN(format,...) if(0 == Log::get_instance()->is_close) Log::get_instance()->write_log(2,format,##__VA_ARGS__)
#define LOG_ERROR(format,...) if(0 == Log::get_instance()->is_close) Log::get_instance()->write_log(3,format,##__VA_ARGS__)







}


#endif //WEBSERVERZZM_LOG_H
