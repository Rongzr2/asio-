#include <iostream>
#include<boost/asio.hpp>
using namespace std;
using namespace boost::asio::ip;
const int MAX_LENGTH = 1024;
#define OPSZ 4

int main()
{
    try {
        //创建上下文服务
        boost::asio::io_context ioc;
        //构造对端端点
        tcp::endpoint remote_ep(address::from_string("127.0.0.1"), 5283);
        tcp::socket sock(ioc);
        boost::system::error_code error = boost::asio::error::host_not_found;
        sock.connect(remote_ep, error);

        //魔改算数器
        cout << "Operand count: ";
        char opmsg[MAX_LENGTH];
        int opmsg_cnt = 0;
        cin >> opmsg_cnt;
        opmsg[0] = (char)opmsg_cnt;

        for (int i = 0; i < opmsg_cnt; ++i) {
            cout << "Operand " << i + 1 << ": ";
            int value;
            cin >> value;
            *(int*)&opmsg[i * OPSZ + 1] = value;
        }
        cout << "Operator: ";
        char op;
        cin >> op;
        opmsg[opmsg_cnt * OPSZ + 1] = op;
        boost::asio::write(sock, boost::asio::buffer(opmsg, opmsg_cnt * OPSZ + 2));

        char reply[MAX_LENGTH];
        size_t reply_length = boost::asio::read(sock, 
            boost::asio::buffer(reply, sizeof(int)), boost::asio::transfer_all());
        cout << "Operation result is: ";
        int result = *(int*)reply;
        cout << result << "\n";
    }
    catch (std::exception& e) {
        cerr << "Exception: " << e.what() << endl;
    }
    return 0;
}