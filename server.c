#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <string.h>
#include <sys/types.h>
#include <sys/epoll.h>
#include <stdio.h>
#include <errno.h>
#include "clientinfo.h"
#include "message.h"


#define FIFO_1 "/tmp/server_fifo1"
#define FIFO_2 "/tmp/server_fifo2"
#define FIFO_3 "/tmp/server_fifo3"
#define MAX_EVENT_NUMBER 1000
#define MAX_USER_NUMBER 100
#define BUFFER_SIZE 100

void handler(int sig){
    unlink(FIFO_1);
    unlink(FIFO_2);
    unlink(FIFO_3);
    exit(1);
}

void addfd(int epollfd,int fd){
    struct epoll_event ev;
    ev.data.fd = fd;
    ev.events = EPOLLIN | EPOLLRDHUP;
    if (epoll_ctl(epollfd,EPOLL_CTL_ADD,fd,&ev)==-1){
        perror("epoll_ctl error");
    }
}
int main(){
    int res,user_num=0;
    int epoll_fd,event_fd;
    struct epoll_event ev , events[MAX_EVENT_NUMBER];
    int fifo1_fd,fifo2_fd,fifo3_fd;
    CLIENTINFO info[MAX_USER_NUMBER];
    char buffer[BUFFER_SIZE];


    if(signal(SIGINT, handler)==SIG_ERR){
        perror("signal error");
    }
    if(signal(SIGTERM, handler)==SIG_ERR){
        perror("signal error");
    }


    if (access(FIFO_1,F_OK) == -1){
        res = mkfifo(FIFO_1,0777);
        if (res == -1){
            printf("FIFO %s was not created\n",FIFO_1);
            exit(EXIT_FAILURE);
        }
    }
    if (access(FIFO_2,F_OK) == -1){
        res = mkfifo(FIFO_2,0777);
        if (res == -1){
            printf("FIFO %s was not created\n",FIFO_2);
            exit(EXIT_FAILURE);
        }
    }
    if (access(FIFO_3,F_OK) == -1){
        res = mkfifo(FIFO_3,0777);
        if (res == -1){
            printf("FIFO %s was not created\n",FIFO_3);
            exit(EXIT_FAILURE);
        }
    }


    fifo1_fd = open(FIFO_1,O_RDONLY | O_NONBLOCK);
    if (fifo1_fd == -1){
        printf("Could not open %s for read only access\n",FIFO_1);
        exit(EXIT_FAILURE);
    }

    fifo2_fd = open(FIFO_2,O_RDONLY | O_NONBLOCK);
    if (fifo2_fd == -1){
        printf("Could not open %s for read only access\n",FIFO_2);
        exit(EXIT_FAILURE);
    }

    fifo3_fd = open(FIFO_3,O_RDONLY | O_NONBLOCK);
    if (fifo3_fd == -1){
        printf("Could not open %s for read only access\n",FIFO_3);
        exit(EXIT_FAILURE);
    }

    printf("\nServer is ready to go!\n");

    //创建epoll句柄
    epoll_fd = epoll_create(5);
    if(epoll_fd == -1){
        perror("epoll error");
    }

    //注册内核事件
    addfd(epoll_fd,fifo1_fd);
    addfd(epoll_fd,fifo2_fd);
    addfd(epoll_fd,fifo3_fd);


    while (1){
        int num = epoll_wait(epoll_fd,events,MAX_EVENT_NUMBER,-1);
        if (num < 0 && errno != EINTR){
            printf("epoll failure");
            break;
        }
        for (int i = 0; i < num; ++i) {
            int fd = events[i].data.fd;
            if (fd == fifo1_fd){
                if (user_num >= MAX_USER_NUMBER){
                    printf("server's register was full!\n");
                    exit(EXIT_FAILURE);
                }
                int flag = 0;
                CLIENTINFO info1;
                res = read(fd,&info1, sizeof(CLIENTINFO));
                if (res != 0){
                    for (int j = 0; j < user_num; ++j) {
                        if(strcmp(info[j].username,info1.username) == 0){
                            printf("%s is exist!!\n",info[user_num].username);
                            sprintf(buffer,"register fall!!\n");
                            int fd1 = open(info1.my_fifo,O_WRONLY);
                            write(fd1,buffer, strlen(buffer)+1);
                            close(fd1);
                            flag = 1;
                            break;
                        }
                    }
                    if(flag){
                        continue;
                    }
                    strcpy(info[user_num].username,info1.username);
                    strcpy(info[user_num].my_fifo,info1.my_fifo);
                    strcpy(info[user_num].password,info1.password);
                    info[user_num].login = 0;
                    memset(buffer,'\0',BUFFER_SIZE);
                    printf("%s register successfully!!\n",info[user_num].username);
                    sprintf(buffer,"Successfully register!!\n");
                    int fd1 = open(info[user_num].my_fifo,O_WRONLY);
                    write(fd1,buffer, strlen(buffer)+1);
                    close(fd1);
                    user_num+=1;
                }
            }
            else if (fd == fifo2_fd){
                CLIENTINFO info1;
                int flag = 0;
                res = read(fd,&info1, sizeof(CLIENTINFO));
                if (res != 0){
                    for (int j = 0; j < user_num; ++j) {
                        if (strcmp(info1.username,info[j].username) == 0 && strcmp(info1.password , info[j].password) == 0){
                            info[j].login = 1;
                            printf("%s Successfully login!!\n",info1.username);
                            memset(buffer,'\0',BUFFER_SIZE);
                            sprintf(buffer,"Successfully login!!\n");
                            int fd1 = open(info[j].my_fifo,O_WRONLY | O_NONBLOCK);
                            write(fd1,buffer, strlen(buffer)+1);
                            close(fd1);
                            flag = 1;
                            break;
                        }
                    }
                    if(flag == 0){
                        printf("lose login!\n");
                    }
                }
            }
            else if (fd == fifo3_fd){
                MESSAGE message;
                res = read(fd,&message, sizeof(MESSAGE));
                if (res != 0){
                    for (int j = 0; j < user_num; ++j) {
                        if (strcmp(message.username,info[j].username) == 0){
                            if(info[j].login == 0){
                                printf("%s is not online!\n",message.username);
                                break;
                            }
                            int fd1 = open(info[j].my_fifo,O_WRONLY);
                            write(fd1,message.message_buff, strlen(message.message_buff)+1);
                            printf("Successfully send message!!\n");
                            close(fd1);
                            break;
                        }
                    }
                }
            }
        }
    }

}
