//
// Created by 郑卓铭 on 2020/10/13.
//
#include "ThreadPool.h"

namespace TinyWebServer{
    //不要放在.h文件中，否则可能会造成重定义的问题
    int createEventfd(){
        //构建一个雷eventfd
        int evfd = eventfd(0,EFD_NONBLOCK | EFD_CLOEXEC);
        if(evfd < 0){
            cout<<"创建eventfd失败"<<endl;
            abort();
        }
        return evfd;
    }

    ThreadPool::~ThreadPool() {
        //线程池的析构函数就是要消除线程数组
        delete[] threads;
        delete[] eventfds;//析构eventfds
    }

    ThreadPool::ThreadPool(int thread_number,int max_request) :thread_number(thread_number),max_request(max_request){
        if(thread_number < 0)
            throw std::exception();
        threads = new pthread_t[thread_number];//构建一个堆数组
        eventfds = new int[thread_number];
        index = 0;
        if(!threads || !eventfds){
            //如果申请不到内存
            throw std::exception();
        }
        for(int i = 0;i < thread_number;i++){
            //构建eventfd
            eventfds[i] = createEventfd();
            //构建结构体
            threadInfo* info = new threadInfo(this,eventfds[i]);
            if(pthread_create(threads+i,NULL,thread_cb,info) !=0 ){
                delete[] threads;
                throw std::exception();
            }
            if(pthread_detach(threads[i])){
                delete[] threads;
                throw std::exception();
            }
        }

    }
    bool ThreadPool::append(DiverseInfo info){
        //添加到消息队列中,对待消息队列要加锁
        locker.lock();
        if(message_queue.size() >= max_request){
            locker.unlock();
            return false;
        }
        message_queue.push_back(info);
        locker.unlock();
        //然后唤醒
        //To Do添加唤醒工作线程的代码
        //....
        //选择工作线程，唤醒，
        index = (index + 1)%thread_number;
        //唤醒
        wakeup(eventfds[index]);
        return true;
    }
    //唤醒指定的工作线程
    void ThreadPool::wakeup(int wakeupFd){
        uint64_t one = 1;//写入一个64位的值
        ssize_t n = write(wakeupFd,(char*)(&one),sizeof one);
        if(n != sizeof one){
            cout<<"wakeup 失败，写入了 "<<n<<"个字节，而不是8个字节"<<endl;
        }
    }



