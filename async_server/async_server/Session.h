#pragma once
#include<boost/asio.hpp>
#include<iostream>
#include<queue>
#include<mutex>
#include<boost/uuid/uuid_generators.hpp>
#include<boost/uuid/uuid_io.hpp>	
using namespace boost::asio::ip;

#define HEAD_LENGTH 2
#define MAX_LENGTH 1024*2

// 消息节点
class MsgNode {
public:
	friend class Session;

	// 构造节点时把消息长度也写进去, 用两个字节的大小存储
	// 这个构造函数用于发送消息时构造消息节点
	MsgNode(char* msg, short max_len) :total_len(max_len + HEAD_LENGTH), cur_len(0) {
		_data = new char[total_len + 1]();
		memcpy(_data, &max_len, HEAD_LENGTH);
		memcpy(_data + HEAD_LENGTH, &msg, max_len);
		_data[total_len] = '\0';
	}

	//接收对端数据--构造消息节点时调用
	MsgNode(short max_len) :total_len(max_len), cur_len(0) {
		_data = new char[total_len + 1];
	}

	void Clear() {
		::memset(_data, '\0', total_len);
		cur_len = 0;
	}

	~MsgNode() {
		delete[] _data;
	}

private:
	int cur_len;
	int total_len;
	char* _data;
};

class CServer;
//会话类
class Session:public std::enable_shared_from_this<Session>
{
public:
	Session(boost::asio::io_context& ioc, CServer* server) :_socket(ioc), _server(server)
		, _b_close(false), _b_head_parse(false){
		boost::uuids::uuid a_uuid = boost::uuids::random_generator()();
		_uuid = boost::uuids::to_string(a_uuid);
		_rev_head_node = std::make_shared<MsgNode>(HEAD_LENGTH);
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

	void Send(char* msg, int max_length);

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
	std::queue<std::shared_ptr<MsgNode>> _send_queue;
	std::mutex _send_lock;
	//切包: 接受到的头部信息
	std::shared_ptr<MsgNode> _rev_head_node;
	//判断头部是否解析完成
	bool _b_head_parse;
	//接收到的消息数据
	std::shared_ptr<MsgNode> _rev_msg_node;
	bool _b_close;
};