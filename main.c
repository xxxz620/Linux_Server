#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>

int main()
{
    char buf[100] = "helloworld!";
    int fd = open("xtz_register",O_WRONLY | O_NONBLOCK);
    write(fd,buf, sizeof(buf));
    close(fd);
}
