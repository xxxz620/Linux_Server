//
// Created by xietangzhen on 2024-06-04.
//
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <fcntl.h>
#include <pthread.h>
#include <errno.h>
#include <sys/select.h>
#include "clientinfo.h"
#include "message.h"
#include "client_message.h"

char  reg_fifo[100] = "/";
char  login_fifo[100] = "/";
char  msg_fifo[100] = "/";
char  logout_fifo[100] = "/";

client_info clientInfo1;

void read_server_conf()
{
    FILE * file = fopen("server.conf","r");
    if(file == NULL)
    {
        perror("open server.conf error!");
    }
    char line[256];
    while(fgets(line, sizeof(line),file))
    {
        char * key = strtok(line,"=");
        char * value = strtok(NULL,"\n");
        if(strcmp(key,"REG_FIFO") == 0)
        {
            strcpy(reg_fifo+1,value);
        }
        else if(strcmp(key,"LOGIN_FIFO") == 0)
        {
            strcpy(login_fifo+1,value);
        }
        else if(strcmp(key,"MSG_FIFO") == 0)
        {
            strcpy(msg_fifo+1,value);
        }
        else if(strcmp(key,"LOGOUT_FIFO") == 0)
        {
            strcpy(logout_fifo+1,value);
        }
    }
    fclose(file);
}

void handle_Display()
{
    int fd = open(clientInfo1.pri_fifo, O_RDONLY | O_NONBLOCK);
    if(fd == -1)
    {
        perror("open error");
        pthread_exit(NULL);
    }
    while (1)
    {
        int res;
        char buf[1000];
        res = read(fd, buf, sizeof(buf));
        if(res > 0 && strcmp(buf,"none_push") != 0 && strcmp(buf,"end_push") != 0){
            printf("%s",buf);
            fflush(stdout);
        }
    }
}

int main()
{
    read_server_conf();

    printf("please choose your choice:\n");
    printf("register account please input: 1\n");
    printf("login account please input: 2\n");
    printf("send message please input: 3\n");
    printf("logout account please input: 4\n");
    printf("if you input other number,client will close!\n");
    pthread_t p;
    while (1){

        int choice;
        scanf("%d",&choice);
        getchar();

        if(choice == 1)
        {
            //设置用户名和密码
            printf("please set your username: ");
            scanf("%s",clientInfo1.user_name);
            printf("please set your password: ");
            scanf("%s",clientInfo1.password);
            getchar();
            //设置私有管道
            sprintf(clientInfo1.pri_fifo,"/home/xietangzhen2021150058/final/%s",clientInfo1.user_name);
            if(access(clientInfo1.pri_fifo,F_OK) == -1)
            {
                if(mkfifo(clientInfo1.pri_fifo,0666) < 0){
                    printf("mkfifo error!\n");
                    exit(EXIT_FAILURE);
                }
            }
            //通过众所周知管道将结构体写给服务器
            int fd = open(reg_fifo, O_WRONLY | O_NONBLOCK);
            if(fd == -1)
            {
                perror("open");
            }
            if(write(fd,&clientInfo1, sizeof(client_info)) == -1)
            {
                if (errno == EAGAIN) {
                    // 管道已满，处理非阻塞写入失败的情况
                    fprintf(stderr, "Pipe is full, try again later.\n");
                } else {
                    perror("write");
                }
            }
            close(fd);

            //读取私有管道内容并打印在屏显
            pthread_create(&p,NULL,(void*)handle_Display,NULL);
            sleep(1);
        }

        else if(choice == 2)
        {
            //输入登录账号和密码
            client_info clientInfo;
            printf("please input your username: ");
            scanf("%s",clientInfo.user_name);
            printf("please input your password: ");
            scanf("%s",clientInfo.password);
            getchar();

            int fd = open(login_fifo,O_WRONLY | O_NONBLOCK);
            if(fd == -1)
            {
                perror("open");
            }
            write(fd,&clientInfo, sizeof(client_info));
            close(fd);

            sleep(1);
        }
        else if(choice == 3)
        {
            client_message c_m;
            printf("please input target: ");
            char target[100];
            char msg[100];
            fgets(target,sizeof(target),stdin);
            printf("please input message: \n");
            fgets(msg, sizeof(msg),stdin);
            strncpy(c_m.m.msg,msg,strlen(msg)-1);

            size_t len = strlen(target);
            if (len > 0 && target[len-1] == '\n')
            {
                target[len-1] = '\0';
            }
            char* token = strtok(target," ");
            while(token != NULL)
            {
                strcpy(c_m.m.target,token);
                token = strtok(NULL," ");

                strcpy(c_m.c.pri_fifo,clientInfo1.pri_fifo);
                strcpy(c_m.c.user_name,clientInfo1.user_name);
                strcpy(c_m.c.password,clientInfo1.password);

                //将信息打包成结构体传输给服务器
                int fd = open(msg_fifo,O_WRONLY | O_NONBLOCK);
                if(fd == -1)
                {
                    perror("open");
                }
                write(fd,&c_m, sizeof(client_message));
                close(fd);

                sleep(1);
            }
        }
        else if(choice == 4)
        {
            int fd = open(logout_fifo,O_WRONLY | O_NONBLOCK);
            if(fd == -1)
            {
                perror("open");
            }
            write(fd,&clientInfo1, sizeof(client_info));
            close(fd);

            sleep(1);
        }
        else
        {
            printf("client close!\n");
            exit(0);
        }
    }
}