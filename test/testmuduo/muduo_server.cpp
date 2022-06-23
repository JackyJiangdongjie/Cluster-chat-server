/*
muduo网络库给用户提供了两个主要的类
TcpServer ： 用于编写服务器程序的
TcpClient ： 用于编写客户端程序的

epoll + 线程池
好处：能够把网络I/O的代码和业务代码区分开
                        用户的连接和断开       用户的可读写事件
*/
#include <muduo/net/TcpServer.h>
#include <muduo/net/EventLoop.h>
#include <iostream>
#include <functional>
#include <string>
using namespace std;
using namespace muduo;
using namespace muduo::net;
using namespace placeholders;//参数占位符

/*基于muduo网络库开发服务器程序
1.组合TcpServer对象
2.创建EventLoop事件循环对象的指针
3.明确TcpServer构造函数需要什么参数，输出ChatServer的构造函数
4.在当前服务器类的构造函数当中，注册处理连接的回调函数和处理读写时间的回调函数
5.设置合适的服务端线程数量，muduo库会自己分配I/O线程和worker线程
*/
class ChatServer
{
public:
    ChatServer(EventLoop *loop,               // 事件循环
               const InetAddress &listenAddr, // IP+Port
               const string &nameArg)                 //服务器的名字
        : _server(loop, listenAddr, nameArg), _loop(loop)
    {
        // 给服务器注册用户连接的创建和断开回调     回调：函数调用什么时候发生，发生以后怎么做，没在同一时刻
        _server.setConnectionCallback(std::bind(&ChatServer::onConnection, this, _1));
        //muduo库这里的setConnectionCallback函数只有一个参数，而非静态成员函数onConnection有两个参数，包含了this指针
        //this指针是隐含在每一个非静态成员函数内的一种指针，this指针指向被调用的成员函数所属的对象。
        //而非静态成员函数需要传递this指针作为第一个参数，所以需要用std::bind来绑定参数。

        // 给服务器注册用户读写事件回调
        _server.setMessageCallback(std::bind(&ChatServer::onMessage, this, _1, _2, _3));
        

        // 设置服务器端的线程数量 1个I/O线程   3个worker线程
        _server.setThreadNum(4);
    }

    void start()
    {
        _server.start();
    }

private:
    // 专门处理用户的连接创建和断开  epoll listenfd accept
    void onConnection(const TcpConnectionPtr &conn)   //非静态成员函数这里有两个参数，除了Ptr之外，还有this指针
    {
        if (conn->connected())  //true 的话，就是有连接成功的
        {
            cout << conn->peerAddress().toIpPort() << " -> " << conn->localAddress().toIpPort() << " state:online" << endl;
        }                                   //对端的地址信息                                                                                    //打印IP和端口 信息
        else
        {
            cout << conn->peerAddress().toIpPort() << " -> " << conn->localAddress().toIpPort() << " state:offline" << endl;
            conn->shutdown(); // close(fd)
            // _loop->quit();  //退出epoll  
        }
    }

    // 专门处理用户的读写事件
    void onMessage(const TcpConnectionPtr &conn, // 连接
                   Buffer *buffer,               // 缓冲区
                   Timestamp time)               // 接收到数据的时间信息
    {
        string buf = buffer->retrieveAllAsString();  //把缓冲区的数据全部放到字符串中
        cout << "recv data:" << buf << " time:" << time.toFormattedString() << endl;      //把time信息转换成字符串
        conn->send(buf);  //把收到的用户信息，处理之后，再返回去，类似于回声服务器
    }

    TcpServer _server; // #1
    EventLoop *_loop;  // #2 epoll
};

int main()
{
    EventLoop loop; // 类似于epoll
    InetAddress addr("127.0.0.1", 6000);
    ChatServer server(&loop, addr, "ChatServer");

    server.start(); // listenfd epoll_ctl=>epoll
    loop.loop();    // epoll_wait以阻塞方式等待新用户连接，已连接用户的读写事件等  有这两种事件发生的话，会帮我们回调
                                // onConnection和onMessage 方法
}