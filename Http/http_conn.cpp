//
// Created by Administrator on 2020/5/17.
//

#include "http_conn.h"
//#include "../Log/log.h"


#define  DEBUG
namespace TinyWebServer {
/*定义HTTP响应的一些状态信息*/
    const char *ok_200_title = "OK";
    const char *error_400_title = "Bad Request";
    const char *error_400_form = "Your Request has bad syntax or is inherently impossible to statisfy.\n";
    const char *error_403_title = "Forbidden";
    const char *error_403_form = "You do not have permission to get file from this server.\n";
    const char *error_404_title = "Not Found";
    const char *error_404_form = "The requested file was not found on this server.\n";
    const char *error_500_title = "Internal Error";
    const char *error_500_form = "There was an unusual probel serbing the requested file.\n";
//网站的根目录
    const char *doc_root = "./root";

//设置非阻塞
//    int setnonblocking(int fd) {
//        int old_option = fcntl(fd, F_GETFL);
//        int new_option = old_option | O_NONBLOCK;
//        fcntl(fd, F_SETFL, new_option);
//        return old_option;
//    }
//
//    void addfd(int epollfd, int fd, bool one_shot, int TRIGMode) {
//        epoll_event event;
//        event.data.fd = fd;
//        //EPOLLRDHUP表示如果对方关闭socket我们可以直接检测到这个属性
//        //不设置这个就需要自己read到0表示关闭了
//        if (1 == TRIGMode)
//            event.events = EPOLLIN | EPOLLET | EPOLLRDHUP;
//        else
//            event.events = EPOLLIN | EPOLLRDHUP;
//        if (one_shot)
//            //发生一次事件通知后，就不再接收通知，需要重新设置
//            event.events |= EPOLLONESHOT;
//        epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);
//        setnonblocking(fd);
//    }
//
//    void removefd(int epollfd, int fd) {
//        epoll_ctl(epollfd, EPOLL_CTL_DEL, fd, 0);
//        close(fd);
//    }
//
//    void modfd(int epollfd, int fd, int ev) {
//        epoll_event event;
//        event.data.fd = fd;
//        event.events = ev | EPOLLET | EPOLLONESHOT | EPOLLRDHUP;
//        epoll_ctl(epollfd, EPOLL_CTL_MOD, fd, &event);
//    }

    int http_conn::m_user_count = 0;
    //int http_conn::m_epollfd = -1;


    http_conn::http_conn(int epollfd,int sockfd, const sockaddr_in &addr, int TRIGMode){
        init(epollfd,sockfd,addr,TRIGMode);
    }

    void http_conn::close_conn(bool real_close) {
        if (real_close && (m_sockfd != -1)) {
            //关闭当前的连接

            cout<<"关闭端口在"<<m_address.sin_port<<"的连接"<<endl;
            Utils::removefd(m_epollfd, m_sockfd);
            m_sockfd = -1;
            m_user_count--;//关闭一个连接时，将客户总量减1
        }
    }

