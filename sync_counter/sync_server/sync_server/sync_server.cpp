#include <iostream>
#include<cstdlib>
#include<boost/asio.hpp>
#include<set>
using namespace boost::asio::ip;
using namespace std;
const int max_length = 1024;
typedef std::shared_ptr<tcp::socket> sock_ptr;
std::set<std::shared_ptr<std::thread>> thread_set;

//为服务端处理客户端请求, 每当接受客户端连接就调用该函数
void session(sock_ptr sock) {
    try {
        cout << "server is running" << endl;
        for (;;) {
            char data[max_length];
            memset(data, '\0', sizeof(data));
            boost::system::error_code error;
            size_t length = sock->read_some(boost::asio::buffer(data, max_length), error);
            if (error == boost::asio::error::eof) {
                cout << "connection closed by peer" << endl;
            }
            else if (error) {
                throw boost::system::system_error(error);
            }

            cout << "receive from" << sock->remote_endpoint().address().to_string() << endl;
            cout << "receive message is: " << data << endl;
            //回传信息值
            boost::asio::write(*sock, boost::asio::buffer(data, length));
        }
    }
    catch (std::exception& e) {
        cerr << "Exception is: " << e.what() << endl;
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
        std::thread server_thread(server, std::ref(ioc), 5283);

        // 主线程等待用户输入，用于测试
        std::string input;
        std::getline(std::cin, input);

        // 关闭服务器
        ioc.stop();
        server_thread.join();

        for (auto t : thread_set) {
            t->join();
        }
    }
    catch (std::exception& e) {
        cerr << "Exception is: " << e.what() << endl;
    }
    return 0;
}