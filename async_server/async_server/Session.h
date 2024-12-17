#pragma once
#include<boost/asio.hpp>
#include<iostream>
#include<queue>
#include<mutex>
#include<boost/uuid/uuid_generators.hpp>
#include<boost/uuid/uuid_io.hpp>
#include "const.h"
#include "MsgNode.h"
//#include "msg.pb.h"

using namespace boost::asio::ip;

class CServer;
//会话类
class Session:public std::enable_shared_from_this<Session>
{
public:
	Session(boost::asio::io_context& ioc, CServer* server) :_socket(ioc), _server(server)
		, _b_close(false), _b_head_parse(false){
		boost::uuids::uuid a_uuid = boost::uuids::random_generator()();
		_uuid = boost::uuids::to_string(a_uuid);
		_rev_head_node = std::make_shared<MsgNode>(HEAD_TOTAL_LEN);
	}
	~Session() {
		std::cout << "Session destruct" << std::endl;
	}

	tcp::socket& GetSocket() {
		return _socket;
	}
	std::string Getuuid() {
		return _uuid;
	}

	//链接开始, 调用读函数
	void Start();

	void Send(char* msg, short max_length, short msg_id);

	//重载Send函数
	void Send(std::string msg, short msg_id);

	std::shared_ptr<Session> SharedSelf();

	void Close();

private:
	//回调函数, 调用写函数
	void HandleRead(const boost::system::error_code& error, size_t bytes_transfered, std::shared_ptr<Session> _self_shared);
	void HandleWrite(const boost::system::error_code& error, std::shared_ptr<Session> _self_shared);
	enum { max_length = 1024 };
	tcp::socket _socket;
	char _data[MAX_LENGTH];
	CServer* _server;
	std::string _uuid;
	std::queue<std::shared_ptr<SendNode>> _send_queue;
	std::mutex _send_lock;
	//切包: 接受到的头部信息
	std::shared_ptr<MsgNode> _rev_head_node;
	//判断头部是否解析完成
	bool _b_head_parse;
	//接收到的消息数据
	std::shared_ptr<RecvNode> _rev_msg_node;
	bool _b_close;
};