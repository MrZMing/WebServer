//
// Created by 郑卓铭 on 2020/10/14.
//
#include "Log.h"

#if 0
        Block_Queue *q;
        char dir_name[128];//路径名
        char file_name[128];//文件名
        int max_split_lines;//最大行数
        int buffsize;//缓冲区的大小
        long long cur_lines;//当前日志的一个行数
        int today;//因为要按照行和天来区分日志，所以要缓存当天时间

        int fd;//打开文件描述符
        char* buff;//写入缓冲区
        bool is_async;//是否是异步
        bool is_close;//日志是否关闭
#endif


namespace TinyWebServer{
    //默认日志文件的保存家目录
    const char* log_root = "./LogFile/";

    Log::Log():cur_lines(0),fd(-1),fileNum(1),is_async(false),locker() {}
    Log::~Log() {
        //析构
        if(fd > 0)
            close(fd);
        delete buff;
    }
    //该方法只能由一个线程调用对instance进行初始化操作
    //todo:添加并发控制，使得多个线程同时调用只有一个真正执行初始化操作
    bool Log::init(const char *file_name, int log_buf_size, int split_lines, int max_queue_size) {
        //初始化过程，防止多个
        if(is_close == true){
            cout<<"日志功能关闭了还想初始化？"<<endl;
            return false;
        }
        if(max_queue_size >= 1){
            //说明要使用异步模式，创建队列
            is_async = true;//异步模式
            q = new Block_Queue(max_queue_size);//构建阻塞队列
            //还要创建异步日志线程
            pthread_t tid;
            pthread_create(&tid,NULL,flush_log_thread,NULL);
        }

        buffsize = log_buf_size;
        buff = new char[buffsize];
        memset(buff,'\0',sizeof(buff));
        max_split_lines = split_lines;


        //获取当前时间
        time_t curTime = time(NULL);
        //然后转化成可视化的tm的结构体形式
        //localtime可以转化成北京时区，而gmtime不进行时区转换
        struct tm curTime_tm = *(localtime(&curTime));
        //记录当前的时间
        today = curTime_tm.tm_mday;
        //从右往左找出第一个/
        const char *p = strrchr(file_name,'/');
        char log_full_name[256] = {0};

        if(p == NULL){
            //说明不存在子目录，那么就可以直接生成文件径了
            //格式是根目录/文件名_年_月_日_当前的第几个文件编号
            memset(mdir_name,'\0',sizeof(mdir_name));
            strcpy(mfile_name,file_name);
            snprintf(log_full_name,255,"%s%s_%d_%02d_%02d_%d",log_root,file_name,curTime_tm.tm_year + 1900,curTime_tm.tm_mon + 1,curTime_tm.tm_mday,fileNum);
        }else{
            //那就先解析出
            strcpy(mfile_name,p+1);//先把后面部分的文件名复制出来
            strncpy(mdir_name,file_name,p - file_name + 1);//从file_name开始到p之间复制给dir_name
            snprintf(log_full_name,255,"%s%s%s_%d_%02d_%02d_%d",log_root,mdir_name,mfile_name,curTime_tm.tm_year + 1900,curTime_tm.tm_mon + 1,curTime_tm.tm_mday,fileNum);

        }

        //打开文件
        //判断文件是否存在
//        if(access(path,F_OK) == 0){
//            cout<<"文件已经存在了"<<endl;
//            fd = open(path,O_CREAT | O_APPEND | O_WRONLY,"0777");
//        }else{
//            cout<<"文件不存在"<<endl;
//            fd = open(path,O_APPEND | O_WRONLY);
//        }
        //cout<<"初始化文件名为"<<log_full_name<<endl;
        if(access(log_full_name,F_OK) == 0){
            //说明文件存在
            //cout<<"文件存在"<<endl;
            fd = open(log_full_name,O_APPEND | O_WRONLY);
            //fd = open(path,O_APPEND | O_WRONLY);
        }else{
            //不存在
            //cout<<"文件不存在"<<endl;
            fd = open(log_full_name,O_CREAT | O_APPEND | O_WRONLY,0777);
        }
        if(fd < 0){
            LOG_ERROR("create log file failure,errno is %d when init",errno);
            //cout<<"创建日志文件失败"<<endl;
            //cout<<"错误码是:"<<errno<<endl;
            return false;
        }
        return true;
    }
    //为了严格实现每个文件都是n行的大小，在每次进行写入前进行判断是否需要先创建新的文件
    //不管同步写入还是异步写入都需要先行判断，写成一个子函数
    void Log::judge_create_newFile(struct tm curTime_tm){
        //分析是否需要重新创建新的日志文件
        char new_log_full_name[256] = {0};
        locker.lock();
        if(today != curTime_tm.tm_mday || cur_lines >= max_split_lines){
            //如果日志变更或者是当前的日志行数超过了最大分割日志行数则重新建立一个新文件
            //cout<<"开始创建新的日志文件"<<endl;
            if(today != curTime_tm.tm_mday){
                today = curTime_tm.tm_mday;
                fileNum = 1;
            }else{
                fileNum++;//当天的第n个
            }
            cur_lines = 0;//当前文件的行数归零
            //关闭原来的文件描述符
            if(fd > 0)
                close(fd);
            snprintf(new_log_full_name,255,"%s%s%s_%d_%02d_%02d_%d",log_root,mdir_name,mfile_name,curTime_tm.tm_year + 1900,curTime_tm.tm_mon + 1,curTime_tm.tm_mday,fileNum);
            //cout<<"新的日志文件名是"<<new_log_full_name<<endl;
            if(access(new_log_full_name,F_OK) == 0){
                //说明文件存在
                fd = open(new_log_full_name,O_WRONLY | O_APPEND);
            }else{
                //不存在,第三个参数权限值只在O_Creat的flag下才生效
                fd = open(new_log_full_name,O_CREAT | O_APPEND | O_WRONLY,0777);
            }
            //fd = open(new_log_full_name,O_CREAT | O_APPEND | O_RDWR,"0666");
            if(fd < 0){
                LOG_ERROR("create log file failure,errno is %d",errno);
                //cout<<"创建日志文件失败"<<endl;
                //cout<<"错误码是:"<<errno<<endl;
                return;
            }
        }
        locker.unlock();
    }


