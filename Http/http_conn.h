//
// Created by Administrator on 2020/5/17.
//

#ifndef WEBSERVERZZM_HTTP_CONN_H
#define WEBSERVERZZM_HTTP_CONN_H
#include <netinet/in.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/epoll.h>
#include <unistd.h>
#include <sys/stat.h>
#include <errno.h>
#include <sys/mman.h>
#include <cstring>
#include <cstdlib>
#include <cstdio>
#include <cstdarg>
#include <iostream>

using std::cout;
using std::endl;

namespace TinyWebServer {
    class http_conn {
    public:
        //文件名的最大长度
        static const int FILENAME_LEN = 200;
        //读缓冲区的大小
        static const int READ_BUFFER_SIZE = 2048;
        //写缓冲区的大小
        static const int WRITE_BUFFER_SIZE = 1024;
        //HTTP请求方法，仅支持GET
        enum METHOD {
            GET = 0,
            POST, HEAD, PUT, DELETE, TRACE, OPTIONS, CONNECT, PATCH
        };
        //解析客户请求时，主状态机所处的状态
        //根据HTTP请求报文分为请求行、请求头、请求内容
        enum CHECK_STATE {
            CHECK_STATE_REQUESTLINE = 0,
            CHECK_STATE_HEADER,
            CHECK_STATE_CONTENT
        };
        //从状态机的三种可能状态，即行的读取状态
        //读取一个完整的行、行出错和行数据尚且不完整
        enum LINE_STATUS {
            LINE_OK = 0,
            LINE_BAD,
            LINE_OPEN
        };
        //服务器处理HTTP请求的可能结果
        enum HTTP_CODE {
            NO_REQUEST,//表示请求不完整，需要继续读取客户数据
            GET_REQUEST,//表示获得了一个完整的客户请求
            BAD_REQUEST,//表示客户请求有语法错误
            NO_RESOURCE,//
            FORBIDDEN_REQUEST,//表示客户对资源没有足够的访问权限
            FILE_REQUEST,//
            INTERNAL_ERROR,//表示服务器内部错误
            CLOSED_CONNECTION//表示客户端已经关闭连接了
        };
    public:
        //所有socket上的事件都被注册到同一个epoll内核事件表上。
        //所以将epoll文件描述符设置为静态的
        int m_epollfd;
        //统计用户数量
        static int m_user_count;
    private:
        //该HTTP连接的socket和对方的socket地址
        int m_sockfd;
        sockaddr_in m_address;

        CHECK_STATE m_check_state;
        METHOD m_method;

        //读缓冲区
        char m_read_buf[READ_BUFFER_SIZE];
        //标识度缓冲中已经读如的客户数据的最后一个字节的下一个位置
        int m_read_idx;
        //当前正在分析的字符在读缓冲区中的位置
        int m_checked_idx;
        //当前正在解析的行的起始位置
        int m_start_line;
        //写缓冲区
        char m_write_buf[WRITE_BUFFER_SIZE];
        //写缓冲区中待发送的字节数
        int m_write_idx;

        //POST解析所需要的参数
        int cgi;        //是否启动的POST
        char *m_string;  //保存请求数据


        //客户请求的目标文件的完整路径，其内容等于doc_root + m_url,doc 是网站根目录
        char m_real_file[FILENAME_LEN];
        //客户请求的目标文件的文件名
        char *m_url;
        //HTTP协议版本号，仅支持HTTP/1.1
        char *m_version;
        //主机名
        char *m_host;
        //HTTP请求的消息体的长度
        int m_content_length;
        //HTTP请求是否要求保持连接
        bool m_linger;

        //客户请求的目标文件被mmap到内存中的起始位置
        char *m_file_address;
        //目标文件的状态，通过它我们可以判断文件是否存在、是否为目录、是否可读、并获取文件大小等
        struct stat m_file_stat;
        //我们将用writev来执行写操作，所以定义下面两个成员
        //m_iv_count表示被写内存块的数量
        struct iovec m_iv[2];
        int m_iv_count;
//写回去保存写入的指针，写入也可能没有写完
        int bytes_to_send;
        int bytes_have_send;

        int m_TRIGMode;
    public:
        http_conn() {};

        ~http_conn() {};
    public:
        //初始化新接受的连接
        void init(int epollfd,int sockfd, const sockaddr_in &addr, int);

        //关闭连接
        void close_conn(bool real_close = true);

        //处理客户请求
        void process();

        //非阻塞读操作
        bool read();

        //非阻塞写操作
        bool write();

    private:
        //初始化连接
        void init();

        //解析HTTP请求
        HTTP_CODE process_read();

        //填充HTTP应答
        bool process_write(HTTP_CODE ret);

        //下面这一组函数被process_read调用以解析HTTP请求
        HTTP_CODE parse_request_line(char *text);

        HTTP_CODE parse_headers(char *text);

        HTTP_CODE parse_content(char *text);

        HTTP_CODE do_request();

        char *get_line() {
            return m_read_buf + m_start_line;
        }

        LINE_STATUS parse_line();

        //下面这一组函数被process_write调用以填充HTTP应答
        void unmap();

        bool add_response(const char *format, ...);

        bool add_content(const char *content);

        bool add_status_line(int status, const char *title);

        bool add_headers(int content_length);

        bool add_content_length(int content_length);

        bool add_linger();

        bool add_blank_line();

    };

//int setnonblocking(int fd);
//void addfd(int epollfd,int fd,bool one_shot = true);
//void removefd(int epollfd,int fd);
}
#endif //MYTINYWEBSERVERNEWFORMYSELF_HTTP_CONN_H
