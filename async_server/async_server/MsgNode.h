#pragma once
#include <iostream>
#include "const.h"
#include <boost/asio.hpp>

class MsgNode
{
public:
	MsgNode(short max_len) :total_len(max_len), cur_len(0) {
		_data = new char[total_len + 1]();
		_data[total_len] = '\0';
	}

	~MsgNode() {
		std::cout << "destruct MsgNode" << std::endl;
		delete[] _data;
	}

	void Clear() {
		::memset(_data, '\0', total_len);
		cur_len = 0;
	}

public:
	short cur_len;
	short total_len;
	char* _data;
};

class RecvNode :public MsgNode {
public:
	RecvNode(short max_len, short msg_id);

public:
	short msg_id;
};

class SendNode :public MsgNode {
public:
	SendNode(const char* msg, short max_len, short msg_id);

public:
	short msg_id;
};