    //写日志函数，在异步时写入阻塞队列，同步时直接写入文件
    void Log::write_log(int level, const char *format, ...) {
        //首先获取当前时间，因为要获取的时间还有秒等，所以采用精度更高的gettimeofday
        struct timeval now = {0,0};
        gettimeofday(&now,NULL);
        //转化成time_t->tm
        struct tm curTime_tm = *(localtime(&(now.tv_sec)));
        //写入日志，日志格式 时间[等级标志] 日志
        char s[16] = {0};
        switch(level){
            case 0:
                strcpy(s,"[debug]:");
                break;
            case 1:
                strcpy(s,"[info]:");
                break;
            case 2:
                strcpy(s,"[warn]:");
                break;
            case 3:
                strcpy(s,"[erro]:");
                break;
            default:
                strcpy(s,"[info]:");
                break;
        }
        va_list valst;
        va_start(valst,format);//将valst指向参数的首地址

        string log_str;
        locker.lock();
        //格式： 时间[等级标志]
        int n = snprintf(buff,48,"%d-%02d-%02d %02d:%02d:%02d.%06ld %s",
                curTime_tm.tm_year + 1900,curTime_tm.tm_mon + 1,curTime_tm.tm_mday,
        curTime_tm.tm_hour,curTime_tm.tm_min,curTime_tm.tm_sec,now.tv_usec,s);
        //前面加v表示可用va_list的打印函数，效果和不加v的函数一样
        int m = vsnprintf(buff + n,buffsize - 1,format,valst);
        buff[n + m] = '\n';
        buff[n + m + 1] = '\0';
        log_str = buff;

        locker.unlock();

        //如果是异步的话就添加到阻塞队列
        if(is_async){
            q->push(log_str);
        }else{
            //同步写入
            //先行判断是否需要创建文件，需要就创建，等待写入前才进行判断，保证每个文件都是严格的n行
            judge_create_newFile(curTime_tm);
            locker.lock();
            //cout<<log_str.c_str()<<endl;
            //cout<<sizeof(log_str.c_str())<<endl;
            int ret = write(fd,log_str.c_str(),strlen(log_str.c_str()));
            cur_lines++;//当前的日志行数++,写入了才加1
            //cout<<"写入的字节数是"<<ret<<endl;
            locker.unlock();
        }

        va_end(valst);


    }

    void Log::test(){
        Log *log = Log::get_instance();
        //测试同步写入,队列大小为0，日志文件最大行数为2
//        log->init("a",0,8192,2,0);
//        for(int i = 0;i < 8;i++){
//            LOG_DEBUG("%d_%s",i,"DEBUG");
//            LOG_INFO("%d_%s",i,"INFO");
//            LOG_WARN("%d_%s",i,"WARN");
//            LOG_ERROR("%d_%s",i,"ERROR");
//        }
        //测试异步
//        if(!log->init("a",0,8192,4,4)){
//            cout<<"初始化日志文件失败"<<endl;
//            return;
//        };
//        for(int i = 0;i < 8;i++){
//            LOG_DEBUG("%d_%s",i,"测试DEBUG");
//            LOG_INFO("%d_%s",i,"测试INFO");
//            LOG_WARN("%d_%s",i,"测试WARN");
//            LOG_ERROR("%d_%s",i,"测试ERROR");
//        }
        //测试关闭日志
        log->init("",true,0,0);
        for(int i = 0;i < 8;i++){
            LOG_DEBUG("%d_%s",i,"测试DEBUG");
            LOG_INFO("%d_%s",i,"测试INFO");
            LOG_WARN("%d_%s",i,"测试WARN");
            LOG_ERROR("%d_%s",i,"测试ERROR");
        }

    }
}