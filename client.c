#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <fcntl.h>
#include <signal.h>
#include "clientinfo.h"
#include "message.h"

#define FIFO_1 "/tmp/server_fifo1"
#define FIFO_2 "/tmp/server_fifo2"
#define FIFO_3 "/tmp/server_fifo3"
#define BUFF_SZ 100
#define USER_SZ 50
#define MESG_SZ 1000

char mypipename[BUFF_SZ];

void handler(int sig){
    unlink(mypipename);
    exit(1);
}

int main(){
    int res,op;
    int fifo_fd1,fifo_fd2,fifo_fd3 ,my_fifo;
    int fd;
    char buffer[BUFF_SZ];
    char username[USER_SZ];
    char password[USER_SZ];
    char target_user[USER_SZ];
    char message[MESG_SZ];

    /*handle some signal*/

    if(signal(SIGINT, handler)==SIG_ERR){
        perror("signal error!");
    }
    if(signal(SIGTERM,handler)==SIG_ERR){
        perror("signal error!");
    }


    while(1){

        scanf("%d",&op);
        if(op == 1){
            /* check if server fifo exists */
            if (access(FIFO_1,F_OK) == -1){
                printf("Could not open FIFO %s\n",FIFO_1);
                exit(EXIT_FAILURE);
            }
            /* open server fifo for write */
            fifo_fd1 = open(FIFO_1,O_WRONLY | O_NONBLOCK);
            if(fifo_fd1 == -1){
                printf("Could not open %s for write access\n",FIFO_1);
                exit(EXIT_FAILURE);
            }

            CLIENTINFO info;
            printf("please input your username :\n");
            scanf("%s",username);
            printf("please input your password :\n");
            scanf("%s",password);

            /* create my own FIFO */
            sprintf(mypipename,"/tmp/chat_client%d_fifo",getpid());
            res = mkfifo(mypipename,0777);
            if (res != 0){
                printf("FIFO %s was not created\n",mypipename);
                exit(EXIT_FAILURE);
            }

            /* construct client info */
            strcpy(info.my_fifo, mypipename);
            strcpy(info.username, username);
            strcpy(info.password , password);
            info.login = 0;

            write(fifo_fd1,&info, sizeof(CLIENTINFO));
            close(fifo_fd1);

            my_fifo = open(mypipename,O_RDONLY);
            if(my_fifo == -1){
                printf("Could not open %s for read only access\n",mypipename);
                exit(EXIT_FAILURE);
            }
            /* get result fron server */
            memset(buffer, '\0', BUFF_SZ);

            res = read(my_fifo,buffer,BUFF_SZ);
            if(res >0){
                printf("receive message from server: %s\n",buffer);
            }
            close(my_fifo);

        }
        else if(op == 2){
            //判断管道文件是否存在
            if(access(FIFO_2,F_OK) == -1){
                printf("Could not open FIFO %s\n",FIFO_2);
                exit(EXIT_FAILURE);
            }
            //打开管道
            fifo_fd2 = open(FIFO_2,O_WRONLY | O_NONBLOCK);
            if(fifo_fd2 == -1){
                printf("Could not open %s for write access\n",FIFO_2);
                exit(EXIT_FAILURE);
            }

            CLIENTINFO info;
            printf("please input your username :\n");
            scanf("%s",username);
            printf("please input your password :\n");
            scanf("%s",password);

            strcpy(info.username , username);
            strcpy(info.password , password);
            info.login = 1;

            write(fifo_fd2,&info, sizeof(CLIENTINFO));
            close(fifo_fd2);

            my_fifo = open(mypipename,O_RDONLY);
            if(my_fifo == -1){
                printf("Could not open %s for read only access\n",mypipename);
                exit(EXIT_FAILURE);
            }
            /* get result fron server */
            memset(buffer, '\0', BUFF_SZ);

            res = read(my_fifo,buffer,BUFF_SZ);
            if(res >0){
                printf("receive message from server: %s\n",buffer);
            }
            close(my_fifo);
        }
        else if(op == 3){
            //判断管道文件是否存在
            if(access(FIFO_3,F_OK) == -1){
                printf("Could not open FIFO %s\n",FIFO_3);
                exit(EXIT_FAILURE);
            }
            //打开管道
            fifo_fd3 = open(FIFO_3,O_WRONLY | O_NONBLOCK);
            if(fifo_fd3 == -1){
                printf("Could not open %s for write access\n",FIFO_3);
                exit(EXIT_FAILURE);
            }
            //从客户端读入目标用户名和聊天消息
            printf("please input target_username: \n");
            scanf("%s",target_user);
            printf("please input your message: \n");
            scanf("%s",message);

            MESSAGE message1;
            strcpy(message1.message_buff , message);
            strcpy(message1.username , target_user);

            write(fifo_fd3,&message1, sizeof(MESSAGE));
            close(fifo_fd3);
        }
        else if(op == 0){
            printf("Client is terminated\n");
            close(my_fifo);
            (void) unlink(mypipename);
            exit(0);
        }
        else if(op == 4){
            my_fifo = open(mypipename,O_RDONLY);
            if(my_fifo == -1){
                printf("Could not open %s for read only access\n",mypipename);
                exit(EXIT_FAILURE);
            }
            /* get result fron server */
            memset(buffer, '\0', BUFF_SZ);

            res = read(my_fifo,buffer,BUFF_SZ);
            if(res >0){
                printf("receive message from server: %s\n",buffer);
            }
            close(my_fifo);
        }
        else{
            printf("Error operation!\n");
            printf("please input again!\n");
        }
    }
    return 0;

}