    void http_conn::init(int epollfd,int sockfd, const sockaddr_in &addr, int et_mode) {
        m_epollfd = epollfd;
        m_sockfd = sockfd;
        m_address = addr;
        //如下两行是为了避免time_wait状态，仅用于测试，实际使用时应去掉
        int reuse = 1;
        //设置避免timewait状态
        setsockopt(m_sockfd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
        Utils::addfd(m_epollfd, sockfd, true, et_mode);
        m_user_count++;
        m_et_mode = et_mode;
        init();
    }

    void http_conn::init() {
        m_check_state = CHECK_STATE_REQUESTLINE;//初始化为读取状态行，这是主状态机的状态
        m_linger = false;

        bytes_to_send = 0;
        bytes_have_send = 0;


        m_method = GET;
        m_url = 0;
        m_version = 0;
        m_content_length = 0;
        m_host = 0;
        m_start_line = 0;
        m_checked_idx = 0;
        m_read_idx = 0;
        m_write_idx = 0;

        memset(m_read_buf, '\0', READ_BUFFER_SIZE);
        memset(m_write_buf, '\0', WRITE_BUFFER_SIZE);
        memset(m_real_file, '\0', FILENAME_LEN);
    }

//从状态机
    http_conn::LINE_STATUS http_conn::parse_line() {
        char temp;
#ifdef DEBUG
        printf("读取行开始\n");
#endif
        for (; m_checked_idx < m_read_idx; ++m_checked_idx) {
            temp = m_read_buf[m_checked_idx];
#ifdef DEBUG
            std::cout<<"当前读取到的字符是:"<<temp<<std::endl;
#endif
            if (temp == '\r') {
                if ((m_checked_idx + 1) == m_read_idx) {
                    //表示当前行尚未读取完毕，仍需继续读取
                    return LINE_OPEN;
                } else if (m_read_buf[m_checked_idx + 1] == '\n') {
                    //说明读取到一个空行，所以读取行完成
                    m_read_buf[m_checked_idx++] = '\0';
                    m_read_buf[m_checked_idx++] = '\0';
#ifdef DEBUG
                    printf("当前提取的一行是:%s\n",m_read_buf);
#endif
                    return LINE_OK;
                }
                return LINE_BAD;
            } else if (temp == '\n') {
                //这种情况是因为上次读取到了\r然后就结束了
                //那么这次读取到\n就继续判断前一个
                if (m_checked_idx > 1 && m_read_buf[m_checked_idx - 1] == '\r') {
                    //这时候也说明读取到了行
                    m_read_buf[m_checked_idx - 1] = '\0';
                    m_read_buf[m_checked_idx++] = '\0';
#ifdef DEBUG
                    printf("当前提取的一行是:%s\n",m_read_buf);
#endif
                    return LINE_OK;
                }
                return LINE_BAD;
            }
        }
        //打印出当前提取的一行

        //尚未读取到结束符就继续读取
        return LINE_OPEN;
    }

//循环读取客户数据，知道无数据可读或者对方关闭连接
    bool http_conn::read() {
        if (m_read_idx >= READ_BUFFER_SIZE) {
            return false;
        }
        int bytes_read = 0;
        while (true) {
            bytes_read = recv(m_sockfd, m_read_buf + m_read_idx, READ_BUFFER_SIZE - m_read_idx, 0);
            cout<<"打印出读取到的内容:";
            cout<<m_read_buf<<endl;
            //打印接收的请求报文，可以接收
            if (bytes_read == -1) {
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    //要求再次读取,则直接返回true，表示读取成功
                    break;
                }
                return false;
            } else if (bytes_read == 0) {
                //收到EOF，也就是客户端关闭连接,返回失败会关闭连接
                return false;
            }
            m_read_idx += bytes_read;
        }
        return true;
    }

//解析HTTP请求行，获得请求方法，目标URL，以及HTTP版本号
    http_conn::HTTP_CODE http_conn::parse_request_line(char *text) {
        //找出text中第一个\t
        m_url = strpbrk(text, " \t");
        //std::cout<<"m_rul"<<*m_url<<std::endl;
        if (!m_url) {
            std::cout << "-->解析行有错误，直接返回了!" << std::endl;
            return BAD_REQUEST;
        }
        *m_url++ = '\0';
        //到第一个\t结束即是method
        char *method = text;
#ifdef DEBUG
        std::cout<<"-->当前解析出来的方法"<<method<<std::endl;
#endif
        //忽略大小写比较两个字符串
        if (strcasecmp(method, "GET") == 0) {
            m_method = GET;
        } else if (strcasecmp(method, "POST") == 0) {
            //添加POST方法的解析
            m_method = POST;
            //设置cgi
            cgi = 1;
        } else {
            //仅支持GET一种
            return BAD_REQUEST;
        }
        //strspn返回str1中第一个不在str2中出现的下标
        m_url += strspn(m_url, " \t");
        //找出version
        m_version = strpbrk(m_url, " \t");
        if (!m_version) {
            std::cout << "解析版本错误1" << std::endl;
            return BAD_REQUEST;
        }
        *m_version++ = '\0';
        m_version += strspn(m_version, " \t");
#ifdef DEBUG
        std::cout<<"-->当前解析出来的版本"<<m_version<<std::endl;
#endif
        if ((strcasecmp(m_version, "HTTP/1.1") != 0) && (strcasecmp(m_version, "HTTP/1.0") != 0)) {
            //只处理这种版本的HTTP
            std::cout << "解析版本错误2" << std::endl;
            return BAD_REQUEST;
        }
        //当前url指向的就是中间的文件
        if (strncasecmp(m_url, "http://", 7) == 0) {
            //如果包含该字符则跳过
            m_url += 7;
            //从str1中搜索第一次出现str2的位置
            m_url = strchr(m_url, '/');
        }
        if (!m_url || m_url[0] != '/') {
            std::cout << "解析url错误" << std::endl;
            return BAD_REQUEST;//语法错误
        }
        //如果当前解析到的是“/”，则显示欢迎页面
        //if(strcasecmp(m_url,"/") == 0){
        if (strlen(m_url) == 1) {
            strcat(m_url, "judge.html");
        }
#ifdef DEBUG
        std::cout<<"-->当前解析出来的文件url"<<m_url<<std::endl;
        std::cout<<"-->当前解析出来的文件version"<<m_version<<std::endl;
#endif
        //转移到下一个状态
        m_check_state = CHECK_STATE_HEADER;
        return NO_REQUEST;//表示当前服务器处理HTTP请求的状态是需要继续读取
    }

//解析HTTP请求的一个头部信息
    http_conn::HTTP_CODE http_conn::parse_headers(char *text) {
        //遇到空行表示头部信息解析完毕
        if (text[0] == '\0') {
            //如果HTTP请求有消息体，则还需要读取m_content_length字节的消息体
            //状态机转移到CHECK_STATE_CONTENT状态
            if (m_content_length != 0) {
                //说明是POST方法
                //转移到下一个状态
                m_check_state = CHECK_STATE_CONTENT;
                //并且设置当前读取尚未完成,需要继续提取行，继续解析
                return NO_REQUEST;//表示读取尚未结束
            }
            //否则说明我们已经得到了一个完整的HTTP请求
            return GET_REQUEST;
        } else if (strncasecmp(text, "Connection:", 11) == 0) {
            //处理Connection头部字段
            text += 11;
            text += strspn(text, " \t");
            if (strcasecmp(text, "keep-alive") == 0) {
                m_linger = true;//表示长连接
            }
        } else if (strncasecmp(text, "Content-Length:", 15) == 0) {
            //处理Content-Length头部字段
            text += 15;
            text += strspn(text, " \t");
            m_content_length = atol(text);
        } else if (strncasecmp(text, "Host:", 5) == 0) {
            //处理Host头部字段
            text += 5;
            text += strspn(text, " \t");
            m_host = text;
        } else {
#ifdef DEBUG
            printf("oop! unknow header %s\n",text);
#endif
        }
        return NO_REQUEST;
    }

//我们没有真正解析HTTP请求的消息体，只是判断它是否被完整读取
    http_conn::HTTP_CODE http_conn::parse_content(char *text) {
        if (m_read_idx >= (m_content_length + m_checked_idx)) {
            text[m_content_length] = '\0';
            m_string = text;//保存请求内容
            return GET_REQUEST;//表示读取完毕
        }
        return NO_REQUEST;
    }

//主状态机，按照顺序解析HTTP请求的各个字段
    http_conn::HTTP_CODE http_conn::process_read() {
        LINE_STATUS line_status = LINE_OK;//保存行读取状态
        HTTP_CODE ret = NO_REQUEST;//保存解析状态
        char *text = 0;
        //提取一行看看返回值
        //std::cout<<parse_line()<<std::endl;
        while (((m_check_state == CHECK_STATE_CONTENT) && (line_status == LINE_OK))
               || ((line_status = parse_line()) == LINE_OK)) {
#ifdef DEBUG
            std::cout<<"-->成功提取到一行,进的了主循环"<<std::endl;
#endif
            //什么情况下进入状态机进行循环解析呢？
            //1.主状态机处在解析消息体，且行提取完毕
            //2.提取一行成功
            text = get_line();//获取当前的起始地址
#ifdef DEBUG
            printf("-->当前提取出来的一行是：%s\n",text);
#endif
            m_start_line = m_checked_idx;//记录下一行的起始位置
            //printf("got 1 http line:%s\n",text);
            switch (m_check_state) {
                //正在确认请求行
                case CHECK_STATE_REQUESTLINE: {
                    ret = parse_request_line(text);
#ifdef DEBUG
                    std::cout<<"-->解析请求行得到："<<m_method<<" "<<m_version<<" "<<m_url<<std::endl;
#endif
                    if (ret == BAD_REQUEST) {
#ifdef DEBUG
                        std::cout<<"-->process_read return BAD_REQUEST"<<std::endl;
#endif
                        return BAD_REQUEST;
                    }
                    break;
                }
                case CHECK_STATE_HEADER: {
                    ret = parse_headers(text);
                    if (ret == BAD_REQUEST)
                        return BAD_REQUEST;
                    else if (ret == GET_REQUEST)
                        //说明是GET方法，读取完毕了
                        return do_request();
                    //否则如果是POST方法，则要再次读取
                    break;
                }
                case CHECK_STATE_CONTENT: {
                    ret = parse_content(text);
#ifdef DEBUG
                    printf("当前得到的post请求的内容是:%s\n",m_string);
#endif
                    if (ret == GET_REQUEST)
                        return do_request();
                    line_status = LINE_OPEN;
                    break;
                }
                default: {
                    return INTERNAL_ERROR;
                }
            }
        }
        return NO_REQUEST;
    }

//当得到一个完整、正确的HTTP请求时，我们就分析目标文件的属性
//如果目标文件存在，对所有用户可读，且不是目录，则使用mmap将其映射到
//内存地址，m_file_address处，并告诉调用者获取文件成功
    http_conn::HTTP_CODE http_conn::do_request() {
        strcpy(m_real_file, doc_root);
        int len = strlen(doc_root);
        //解析post的账号密码
        if (strcasecmp(m_url, "/login") == 0) {
            //如果是登录请求，判断账号密码
            char *username, *password;
            char *text = m_string;
            //user=123&password=123
            //跳过多余的空格
            text += strspn(m_string, " \t");
            if (strncasecmp(text, "user=", 5) == 0) {
                //如果找到了用户名,提取出来
                text += 5;
                char *temp = strpbrk(text, "&");//找出&所在位置
                *temp = '\0';//设为结束符
                username = text;
#ifdef DEBUG
                std::cout<<"解析到用户名是:"<<username<<std::endl;
#endif
                text = temp + 1;
                //解析密码
                if (strncasecmp(text, "password=", 9) == 0) {
                    text += 9;
                    password = text;
#ifdef DEBUG
                    std::cout<<"解析到用户名是:"<<password<<std::endl;
#endif
                    if ((strcasecmp(username, "123") == 0) && (strcasecmp(password, "123") == 0)) {
                        //构建root/index.html
                        strncpy(m_real_file + len, "/welcome.html", FILENAME_LEN - len - 1);
                    } else {
                        strncpy(m_real_file + len, "/logError.html", FILENAME_LEN - len - 1);
                    }
                } else {
                    strncpy(m_real_file + len, "/logError.html", FILENAME_LEN - len - 1);
                }
            } else {
                strncpy(m_real_file + len, "/logError.html", FILENAME_LEN - len - 1);
            }
        } else {
            strncpy(m_real_file + len, m_url, FILENAME_LEN - len - 1);
        };

//    //构建root/index.html
//    strncpy(m_real_file + len,m_url,FILENAME_LEN-len-1);
        cout<<m_real_file<<endl;
        if (stat(m_real_file, &m_file_stat) < 0) {

            cout<<"？？？？？？？？？？"<<endl;
            return NO_RESOURCE;
        }
        if (!(m_file_stat.st_mode & S_IROTH)) {
            //如果其他用户对该文件没有可读权限，则出错
            std::cout << "没有权限" << std::endl;
            return FORBIDDEN_REQUEST;
        }
        if (S_ISDIR(m_file_stat.st_mode)) {
            //如果请求的文件夹
            std::cout << "请求的是文件夹" << std::endl;
            return BAD_REQUEST;//错误请求
        }
        //打开文件
        int fd = open(m_real_file, O_RDONLY);
        //表示只读该文件，然后是进程独有，从0开始读取，返回指针
        m_file_address = (char *) mmap(0, m_file_stat.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
        close(fd);
        return FILE_REQUEST;
    }

    void http_conn::unmap() {
        if (m_file_address) {
            //如果该地址存在，就回收
            munmap(m_file_address, m_file_stat.st_size);
            m_file_address = 0;
        }
    }
/*
//写HTTP响应
bool http_conn::write() {
    int temp = 0;
    int newadd = 0;
//    int bytes_have_send = 0;
//    int bytes_to_send = m_write_idx;
    if(bytes_to_send == 0){
        //如果不需要写入，则复原
        modfd(m_epollfd,m_sockfd,EPOLLIN);
        init();
        return true;
    }
    while(1){
        temp = writev(m_sockfd,m_iv,m_iv_count);
        if(temp > 0 ){
            bytes_have_send += temp;
            newadd = bytes_have_send - m_write_idx;
        }
        if(temp <= -1){
            //如果TCP写缓冲没有空间，则等待下一轮EPOLLOUT事件
            //虽然在此期间服务器无法立即接收同一客户端的下一个秦桧去
            //但这可以保证连接的完整性
            if(errno = EAGAIN){
//                modfd(m_epollfd,m_sockfd,EPOLLOUT);
//                return true;

                if (bytes_have_send >= m_iv[0].iov_len)
                {
                    m_iv[0].iov_len = 0;
                    m_iv[1].iov_base = m_file_address + newadd;
                    m_iv[1].iov_len = bytes_to_send;
                }
                else
                {
                    m_iv[0].iov_base = m_write_buf + bytes_to_send;
                    m_iv[0].iov_len = m_iv[0].iov_len - bytes_have_send;
                }
                modfd(m_epollfd, m_sockfd, EPOLLOUT);
                return true;

            }
            unmap();
            return false;
        }
        bytes_to_send -= temp;
        //bytes_have_send += temp;
        if(bytes_to_send <= 0 ){
            //发送HTTP响应成功，根据HTTP请求中的Connection字段决定是否立即关闭连接
            unmap();
            if(m_linger){
                //如果是长连接
                init();
                modfd(m_epollfd,m_sockfd,EPOLLIN);
                return true;
            }else{
                modfd(m_epollfd,m_sockfd,EPOLLIN);
                return false;
            }
        }
    }
}*/
/*
bool http_conn::write(){
    int temp = 0;
    //打印一下要写的东西先
    printf("要写的头:%s\n",m_write_buf);
    printf("要写的文件:%s\n",m_file_address);
    if (bytes_to_send == 0)
    {
        modfd(m_epollfd, m_sockfd, EPOLLIN);
        init();
        return true;
    }
    //循环写
    while(1) {
        m_linger = false;
        //判断当前要发送第一个块还是第二个块
        if (bytes_have_send < m_iv[0].iov_len) {
            //说明第一个块尚未发送完成
            temp = send(m_sockfd, m_write_buf + bytes_have_send, m_iv[0].iov_len - bytes_have_send, 0);
        }else{
            //发送第二个块
            temp = send(m_sockfd, m_file_address + bytes_have_send - (m_iv[0].iov_len), m_iv[1].iov_len - (bytes_have_send - m_iv[0].iov_len), 0);
        }
        if (temp == -1) {
            //说明当前发送尚未完成
            if(errno == EAGAIN || errno == EWOULDBLOCK){
                modfd(m_epollfd, m_sockfd, EPOLLOUT);
                return true;
            }
            printf("这里错误1\n");
            unmap();
            return false;
        }
        bytes_have_send += temp;
        bytes_to_send -= temp;
        if(bytes_to_send <= 0){
            unmap();
            modfd(m_epollfd, m_sockfd, EPOLLIN);
            if (m_linger)
            {
                printf("写回去给客户端成功\n");
                init();
                return true;
            }
            else
            {
                printf("短链接返回失败\n");
                return false;
            }
        }
    }

}*/
    bool http_conn::write() {
        int temp = 0;

        if (bytes_to_send == 0) {
            Utils::modfd(m_epollfd, m_sockfd, EPOLLIN);
            init();
            return true;
        }

        while (1) {
            //打印一下要写的东西先
#ifdef DEBUG
            printf("要写的头:%s\n",m_write_buf);
            printf("要写的文件:%s\n",m_file_address);
#endif
            temp = writev(m_sockfd, m_iv, m_iv_count);
#ifdef DEBUG
            printf("写入了多少temp：%d\n",temp);
#endif
            if (temp < 0) {
                if (errno == EAGAIN) {
#ifdef DEBUG
                    printf("需要继续写\n");
#endif
                    Utils::modfd(m_epollfd, m_sockfd, EPOLLOUT);
                    return true;
                }
#ifdef  DEBUG
                printf("这里错误1\n");
#endif
                unmap();
                return false;
            }
            bytes_have_send += temp;
            bytes_to_send -= temp;
#ifdef DEBUG
            printf("一种已经发送了bytes_hava_send：%d\n",bytes_have_send);

            printf("还差多少bytes_to_send：%d\n",bytes_to_send);
#endif
            if (bytes_have_send >= m_iv[0].iov_len) {
                m_iv[0].iov_len = 0;
                m_iv[1].iov_base = m_file_address + (bytes_have_send - m_write_idx);
                m_iv[1].iov_len = bytes_to_send;
            } else {
                m_iv[0].iov_base = m_write_buf + bytes_have_send;
                m_iv[0].iov_len = m_iv[0].iov_len - bytes_have_send;
            }

            if (bytes_to_send <= 0) {
                unmap();
                Utils::modfd(m_epollfd, m_sockfd, EPOLLIN);

                if (m_linger) {
#ifdef DEBUG
                    printf("写回去给客户端成功\n");
#endif
                    init();
                    return true;
                } else {
#ifdef DEBUG
                    printf("短链接返回失败\n");
#endif
                    return false;
                }
            }
        }
    }

//bool http_conn::write()
//{
//    int temp = 0;
//
//    int newadd = 0;
//
//    if (bytes_to_send == 0)
//    {
//        printf("写入文件大小为0,因此没有写\n");
//        modfd(m_epollfd, m_sockfd, EPOLLIN);
//        init();
//        return true;
//    }
//
//    while (1)
//    {
//        printf("-->进入write函数\n");
//        temp = writev(m_sockfd, m_iv, m_iv_count);
//        printf("本次写入temp:%d\n",temp);
//        if (temp > 0)
//        {
//            bytes_have_send += temp;
//            //newadd = bytes_have_send - m_write_idx;
//            newadd = ;
//            printf("bytes_have_send = %d,newadd = %d\n",bytes_have_send,newadd);
//        }
//        if (temp <= -1)
//        {
//            if (errno == EAGAIN)
//            {
//                if (bytes_have_send >= m_iv[0].iov_len)
//                {
//                    m_iv[0].iov_len = 0;
//                    m_iv[1].iov_base = m_file_address + newadd;
//                    m_iv[1].iov_len = bytes_to_send;
//                }
//                else
//                {
//                    m_iv[0].iov_base = m_write_buf + bytes_have_send;
//                    m_iv[0].iov_len = m_iv[0].iov_len - bytes_have_send;
//                }
//                modfd(m_epollfd, m_sockfd, EPOLLOUT);
//                return true;
//            }
//            unmap();
//            printf("这里出错1\n");
//            return false;
//        }
//        bytes_to_send -= temp;
//        if (bytes_to_send <= 0)
//        {
//            unmap();
//            modfd(m_epollfd, m_sockfd, EPOLLIN);
//            if (m_linger)
//            {
//                init();
//                return true;
//            }
//            else
//            {
//                printf("短连接返回错误\n");
//                return false;
//            }
//        }
//    }
//}
//往写缓冲中写入待发送的数据
    bool http_conn::add_response(const char *format, ...) {
        if (m_write_idx >= WRITE_BUFFER_SIZE) {
            return false;
        }
        //可变参数列表
        va_list arg_list;
        //用arg_list指向可变参数列表
        va_start(arg_list, format);
        //将可变参数写入写缓冲区
        int len = vsnprintf(m_write_buf + m_write_idx, WRITE_BUFFER_SIZE - 1 - m_write_idx,
                            format, arg_list);
        if (len >= (WRITE_BUFFER_SIZE - 1 - m_write_idx)) {
            return false;
        }
        m_write_idx += len;
        va_end(arg_list);//关闭，与start成对使用
        return true;
    }

//添加状态行
    bool http_conn::add_status_line(int status, const char *title) {
        return add_response("%s %d %s\r\n", "HTTP/1.1", status, title);
    }

//添加消息头
    bool http_conn::add_headers(int content_length) {
        add_content_length(content_length);
        add_linger();
        add_blank_line();
    }

//添加消息长度
    bool http_conn::add_content_length(int content_length) {
        return add_response("Content-Length: %d\r\n", content_length);
    }

    bool http_conn::add_linger() {
        return add_response("Connection: %s\r\n", (m_linger == true) ? "keep-alive" : "close");
    }

    bool http_conn::add_blank_line() {
        return add_response("%s", "\r\n");
    }

    bool http_conn::add_content(const char *content) {
        return add_response("%s", content);
    }

//根据服务器处理HTTP请求的结果，决定返回客户端的内容
    bool http_conn::process_write(http_conn::HTTP_CODE ret) {
        //打印当前的写返回的状态码
#ifdef DEBUG
        printf("do the process_write\n");
        printf("current HTTP_CODE is %d\n",ret);
#endif
        switch (ret) {
            case INTERNAL_ERROR: {
                add_status_line(500, error_500_title);
                add_headers(strlen(error_500_form));
                if (!add_content(error_500_form)) {
                    return false;
                }
                break;
            }
            case BAD_REQUEST: {
                add_status_line(400, error_400_title);
                add_headers(strlen(error_400_form));
                if (!add_content(error_400_form)) {
                    return false;
                }
                break;
            }
            case NO_RESOURCE: {
                add_status_line(403, error_403_title);
                add_headers(strlen(error_403_form));
                if (!add_content(error_403_form)) {
                    return false;
                }
                break;
            }
            case FILE_REQUEST: {
#ifdef DEBUG
                printf("执行到FILE_REQUEST！");
#endif
                add_status_line(200, ok_200_title);
                if (m_file_stat.st_size != 0) {
                    add_headers(m_file_stat.st_size);
                    //内容部分包括返回的信息及返回的文件
                    m_iv[0].iov_base = m_write_buf;
                    m_iv[0].iov_len = m_write_idx;
                    m_iv[1].iov_base = m_file_address;
                    m_iv[1].iov_len = m_file_stat.st_size;
                    m_iv_count = 2;
                    bytes_to_send = m_write_idx + m_file_stat.st_size;
                    return true;
                } else {
                    const char *ok_string = "<html><body></body></html>";
                    add_headers(strlen(ok_string));
                    if (!add_content(ok_string)) {
#ifdef DEBUG
                        printf("写入okstring出错\n");
#endif
                        return false;
                    }
                }
//改成这样子
//            const char* ok_string = "<html><body>ok</body></html>";
//                add_headers(strlen(ok_string));
//                if(!add_content(ok_string)){
//                    return false;
//                }
            }
            default: {
                return false;
            }
        }

        m_iv[0].iov_base = m_write_buf;
        m_iv[0].iov_len = m_write_idx;
        m_iv_count = 1;
        bytes_to_send = m_write_idx;
        return true;
    }

//由线程池中的工作线程调用，这是处理HTTP请求的入口函数
    void http_conn::process() {
        //先执行解析读取到的请求报文,根据读取状态决定当前继续执行读操作还是可以返回响应报文了
        //首先打印出读取到的值
        //LOG_INFO("读取到的请求报文是:\n %s\n",m_read_buf);
#ifdef DEBUG
        printf("读取到的请求报文是:\n %s\n",m_read_buf);
#endif
        HTTP_CODE read_ret = process_read();
        //打印一下能否解析请求报文，看看能否正常解析
#ifdef DEBUG
        //    std::cout<<"解析完读取到的报文的状态是："<<read_ret<<std::endl;
    //    std::cout<<"请求行是："<<m_method<<" "<<m_url<<" "<<m_version<<" "<<std::endl;
    printf("解析完读取的报文的状态是%d\n",read_ret);
    printf("请求行是方法:%d url:%s version:%s\n",m_method,m_url,m_version);
#endif
        if (read_ret == NO_REQUEST) {
            //如果尚未读取完成，则继续读取
#ifdef DEBUG
            printf("-->do the read_ret == NO_REQUEST");
#endif
            Utils::modfd(m_epollfd, m_sockfd, EPOLLIN);
            return;
        }
        //其他时候根据读取结果返回信息
#ifdef DEBUG
        std::cout<<"**********************执行到写了"<<std::endl;
#endif
        bool write_ret = process_write(read_ret);
        if (write_ret == false) {
            //如果写回失败，认为客户端关闭了，则关闭这个连接
#ifdef DEBUG
            printf("********写入失败了，返回的是%d\n",write_ret);
#endif
            close_conn();
        } else {
#ifdef DEBUG
            printf("*******写入成功,返回码是%d\n", write_ret);
#endif
        }
        //如果写入成功了，我们就可注册写事件了
        Utils::modfd(m_epollfd, m_sockfd, EPOLLOUT);
    }
}