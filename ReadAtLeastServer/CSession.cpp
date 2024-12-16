#include "CSession.h"
#include "CServer.h"
#include <iostream>

CSession::CSession(boost::asio::io_context& io_context, CServer* server) :
	_socket(io_context), _server(server), _b_close(false), _b_head_parse(false) {
	boost::uuids::uuid  a_uuid = boost::uuids::random_generator()();
	_uuid = boost::uuids::to_string(a_uuid);
	_recv_head_node = make_shared<MsgNode>(HEAD_LENGTH);
}
CSession::~CSession() {
	cout << "~CSession destruct" << endl;
}

tcp::socket& CSession::GetSocket() {
	return _socket;
}

std::string& CSession::GetUuid() {
	return _uuid;
}

void CSession::Start() {

	/*::memset(_data, 0, MAX_LENGTH);
	_socket.async_read_some(boost::asio::buffer(_data, MAX_LENGTH), std::bind(&CSession::HandleRead, this,
		std::placeholders::_1, std::placeholders::_2, SharedSelf()));*/
	//_recv_head_node = std::make_shared<MsgNode>(HEAD_LENGTH);
	// 在构造函数里new _recv_head_node
	_recv_head_node->Clear();
	boost::asio::async_read(_socket, boost::asio::buffer(_recv_head_node->_data, _recv_head_node->_total_len),
		std::bind(&CSession::HandleReadHead, this, std::placeholders::_1, std::placeholders::_2, SharedSelf()));
	/*boost::asio::async_read(_socket, boost::asio::buffer(_recv_head_node->_data, _recv_head_node->_total_len),
		[this, self = SharedSelf()](const boost::system::error_code& ec, size_t byte_transfered) {
			HandleReadHead(ec, byte_transfered, self);
		});*/
}

void CSession::Send(char* msg, int max_length) {
	std::lock_guard<std::mutex> lock(_send_lock);
	int send_que_size = _send_que.size();
	if (send_que_size > MAX_SENDQUE) {
		cout << "session: " << _uuid << " send que fulled, size is " << MAX_SENDQUE << endl;
		return;
	}

	_send_que.push(make_shared<MsgNode>(msg, max_length));
	if (send_que_size > 0) {
		return;
	}
	auto& msgnode = _send_que.front();
	boost::asio::async_write(_socket, boost::asio::buffer(msgnode->_data, msgnode->_total_len),
		std::bind(&CSession::HandleWrite, this, std::placeholders::_1, SharedSelf()));
}

void CSession::Close() {
	_socket.close();
	_b_close = true;
}

std::shared_ptr<CSession>CSession::SharedSelf() {
	return shared_from_this();
}

void CSession::HandleWrite(const boost::system::error_code& error, std::shared_ptr<CSession> shared_self) {

	if (!error) {
		std::lock_guard<std::mutex> lock(_send_lock);
		cout << "send data " << _send_que.front()->_data + HEAD_LENGTH << endl;
		_send_que.pop();
		if (!_send_que.empty()) {
			auto& msgnode = _send_que.front();
			boost::asio::async_write(_socket, boost::asio::buffer(msgnode->_data, msgnode->_total_len),
				std::bind(&CSession::HandleWrite, this, std::placeholders::_1, shared_self));
		}
	}
	else {
		std::cout << "handle write failed, error is " << error.what() << endl;
		Close();
		_server->ClearSession(_uuid);
	}
}

void CSession::PrintRecvData(char* _data, size_t bytes_transferred)
{
	std::cout << "msg is " << _data << " ,the length is " << bytes_transferred << std::endl;
}

