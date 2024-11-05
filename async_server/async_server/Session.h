#pragma once
#include<boost/asio.hpp>
#include<iostream>
using namespace boost::asio::ip;

class Session
{
public:
	Session(boost::asio::io_context& ioc) :_socket(ioc) {

	}
	tcp::socket& Socket() {
		return _socket;
	}

	//链接开始, 调用读函数
	void Start();

private:
	//回调函数, 调用写函数
	void handle_read(const boost::system::error_code& error, size_t bytes_transfered);
	void handle_write(const boost::system::error_code& error);
	enum { max_length = 1024 };
	tcp::socket _socket;
	char _data[max_length];
};

class Server {
public:
	Server(boost::asio::io_context& ioc, short port);

private:
	void start_accept();
	void handle_accept(Session* new_session, const boost::system::error_code& error);
	boost::asio::io_context& _ioc;
	tcp::acceptor _acceptor;
};