#include <iostream>
#include <unistd.h>

void DoSend(const Connection &client)
{
    static int count = 100000;
    if (count == 0) return;
	char buff[2048] = "abc";
	int write_num = write(client.fd, buff, 1024);
    if (write_num == 0) 
    {
        exit(0);
    }
    else if (write_num == -1)
    {
        std::cout << "errno[" << errno << "]" << std::endl; 
    }
    else
    {
        --count;
    }
    
	std::cout <<"write_num[" << write_num << "]" << std::endl; 
}

void WaitAWhile()
{
	timespec t;
	t.tv_sec = 0;
	t.tv_nsec = 10000;
	nanosleep(&t, NULL);
}

int main()
{
    ConnectionManager mgr;
    const Connection *client = mgr.ConnectTo("127.0.0.1", 18889);
    if (client == nullptr) return -1;
    while (true) 
    {
        DoSend(*client);
        WaitAWhile();
        //DoRead(*client);
    }
}
