#include<iostream>
#include<boost/asio.hpp>
using namespace boost::asio::ip;
const int MAX_LENGTH = 1024;

int main()
{
	try {
		boost::asio::io_context ioc;
		// 构造端点
		tcp::endpoint remote_ep(address::from_string("127.0.0.1"), 5283);
		tcp::socket _sock(ioc);
		boost::system::error_code error = boost::asio::error::host_not_found;
		_sock.connect(remote_ep, error);
		if (error) {
			std::cerr << "connect failed, code is: " << error.value() << std::endl;
			return 0;
		}

		char request[MAX_LENGTH];
		std::cout << "Enter Message: ";
		std::cin.getline(request, MAX_LENGTH);
		size_t request_length = strlen(request);
		// 先发送数据2个字节的数据长度，再发送数据消息的结构
		char send_data[MAX_LENGTH] = { 0 };
		memcpy(send_data, &request_length, 2);
		memcpy(send_data + 2, request, request_length);
		boost::asio::write(_sock, boost::asio::buffer(send_data, request_length + 2));

		char reply_head[2];
		size_t reply_head_len = boost::asio::read(_sock, boost::asio::buffer(reply_head, 2));
		short msg_len = 0;
		memcpy(&msg_len, reply_head, 2);
		char msg[MAX_LENGTH];
		boost::asio::read(_sock, boost::asio::buffer(msg, msg_len));

		std::cout << "Reply Message is: ";
		std::cout.write(msg, msg_len) << std::endl;
		std::cout << "reply length is: " << msg_len;
		std::cout << "\n";
	}
	catch(std::exception& e){
		std::cout << "Exception is: " << e.what() << std::endl;
	}
}