#pragma once
#include<boost/asio.hpp>
#include<iostream>
#include"Session.h"
#include<map>
using namespace boost::asio::ip;
class CServer
{
public:
	CServer(boost::asio::io_context& ioc, short port);

	void ClearSession(std::string uuid);

private:
	void StartAccept();
	void HandleAccept(std::shared_ptr<Session> new_session, const boost::system::error_code& error);
	boost::asio::io_context& _ioc;
	short port;
	tcp::acceptor _acceptor;
	//管理创建的会话
	std::map<std::string, std::shared_ptr<Session>> _sessions;
};