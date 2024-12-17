#include "Session.h"
#include "CServer.h"
#include <json/reader.h>
#include <json/value.h>
#include <json/json.h>

void Session::Start()
{
	memset(_data, 0, max_length);
	//注意bind绑定的第五个参数
	_socket.async_read_some(boost::asio::buffer(_data, max_length),
		std::bind(&Session::HandleRead, this, std::placeholders::_1, std::placeholders::_2,shared_from_this()));
}

void Session::Send(char* msg, short max_length, short msg_id)
{
	std::lock_guard < std::mutex> lock(_send_lock);
	int send_queue_size = _send_queue.size();
	if (send_queue_size > MAX_SENDQUE) {
		std::cout << "session " << _uuid << " send queue is full, size is " << MAX_SENDQUE << std::endl;
		return;
	}

	_send_queue.push(std::make_shared<SendNode>(msg, max_length, msg_id));
	// ???????不是<=0???? 
	//当Send函数被调用时, 会尝试将新消息传入队列中
	//当队列为空时, 说明没有消息, 此时可以加入队列并调用异步写发送新消息
	//当不为空时, 就不加入队列, 以此来保证发送消息的有序
	if (send_queue_size > 0) {
		return;
	}

	auto& msgnode = _send_queue.front();
	//发送队列为空，则说明当前没有未发送完的数据，将要发送的数据放入队列并调用async_write函数发送数据
	boost::asio::async_write(_socket, boost::asio::buffer(msgnode->_data, msgnode->total_len),
		std::bind(&Session::HandleWrite, this, std::placeholders::_1, SharedSelf()));
}

void Session::Send(std::string msg, short msg_id)
{
	std::lock_guard<std::mutex> lock(_send_lock);
	int send_queue_size = _send_queue.size();
	if (send_queue_size > MAX_SENDQUE) {
		std::cout << "session " << _uuid << " queue is full, size is " << MAX_SENDQUE << std::endl;
		return;
	}

	_send_queue.push(std::make_shared<SendNode>(msg.c_str(), msg.length(), msg_id));
	if (send_queue_size > 0) {
		return;
	}
	auto& msgnode = _send_queue.front();
	boost::asio::async_write(_socket,boost::asio::buffer(msgnode->_data, msgnode->total_len),
		std::bind(&Session::HandleWrite,this,std::placeholders::_1, SharedSelf()));
}

std::shared_ptr<Session> Session::SharedSelf()
{
	return shared_from_this();
}

void Session::Close()
{
	_socket.close();
	_b_close = true;
}

