#include <iostream>
#include "CServer.h"

int main()
{
    try {
        boost::asio::io_context  io_context;
        CServer s(io_context, 5283);
        io_context.run();
    }
    catch (std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
    }
    boost::asio::io_context io_context;
}