    //静态函数
    void* thread_cb(void *arg) {
//        ThreadPool* cur = (ThreadPool*)arg;
//        cur->thread_cb_core();
//        return cur;
        threadInfo* cur = (threadInfo*)arg;
        cur->threadPool->thread_cb_core(cur->eventfd);
        return cur;
    }
    //线程核心处理函数
    //主要任务是从消息队列中取出sockfd，然后注册事件，开始运行
    //工作线程中每一个都维护一个epoll反应堆，负责自己的socket的整个生命过程
    //主线程通过round robin唤醒工作线程，让其继续监听一组新的socket
    void ThreadPool::thread_cb_core(int eventfd) {
        //线程的核心工作
        //构建自己的反应堆，监听自己的socket,处理

        bool quit = false;
        epoll_event events[MAX_EVENT_NUMBER];
        //对于每一个反应堆，需要缓存每一个connect
        http_conn* conn_set = new http_conn[MAX_FD];


        //构建定时器,通过timerfd_create实现定时功能，在epoll关注该timerfd的读事件实现统一事件源
        //NONBLOCK非阻塞，CLOEXEC表示fork不继承该fd
        //MONOTINIC表示从创建的时候开始计时，不受系统时间影响
        int timerfd = timerfd_create(CLOCK_MONOTONIC,TFD_NONBLOCK | TFD_CLOEXEC);
        struct itimerspec timespend;
        timespend.it_interval.tv_sec = 5;
        timespend.it_interval.tv_nsec = 0;      //间隔时间
        timespend.it_value.tv_sec = 5;
        timespend.it_value.tv_nsec = 0;         //开始时间
        timerfd_settime(timerfd,0,&timespend,0);//设置超时时间
        //timerfd_gettime();//获取距离下次超时剩余的时间


        int epollfd = epoll_create(6);
        struct epoll_event event;
        event.events = EPOLLIN | EPOLLET;//给weakup注册读时间和ET事件
        event.data.fd = eventfd;
        epoll_ctl(epollfd,EPOLL_CTL_ADD,eventfd,&event);


        //注册timerfd的读事件
        event.data.fd = timerfd;
        epoll_ctl(epollfd,EPOLL_CTL_ADD,timerfd,&event);

        //记录一个第一次的时间，测试定时器是否能够使用
        struct timespec start,end;
        int count = 0;


        while(!quit){
            //循环处理epoll
            int size = epoll_wait(epollfd,events,MAX_EVENT_NUMBER,-1);
            if(size < 0 && errno != EINTR){
                cout<<"epoll failure"<<endl;
                break;
            }
            if(TinyWebServer::http_conn::m_user_count >= MAX_FD){
                //那么此时反应堆已经无法再接收新的连接了
                cout<<"当前已经有MAX_FD个连接了，无法接收新的请求"<<endl;
                continue;
            }
            for(int i = 0;i < size;i++){

                int sockfd = events[i].data.fd;
                //处理错误事件
                if(events[i].events & (EPOLLRDHUP | EPOLLHUP | EPOLLERR)){
                    //发生错误
                    //handleError();
                    cout<<"发生错误"<<endl;
                }
                    //处理wakeup事件
                else if((sockfd == eventfd) && (events[i].events & EPOLLIN)){
                    vector<DiverseInfo> tmpMessageQueue;
                    //加锁获取
                    locker.lock();
                    std::swap(tmpMessageQueue,message_queue);
                    locker.unlock();
                    if(tmpMessageQueue.size()){
                        cout<<"当前处在wkeupfd为 "<<eventfd<<"的工作线程中"<<endl;
                        cout<<"本次获取到了这么多个新连接 "<<tmpMessageQueue.size()<<endl;
                        //todo:这里要为传递进来的fd建立一个connect对象，后续的处理直接调用connect中对应的处理函数
                        for(DiverseInfo &message : tmpMessageQueue){
                            conn_set[message.connfd].init(epollfd,message.connfd,message.client_address,1);
                        }
                    }
                    //然后读取出该字节,重新注册
                    uint64_t one = 1;
                    ssize_t n = read(eventfd,&one,sizeof one);
                    if(n != sizeof one){
                        cout<<"读取不足8字节，读取了"<<n<<endl;
                    }
                    //重新注册读事件
                    struct epoll_event event;
                    event.events = EPOLLIN | EPOLLET;//给weakup注册读时间和ET事件
                    event.data.fd = eventfd;
                    epoll_ctl(epollfd,EPOLL_CTL_ADD,eventfd,&event);
                    //handleWakeup();
                }
                    //处理时钟信号
                else if((sockfd == timerfd) && (events[i].events & EPOLLIN)){
                    //表示检测到了时钟时间了
                    if(clock_gettime(CLOCK_MONOTONIC,&end) == -1){
                        cout<<"获取时间失败"<<endl;
                        continue;
                    }
//                    cout<<"当前处在wkeupfd为 "<<eventfd<<"的工作线程中"<<"   ";
//                    cout<<"检测到了时钟事件了,当前是第"<<(count++)<<"    ";
//                    cout<<"时间是:"<<end.tv_sec - start.tv_sec<<endl;
                    start = end;

                    //把timerfd的值读取出来
                    uint64_t one;
                    ssize_t one_size = read(timerfd,&one,sizeof one);
                    if(one_size != sizeof(uint64_t)){
                        cout<<"读取的时值不是8字节的，而是"<<one_size<<endl;
                    }

                }
                    //处理可读事件
                else if(events[i].events & EPOLLIN){
                    //handleRead();
                    cout<<"epollin111111111111111111"<<endl;
                    if(conn_set[sockfd].read()){
                        cout<<"读取成功"<<endl;
                        conn_set[sockfd].process();
                    }else{
                        cout<<"读取失败"<<endl;
                    }
                }
                    //处理可写事件
                else if(events[i].events & EPOLLOUT){
                    //handleWrite();
                    cout<<"epollout22222222222222222222"<<endl;
                    if(conn_set[sockfd].write()){
                        //conn_set[sockfd].process();
                        cout<<"写成功"<<endl;
                    }else{
                        cout<<"写失败"<<endl;
                        //说明是短连接，要断开
                        epoll_ctl(epollfd,EPOLL_CTL_DEL,sockfd,0);
                        close(sockfd);
                    }
                }


            }


        }
    }


}