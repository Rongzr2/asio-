#include "Session.h"

void Session::Start()
{
	memset(_data, 0, max_length);
	_socket.async_read_some(boost::asio::buffer(_data, max_length),
		std::bind(&Session::handle_read, this, std::placeholders::_1, std::placeholders::_2));
}

//读(操作)挂起, 异步写发送给对端
void Session::handle_read(const boost::system::error_code& error, size_t bytes_transfered)
{
	if (!error) {
		std::cout << "service receive data is: ";
		boost::asio::async_write(_socket, boost::asio::buffer(_data, bytes_transfered),
			std::bind(&Session::handle_write, this, std::placeholders::_1));
	}
	else {
		std::cout << "read error" << std::endl;
		delete this;
	}
}

//写(操作)挂起, 从缓冲区中读取数据
void Session::handle_write(const boost::system::error_code& error)
{
	if (!error) {
		memset(_data, 0, max_length);
		_socket.async_read_some(boost::asio::buffer(_data, max_length),
			std::bind(&Session::handle_read, this, std::placeholders::_1, std::placeholders::_2));
	}
	else {
		std::cout << "write error!" << std::endl;
		delete this;
	}
}

Server::Server(boost::asio::io_context& ioc, short port) :_ioc(ioc),
_acceptor(ioc, tcp::endpoint(tcp::v4(), port)) {
	std::cout << "Server Start sucess, is from " << port << std::endl;
	start_accept();
}

void Server::start_accept()
{
	Session* new_session = new Session(_ioc);
	_acceptor.async_accept(new_session->Socket(),
		std::bind(&Server::handle_accept, this, new_session, std::placeholders::_1));
}

void Server::handle_accept(Session* new_session, const boost::system::error_code& error)
{
	if (!error) {
		new_session->Socket();
	}
	else {
		delete new_session;
	}

	start_accept();
}