void CSession::HandleReadHead(const boost::system::error_code& error, size_t bytes_transferred, std::shared_ptr<CSession> shared_self)
{
	if (!error) {
		if (bytes_transferred < HEAD_LENGTH) {
			std::cout << "receive occured error!" << std::endl;
			Close();
			_server->ClearSession(_uuid);
			return;
		}

		memcpy(_recv_head_node->_data, _data, HEAD_LENGTH);
		short data_len = 0;
		memcpy(&data_len, _recv_head_node->_data, HEAD_LENGTH);

		if (data_len > MAX_LENGTH) {
			std::cout << "msg is too long!" << std::endl;
			_server->ClearSession(_uuid);
			return;
		}

		_recv_msg_node = std::make_shared<MsgNode>(data_len);
		boost::asio::async_read(_socket, boost::asio::buffer(_recv_msg_node->_data, _recv_head_node->_total_len),
			std::bind(&CSession::HandleReadMsg, this, std::placeholders::_1, std::placeholders::_2, SharedSelf()));
	}
	else {
		std::cout << "handle read failed, error is " << error.what() << endl;
		Close();
		_server->ClearSession(_uuid);
	}
}

void CSession::HandleReadMsg(const boost::system::error_code& error, size_t bytes_transferred, std::shared_ptr<CSession> shared_self) 
{
	if (!error) {
		PrintRecvData(_data, bytes_transferred);
		std::chrono::milliseconds dura(2000);
		std::this_thread::sleep_for(dura);

		_recv_msg_node->_data[_recv_msg_node->_total_len] = '\0';
		std::cout << "received msg is" << _recv_msg_node->_data << std::endl;
		Send(_recv_msg_node->_data, _recv_msg_node->_total_len);

		// 再挂起读头事件, 接受头部事件
		_recv_head_node->Clear();
		boost::asio::async_read(_socket, boost::asio::buffer(_recv_msg_node->_data, _recv_msg_node->_total_len),
			[this, self = SharedSelf()](const boost::system::error_code& ec, size_t bytes_transferred) {
				HandleReadHead(ec, bytes_transferred, self);
			});
		/*boost::asio::async_read(_socket, boost::asio::buffer(_recv_head_node->_data, _recv_head_node->_total_len),
			std::bind(&CSession::HandleReadHead, this, std::placeholders::_1, std::placeholders::_2, SharedSelf()));*/
	}
	else {
		cout << "handle read msg failed,  error is " << error.what() << endl;
		Close();
		_server->ClearSession(_uuid);
	}
}

