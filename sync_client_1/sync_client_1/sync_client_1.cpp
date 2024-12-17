#include<iostream>
#include<boost/asio.hpp>
//#include "msg.pb.h"
#include <json/json.h>
#include <json/value.h>
#include <json/reader.h>
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

		/*MsgData msgdata;
		msgdata.set_id(1001);
		msgdata.set_data("hello,world");
		std::string request;
		msgdata.SerializeToString(&request);*/

		Json::Value root;
		root["id"] = 1001;
		root["data"] = "hello world!";
		std::string request = root.toStyledString();
		size_t request_length = request.length();
		short request_length_net = boost::asio::detail::socket_ops::host_to_network_short(request_length);
		// 先发送数据2个字节的数据长度，再发送数据消息的结构
		char send_data[MAX_LENGTH] = { 0 };
		memcpy(send_data, &request_length_net, 2);
		memcpy(send_data + 2, request.c_str(), request_length);
		boost::asio::write(_sock, boost::asio::buffer(send_data, request_length + 2));

		char reply_head[2];
		size_t reply_head_len = boost::asio::read(_sock, boost::asio::buffer(reply_head, 2));
		short msg_len = 0;
		memcpy(&msg_len, reply_head, 2);
		// 将网络字节序转换为主机字节序
		short msg_len_host = boost::asio::detail::socket_ops::network_to_host_short(msg_len);
		char msg[MAX_LENGTH] = { 0 };
		size_t msg_length = boost::asio::read(_sock, boost::asio::buffer(msg, msg_len));

		/*MsgData revmsg;
		revmsg.ParseFromArray(msg, msg_len);*/
		Json::Reader reader;
		reader.parse(std::string(msg, msg_length), root);
		std::cout << "msg id is " << root["id"].asInt() << " content is " << root["data"].asString() << std::endl;
		// ?
		getchar();
	}
	catch(std::exception& e){
		std::cout << "Exception is: " << e.what() << std::endl;
	}
}