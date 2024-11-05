#include"Session.h"

int main()
{
    try {
        boost::asio::io_context ioc;
        Server(ioc, 5283);
        ioc.run();
    }
    catch (std::exception& e) {
        std::cout << "Exception: " << e.what() << std::endl;
    }
    return 0;
}