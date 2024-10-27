#include <iostream>
#include<cstdlib>
#include<boost/asio.hpp>
#include<set>
using namespace boost::asio::ip;
using namespace std;
const int max_length = 1024;
#define OPSZ 4
typedef std::shared_ptr<tcp::socket> sock_ptr;
std::set<std::shared_ptr<std::thread>> thread_set;

int calculate(int opnum, int opnds[], char op) {
    int result = opnds[0];
    switch (op) {
    case '+': {
        for (int i = 1; i < opnum; ++i) result += opnds[i];
        break;
    }
    case '-': {
        for (int i = 1; i < opnum; ++i) result -= opnds[i];
        break;
    }
    case '*': {
        for (int i = 1; i < opnum; ++i) result *= opnds[i];
        break;
    }
    }
    return result;
}

//为服务端处理客户端请求, 每当接受客户端连接就调用该函数
void session(sock_ptr sock) {
    for (;;) {
        try {
            cout << "server is running" << endl;
            char data[max_length];
            memset(data, '\0', sizeof(data));
            boost::system::error_code error;

            char opnum_char;
            boost::asio::read(*sock, boost::asio::buffer(&opnum_char, 1));
            int opnum = (unsigned char)opnum_char;

            // 计算预期的数据大小
            size_t expected_size = opnum * OPSZ + 1;

            // 读取完整的消息
            boost::asio::read(*sock, boost::asio::buffer(data, expected_size), 
                boost::asio::transfer_exactly(expected_size), error);
            if (error == boost::asio::error::eof) {
                cout << "Connection closed by peer" << endl;
                break;
            }
            else if (error) {
                throw boost::system::system_error(error);
            }

            int* opnds = (int*)&data[0];
            char op = data[expected_size - 1];
            int result = calculate(opnum, opnds, op);

            //回传信息值
            boost::asio::write(*sock, boost::asio::buffer(&result, 
                sizeof(result)), boost::asio::transfer_all());
        }
        catch (std::exception& e) {
            cerr << "Exception is: " << e.what() << endl;
        }
    }
}

//根据服务器ip和端口创建服务器acceptor来接收数据,用socket来接受新的连接,然后为socket创建session
void server(boost::asio::io_context& ioc, unsigned short port) {
    tcp::acceptor a(ioc, tcp::endpoint(tcp::v4(), port));
    for (;;) {
        sock_ptr socket(new tcp::socket(ioc));
        a.accept(*socket);
        // 创建线程调用session函数可以分配独立的线程用于socket的读写，
        // 保证acceptor不会因为socket的读写而阻塞。
        auto t = std::make_shared<std::thread>(session, socket);
        thread_set.insert(t);
    }
}

int main()
{
    try {
        boost::asio::io_context ioc;
        server(ioc, 5283);

        for (auto t : thread_set) {
            t->join();
        }
    }
    catch (std::exception& e) {
        cerr << "Exception is: " << e.what() << endl;
    }
    return 0;
}