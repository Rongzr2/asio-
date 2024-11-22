#include "Session.h"
#include"CServer.h"

void Session::Start()
{
	memset(_data, 0, max_length);
	//注意bind绑定的第五个参数
	_socket.async_read_some(boost::asio::buffer(_data, max_length),
		std::bind(&Session::HandleRead, this, std::placeholders::_1, std::placeholders::_2,shared_from_this()));
}

void Session::Send(char* msg, int max_length)
{
	//判断是否发送完毕
	bool pending = false;
	std::lock_guard<std::mutex> lock(_send_lock);
	if (_send_queue.size() > 0) {
		pending = true;
	}
	_send_queue.push(std::make_shared<MsgNode>(msg, max_length));   //入队
	if (pending) {
		return;
	}

	//发送队列为空，则说明当前没有未发送完的数据，将要发送的数据放入队列并调用async_write函数发送数据
	boost::asio::async_write(_socket, boost::asio::buffer(msg, max_length),
		std::bind(&Session::HandleWrite, this, std::placeholders::_1, shared_from_this()));
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
				if (bytes_transfered + _rev_head_node->cur_len < HEAD_LENGTH) {
					memcpy(_rev_head_node->_data + _rev_head_node->cur_len, _data + copy_len, bytes_transfered);
					_rev_head_node->cur_len += bytes_transfered;
					::memset(_data, 0, MAX_LENGTH);
					_socket.async_read_some(boost::asio::buffer(_data, MAX_LENGTH),
						std::bind(&Session::HandleRead, this, std::placeholders::_1, std::placeholders::_2, _self_shared));
					return;
				}

				// 比头部多, 继续上次的复制剩余的长度
				int head_remain = HEAD_LENGTH - _rev_head_node->cur_len;
				memcpy(_rev_head_node->_data + _rev_head_node->cur_len, _data + copy_len, head_remain);
				// 更新已经处理的字符数和剩余未处理的函数
				copy_len += head_remain;
				bytes_transfered -= head_remain;
				// 获取信息体的长度
				short data_len = 0;
				memcpy(&data_len, &_rev_head_node->_data, HEAD_LENGTH);
				// 信息体的长度非法
				if (data_len > MAX_LENGTH) {
					std::cout << "invalid data length is: " << data_len << std::endl;
					_server->ClearSession(_uuid);
					return;
				}
				_rev_msg_node = std::make_shared<MsgNode>(data_len);

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
				std::cout << "receive data is: " << _rev_msg_node->_data << std::endl;
				// 调用Send发送测试
				Send(_rev_msg_node->_data, _rev_msg_node->total_len);
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
			std::cout << "receive data is " << _rev_msg_node->_data << std::endl;
			Send(_rev_msg_node->_data, _rev_msg_node->total_len);
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