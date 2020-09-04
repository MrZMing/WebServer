//
// Created by 郑卓铭 on 2020/9/3.
//

#ifndef WEBSERVERZZM_LOCKER_H
#define WEBSERVERZZM_LOCKER_H
//该文件用于使用RAII技术封装 互斥锁，信号量及条件变量
#include <exception>
#include <pthread.h>
#include <semaphore.h>
//直接在.h文件中实现的函数默认内联
namespace TinyWebServer{
    //封装信号量的类
    class Sem{
    private:
        sem_t sem;
    public:
        //在构造函数中创建信号量，在析构函数中销毁
        sem(){
            if(sem_init(&sem_t,0,0) != 0){
                //如果构建失败要抛出异常
                throw std::exception();
            }
        }
        sem(){
            if(sem_init(&sem_t,0,num) != 0){
                //如果构建失败要抛出异常
                throw std::exception();
            }
        }
        ~sem(){
            sem_destroy(&sem);
        }
        //信号量的PV操作,成功都是返回的0
        bool p(){
            return sem_wait(&sem) == 0;
        }
        bool v(){
            return sem_post(&sem) == 0;
        }
    };

    //对互斥锁的RAII封装
    class Locker{
    private:
        pthread_mutex_t mutex;
    public:
        locker(){
            if(pthread_mutex_init(&mutex,NULL) != 0){
                throw std::exception();
            }
        }
        ~locker(){
            pthread_mutex_destroy(&mutex);
        }
        bool lock(){
            return pthread_mutex_lock(&mutex) == 0;
        }
        bool unlock(){
            return pthread_mutex_unlock(&mutex) == 0;
        }
        pthread_mutex_t* get(){
            return &mutex;
        }
    };

    //对条件变量的封装
    class Cond{
    private:
        pthread_cond_t cond;
        pthread_mutex_t mutex;
    public:
        cond(){
            if(pthread_cond_init(&cond) != 0){
                throw std::exception();
            }
            if(pthread_mutex_init(&mutex) != 0){
                throw std::exception();
            }
        }
        ~cond(){
            pthread_mutex_destroy(&mutex);
            pthread_cond_destroy(&cond);
        }
        //等待条件变量
        //条件变量的使用需要配合互斥锁
        bool wait(){
            int ret = 0;
            pthread_mutex_lock(&mutex);
            ret = pthread_cond_wait(&cond,&mutex);
            pthread_mutex_unlock(&mutex);
            return ret == 0;
        }
        //唤醒等待该条件变量的某个线程
        bool signal(){
            return pthread_cond_signal(&cond) == 0;
        }
    };
}











#endif //WEBSERVERZZM_LOCKER_H
