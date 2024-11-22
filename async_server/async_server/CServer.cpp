#include "CServer.h"

CServer::CServer(boost::asio::io_context& ioc, short port) :_ioc(ioc),
_acceptor(ioc, tcp::endpoint(tcp::v4(), port)) {
	std::cout << "Server Start sucess, is from " << port << std::endl;
	StartAccept();
}

void CServer::ClearSession(std::string uuid)
{
	_sessions.erase(uuid);
}

//将acceptor对应的socket绑定到epoll模型上, 实现事件驱动, 并触发handle_accept回调函数
void CServer::StartAccept()
{
	std::shared_ptr<Session> new_session = std::make_shared<Session>(_ioc, this);
	// _acceptor.async_accept(new_session->GetSocket(),
	//		std::bind(&CServer::HandleAccept, this, new_session, std::placeholders::_1));
	_acceptor.async_accept(new_session->GetSocket(),
		[this, new_session](const boost::system::error_code& error) {
			HandleAccept(new_session, error);
		});
}

//回调函数
void CServer::HandleAccept(std::shared_ptr<Session> new_session, const boost::system::error_code& error)
{
	if (!error) {
		new_session->Start();
		_sessions.insert((std::make_pair(new_session->Getuuid(), new_session)));
	}
	else {
		std::cout << "session accept failed, error is " << error.what() << std::endl;
	}

	StartAccept();
}