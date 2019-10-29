#include "../src/connection_manager.h"
#include <iostream>
#include <unistd.h>

int32_t DoRead(const Connection &client)
{
    char buffer[1024];
    int32_t read_num = read(client.fd, buffer, 1024);
    if (read_num > 0)
    {
        std::cout << "read_num[" << read_num << "]" << std::endl;
    }
    if (read_num == 0)
    {
        exit(0);
    }
    return read_num;
}

void WaitAWhile()
{
	timespec t;
	t.tv_sec = 0;
	t.tv_nsec = 1000 * 1000;
	nanosleep(&t, NULL);
}

int main()
{
    ConnectionManager mgr;
    const Connection *client = mgr.ConnectTo("127.0.0.1", 18889);
    if (client == nullptr) return -1;
    while (true) 
    {
        if(DoRead(*client) == -1)
        {
            WaitAWhile();
        }
    }
}