//读(操作)挂起, 异步写发送给对端
//一个智能指针, 智能指针的一个特性: 引用次数不为0,不会被析构;
//利用智能指针延长会话类的生命周期
void Session::HandleRead(const boost::system::error_code& error, size_t bytes_transfered,
	std::shared_ptr<Session> _self_shared)
{
	if (!error) {
		//移动的字符数
		int copy_len = 0;
		while (bytes_transfered > 0) {
			if (!_b_head_parse) {
				// 收到的数据比头部还少
				if (bytes_transfered + _rev_head_node->cur_len < HEAD_TOTAL_LEN) {
					memcpy(_rev_head_node->_data + _rev_head_node->cur_len, _data + copy_len, bytes_transfered);
					_rev_head_node->cur_len += bytes_transfered;
					::memset(_data, 0, MAX_LENGTH);
					_socket.async_read_some(boost::asio::buffer(_data, MAX_LENGTH),
						std::bind(&Session::HandleRead, this, std::placeholders::_1, std::placeholders::_2, _self_shared));
					return;
				}

				// 比头部多, 继续上次的复制剩余的长度
				int head_remain = HEAD_TOTAL_LEN - _rev_head_node->cur_len;
				memcpy(_rev_head_node->_data + _rev_head_node->cur_len, _data + copy_len, head_remain);
				// 更新已经处理的字符数和剩余未处理的函数
				copy_len += head_remain;
				bytes_transfered -= head_remain;
				// 获取消息id
				short msg_id = 0;
				memcpy(&msg_id, _rev_head_node->_data, HEAD_ID_LEN);
				msg_id = boost::asio::detail::socket_ops::network_to_host_short(msg_id);
				if (msg_id > MAX_LENGTH) {
					std::cout << "invalid msg_id!" << std::endl;
					_server->ClearSession(_uuid);
					return;
				}

				// 获取信息体的长度
				short data_len = 0;
				memcpy(&data_len, _rev_head_node->_data+HEAD_ID_LEN, HEAD_LENGTH);
				// 将网络字节序转换为本地字节序(注: 服务器也是"主机")
				data_len = boost::asio::detail::socket_ops::network_to_host_short(data_len);
				// 信息体的长度非法
				if (data_len > MAX_LENGTH) {
					std::cout << "invalid data length is: " << data_len << std::endl;
					_server->ClearSession(_uuid);
					return;
				}
				_rev_msg_node = std::make_shared<RecvNode>(data_len,msg_id);

				//如果接受的长度小于头部规定的长度, 就先把部分消息放进节点中
				if (bytes_transfered < data_len) {
					memcpy(_rev_msg_node->_data + _rev_msg_node->cur_len, _data + copy_len, bytes_transfered);
					_rev_msg_node->cur_len += bytes_transfered;
					::memset(_data, 0, MAX_LENGTH);
					_socket.async_read_some(boost::asio::buffer(_data, MAX_LENGTH),
						std::bind(&Session::HandleRead, this, std::placeholders::_1, std::placeholders::_2, _self_shared));
					// 更新关于头部信息
					_b_head_parse = true;
					return;
				}

				memcpy(_rev_msg_node->_data + _rev_msg_node->cur_len, _data + copy_len, data_len);
				copy_len += data_len;
				_rev_msg_node->cur_len += data_len;
				bytes_transfered -= data_len;
				_rev_msg_node->_data[_rev_msg_node->total_len] = '\0';
				//std::cout << "receive data is: " << _rev_msg_node->_data << std::endl;

				// 调用Send发送测试,
				// protobuf序列
				/*MsgData msgdata;
				msgdata.ParseFromString(std::string(_rev_msg_node->_data, _rev_msg_node->total_len));
				std::cout << "msgdata id is: " << msgdata.id() << ", data is: " << msgdata.data() << std::endl;
				std::string return_str = "Server received msgdata is: " + msgdata.data();
				MsgData msgreturn;
				msgreturn.set_id(msgdata.id());
				msgreturn.set_data(return_str);
				msgreturn.SerializePartialToString(&return_str);*/

				// json序列
				Json::Reader reader;
				Json::Value root;
				reader.parse(std::string(_rev_msg_node->_data, _rev_msg_node->total_len), root);
				std::cout << "receive msg id is: " << root["id"].asInt() << "receive msg data is: " << root["data"].asString() << std::endl;
				root["data"] = "server has received msg, msg data is: " + root["data"].asString();
				std::string return_str = root.toStyledString();
				Send(return_str, root["id"].asInt());

				// 继续轮询未处理完的数据
				_b_head_parse = false;
				_rev_head_node->Clear();
				if (bytes_transfered <= 0) {
					::memset(_data, 0, MAX_LENGTH);
					_socket.async_read_some(boost::asio::buffer(_data, MAX_LENGTH),
						std::bind(&Session::HandleRead, this, std::placeholders::_1, std::placeholders::_2, _self_shared));
					return;
				}
				continue;
			}

			// 已经处理完头部信息, 继续处理上次未接收完的消息数据
			// 接收的数据仍不足以处理剩余数据
			int remain_msg = _rev_msg_node->total_len - _rev_msg_node->cur_len;
			if (bytes_transfered < remain_msg) {
				memcpy(_rev_msg_node->_data + _rev_msg_node->cur_len, _data + copy_len, bytes_transfered);
				_rev_msg_node->cur_len += bytes_transfered;
				::memset(_data, 0, MAX_LENGTH);
				_socket.async_read_some(boost::asio::buffer(_data, MAX_LENGTH),
					std::bind(&Session::HandleRead, this, std::placeholders::_1, std::placeholders::_2, _self_shared));
				return;
			}

			memcpy(_rev_msg_node->_data + _rev_msg_node->cur_len, _data + copy_len, remain_msg);
			_rev_msg_node->cur_len += remain_msg;
			copy_len += remain_msg;
			bytes_transfered -= remain_msg;
			_rev_msg_node->_data[_rev_msg_node->total_len] = '\0';
			//std::cout << "receive data is " << _rev_msg_node->_data << std::endl;

			// protobuf序列
			/*MsgData msgdata;
			msgdata.ParseFromArray(_rev_msg_node->_data, _rev_msg_node->total_len);
			std::string return_str= "Server has received msg, msgdata is " + msgdata.data();
			MsgData msgreturn;
			msgreturn.set_id(msgdata.id());
			msgreturn.set_data(return_str);
			msgreturn.SerializeToString(&return_str);*/

			// json序列
			Json::Reader reader;
			Json::Value root;
			reader.parse(std::string(_rev_msg_node->_data, _rev_msg_node->total_len), root);
			std::cout << "received msg id is: " << root["id"].asInt() << ", received msg is: " << root["data"].asString() << std::endl;
			root["data"] = "Server has received msg, msg data is: " + root["data"].asString();
			std::string return_str = root.toStyledString();
			Send(return_str, root["id"].asInt());

			//继续之前的轮询
			_b_head_parse = false;
			_rev_head_node->Clear();
			if (bytes_transfered <= 0) {
				::memset(_data, 0, MAX_LENGTH);
				_socket.async_read_some(boost::asio::buffer(_data, MAX_LENGTH),
					std::bind(&Session::HandleRead, this, std::placeholders::_1, std::placeholders::_2, _self_shared));
				return;
			}
			continue;
		}
	}
	else {
		std::cout << "handle read failed, error is: " << error.what() << std::endl;
		Close();
		_server->ClearSession(_uuid);
	}
}

//写(操作)挂起, 从缓冲区中读取数据
void Session::HandleWrite(const boost::system::error_code& error, std::shared_ptr<Session> _self_shared)
{
	if (!error) {
		std::lock_guard<std::mutex> lock(_send_lock);
		_send_queue.pop();
		if (!_send_queue.empty()) {
			auto& msgnode = _send_queue.front();
			boost::asio::async_write(_socket, boost::asio::buffer(msgnode->_data, msgnode->total_len),
				std::bind(&Session::HandleWrite, this, std::placeholders::_1, _self_shared));
		}
	}
	else {
		std::cout << "write error!" << std::endl;
		//delete this;
		_server->ClearSession(_uuid);
	}
}