//   复杂的粘包处理方法
//void CSession::HandleRead(const boost::system::error_code& error, size_t  bytes_transferred, std::shared_ptr<CSession> shared_self) {
//	if (!error) {
//		//已经移动的字符数
//		int copy_len = 0;
//		while (bytes_transferred > 0) {
//			if (!_b_head_parse) {
//				//收到的数据不足头部大小
//				if (bytes_transferred + _recv_head_node->_cur_len < HEAD_LENGTH) {
//					memcpy(_recv_head_node->_data + _recv_head_node->_cur_len, _data + copy_len, bytes_transferred);
//					_recv_head_node->_cur_len += bytes_transferred;
//					::memset(_data, 0, MAX_LENGTH);
//					_socket.async_read_some(boost::asio::buffer(_data, MAX_LENGTH),
//						std::bind(&CSession::HandleRead, this, std::placeholders::_1, std::placeholders::_2, shared_self));
//					return;
//				}
//				//收到的数据比头部多
//				//头部剩余未复制的长度
//				int head_remain = HEAD_LENGTH - _recv_head_node->_cur_len;
//				memcpy(_recv_head_node->_data + _recv_head_node->_cur_len, _data + copy_len, head_remain);
//				//更新已处理的data长度和剩余未处理的长度
//				copy_len += head_remain;
//				bytes_transferred -= head_remain;
//				//获取头部数据
//				short data_len = 0;
//				memcpy(&data_len, _recv_head_node->_data, HEAD_LENGTH);
//				//网络字节序转化为本地字节序
//				data_len = boost::asio::detail::socket_ops::network_to_host_short(data_len);
//				cout << "data_len is " << data_len << endl;
//				//头部长度非法
//				if (data_len > MAX_LENGTH) {
//					std::cout << "invalid data length is " << data_len << endl;
//					_server->ClearSession(_uuid);
//					return;
//				}
//				_recv_msg_node = make_shared<MsgNode>(data_len);
//
//				//消息的长度小于头部规定的长度，说明数据未收全，则先将部分消息放到接收节点里
//				if (bytes_transferred < data_len) {
//					memcpy(_recv_msg_node->_data + _recv_msg_node->_cur_len, _data + copy_len, bytes_transferred);
//					_recv_msg_node->_cur_len += bytes_transferred;
//					::memset(_data, 0, MAX_LENGTH);
//					_socket.async_read_some(boost::asio::buffer(_data, MAX_LENGTH),
//						std::bind(&CSession::HandleRead, this, std::placeholders::_1, std::placeholders::_2, shared_self));
//					//头部处理完成
//					_b_head_parse = true;
//					return;
//				}
//
//				memcpy(_recv_msg_node->_data + _recv_msg_node->_cur_len, _data + copy_len, data_len);
//				_recv_msg_node->_cur_len += data_len;
//				copy_len += data_len;
//				bytes_transferred -= data_len;
//				_recv_msg_node->_data[_recv_msg_node->_total_len] = '\0';
//				cout << "receive data is " << _recv_msg_node->_data << endl;
//				//此处可以调用Send发送测试
//				Send(_recv_msg_node->_data, _recv_msg_node->_total_len);
//				//继续轮询剩余未处理数据
//				_b_head_parse = false;
//				_recv_head_node->Clear();
//				if (bytes_transferred <= 0) {
//					::memset(_data, 0, MAX_LENGTH);
//					_socket.async_read_some(boost::asio::buffer(_data, MAX_LENGTH),
//						std::bind(&CSession::HandleRead, this, std::placeholders::_1, std::placeholders::_2, shared_self));
//					return;
//				}
//				continue;
//			}
//
//			//已经处理完头部，处理上次未接受完的消息数据
//			//接收的数据仍不足剩余未处理的
//			int remain_msg = _recv_msg_node->_total_len - _recv_msg_node->_cur_len;
//			if (bytes_transferred < remain_msg) {
//				memcpy(_recv_msg_node->_data + _recv_msg_node->_cur_len, _data + copy_len, bytes_transferred);
//				_recv_msg_node->_cur_len += bytes_transferred;
//				::memset(_data, 0, MAX_LENGTH);
//				_socket.async_read_some(boost::asio::buffer(_data, MAX_LENGTH),
//					std::bind(&CSession::HandleRead, this, std::placeholders::_1, std::placeholders::_2, shared_self));
//				return;
//			}
//			memcpy(_recv_msg_node->_data + _recv_msg_node->_cur_len, _data + copy_len, remain_msg);
//			_recv_msg_node->_cur_len += remain_msg;
//			bytes_transferred -= remain_msg;
//			copy_len += remain_msg;
//			_recv_msg_node->_data[_recv_msg_node->_total_len] = '\0';
//			cout << "receive data is " << _recv_msg_node->_data << endl;
//			//此处可以调用Send发送测试
//			Send(_recv_msg_node->_data, _recv_msg_node->_total_len);
//			//继续轮询剩余未处理数据
//			_b_head_parse = false;
//			_recv_head_node->Clear();
//			if (bytes_transferred <= 0) {
//				::memset(_data, 0, MAX_LENGTH);
//				_socket.async_read_some(boost::asio::buffer(_data, MAX_LENGTH),
//					std::bind(&CSession::HandleRead, this, std::placeholders::_1, std::placeholders::_2, shared_self));
//				return;
//			}
//			continue;
//		}
//	}
//	else {
//		std::cout << "handle read failed, error is " << error.what() << endl;
//		Close();
//		_server->ClearSession(_uuid);
//	}
//}