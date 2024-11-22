#include"Session.h"
#include"CServer.h"

int main()
{
    try {
        boost::asio::io_context ioc;
        CServer(ioc, 5283);
        ioc.run();
    }
    catch (std::exception& e) {
        std::cout << "Exception: " << e.what() << std::endl;
    }

    return 0;
}