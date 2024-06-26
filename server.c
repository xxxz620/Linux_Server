#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <signal.h>
#include <syslog.h>
#include <time.h>
#include <sys/epoll.h>
#include <semaphore.h>
#include <errno.h>
#include "clientinfo.h"
#include "message.h"
#include "client_message.h"
#include "threadpool.h"

#define CONFIG_FILE "server.conf"
#define MAX_EVENT_NUM 100

typedef struct{
    char server_name[40];
    char server_version[10];
    char reg_fifo[40];
    char login_fifo[40];
    char msg_fifo[40];
    char logout_fifo[40];
    char logfiles[40];
    char logfiles_server[40];
    char logfiles_users[40];
    int pool_size;
}server_conf;

int use_num = 0;
int online_num = 0;
server_conf config;
client_info clientInfo[100];
sem_t sem[4];
time_t now;


void read_configure_file(){
    FILE *file = fopen(CONFIG_FILE,"r");
    if(file == NULL){
        perror("open error");
    }
    char line[256];
    while (fgets(line, sizeof(line),file))
    {
        char *key = strtok(line,"=");
        char *value = strtok(NULL,"\n");
        if(strcmp(key,"server_name") == 0){
            strncpy(config.server_name,value, sizeof(config.server_name));
        }
        else if(strcmp(key,"server_version") == 0){
            strncpy(config.server_version,value, sizeof(config.server_version));
        }
        else if(strcmp(key,"REG_FIFO") == 0){
            strncpy(config.reg_fifo,value, sizeof(config.reg_fifo));
        }
        else if(strcmp(key,"LOGIN_FIFO") == 0){
            strncpy(config.login_fifo,value, sizeof(config.login_fifo));
        }
        else if(strcmp(key,"MSG_FIFO") == 0){
            strncpy(config.msg_fifo,value, sizeof(config.msg_fifo));
        }
        else if(strcmp(key,"LOGOUT_FIFO") == 0){
            strncpy(config.logout_fifo,value, sizeof(config.logout_fifo));
        }
        else if(strcmp(key,"LOGFILES") == 0){
            strncpy(config.logfiles,value,sizeof(config.logfiles));
        }
        else if(strcmp(key,"LOGFILES_SERVER") == 0 ){
            strncpy(config.logfiles_server,value, sizeof(config.logfiles_server));
        }
        else if(strcmp(key,"LOGFILES_USERS") == 0){
            strncpy(config.logfiles_users,value, sizeof(config.logfiles_users));
        }
        else if(strcmp(key,"POOLSIZE") == 0){
            config.pool_size = (int)(*value-'0');
        }
    }
    fclose(file);
}

void init_daemon() {
    pid_t pid = fork();
    if (pid < 0) {
        perror("fork error!");
        exit(EXIT_FAILURE);
    }
    if (pid > 0) {
        exit(EXIT_SUCCESS);
    }
    if (setsid() < 0)
    {
        perror("setsid error!");
        exit(EXIT_FAILURE);
    }

    pid = fork();
    if(pid < 0){
        perror("fork error!");
        exit(EXIT_FAILURE);
    }
    if(pid > 0)
    {
        exit(EXIT_SUCCESS);
    }
    //�ر����ļ�ϵͳ�Ĺ���
    for (int i = 0; i < 3; close(i++));
    chdir("/");
    umask(0);


    signal(SIGTERM,SIG_IGN);
    signal(SIGCHLD,SIG_IGN);

    char buf[100];
    time_t n;
    n = time(NULL);
    sprintf(buf,"%sserver.log",config.logfiles_server);
    FILE *file = fopen(buf,"a");

    fprintf(file,"my server is running! server_name: %s version: %s time: %s",config.server_name,config.server_version,ctime(&n));

    fclose(file);

}

void addfd(int fd,int epoll_fd)
{
    struct epoll_event ev;
    ev.data.fd = fd;
    ev.events = EPOLLIN ;
    if(epoll_ctl(epoll_fd,EPOLL_CTL_ADD,fd,&ev) == -1)
    {
        perror("epoll ctl error!");
    }
}

void Broadcast()
{
    //���㵱ǰ���������ͼ�¼�����û���
    char buf[100];
    char buf1[1000];
    sprintf(buf,"current online num : %d\n",online_num);
    sprintf(buf1,"online client: ");
    for (int j = 0; j < use_num; ++j) {
        if(clientInfo[j].online)
        {
            strcat(buf1,clientInfo[j].user_name);
            strcat(buf1," ");
        }
    }
    strcat(buf1,"\n");
    //�����������������û�����ӡ��ÿһ���û�
    for (int j = 0; j < use_num; ++j) {
        if(clientInfo[j].online == 1)
        {
            //д��buf
            int fd = open(clientInfo[j].pri_fifo,O_WRONLY | O_NONBLOCK);
            if(fd == -1)
            {
                perror("open broadcast fifo");
            }
            write(fd,buf, strlen(buf)+1);
            close(fd);
            printf("%d:%s",j,buf);
            //д��buf1
            fd = open(clientInfo[j].pri_fifo,O_WRONLY | O_NONBLOCK);
            if(fd == -1)
            {
                perror("open broadcast fifo");
            }
            write(fd,buf1, strlen(buf1)+1);
            close(fd);
            printf("%d:%s",j,buf1);
        }
    }
}

void register_request(void *args)
{
    sem_wait(&sem[0]);
    client_info *ci = args;
    time_t n;
    n = time(NULL);
    if(use_num>=100)
    {
        exit(1);
    }
    for (int i = 0; i < use_num; ++i) {
        //�жϷ��������Ƿ������ͬ�û���
        if(strcmp(clientInfo[i].user_name,ci->user_name) == 0){
            char buf[100] = "exit repeat name!\n";
            int fd = open(ci->pri_fifo,O_WRONLY | O_NONBLOCK);
            write(fd,buf, strlen(buf)+1);
            close(fd);
            printf("exit repeat name!\n");
            return;
        }
    }
    //��ʼ��clientinfo�ṹ������
    strcpy(clientInfo[use_num].user_name,ci->user_name);
    strcpy(clientInfo[use_num].password,ci->password);
    strcpy(clientInfo[use_num].pri_fifo,ci->pri_fifo);
    sprintf(clientInfo[use_num].log,"%s%s-%d.log", config.logfiles_users, clientInfo[use_num].user_name,getpid());
    sprintf(clientInfo[use_num].failed_log,"%s-%d.failed_log",clientInfo[use_num].user_name,getpid());
    clientInfo[use_num].online = 0;
    clientInfo[use_num].failed = 0;

    int fd = open(clientInfo[use_num].log,O_CREAT | O_WRONLY | O_NONBLOCK , 0600);
    if(fd == -1)
    {
        perror("open log");
    }
    char buf[1000];
    sprintf(buf,"%s register time:%s",ci->user_name, ctime(&n));
    write(fd,buf, strlen(buf)+1);
    close(fd);

    fd = creat(clientInfo[use_num].failed_log, 0600);
    if(fd == -1)
    {
        perror("create failed log error");
    }
    close(fd);
    //����ʾע��ɹ���Ϣд�ؿͻ���
    fd = open(clientInfo[use_num].pri_fifo,O_WRONLY | O_NONBLOCK);
    if(fd == -1)
    {
        perror("open");
    }

    sprintf(buf,"%s register successfully\n",clientInfo[use_num].user_name);
    if(write(fd,buf, strlen(buf)+1)==-1)
    {
        if (errno == EAGAIN) {
            // �ܵ����������������д��ʧ�ܵ����
            fprintf(stderr, "Pipe is full, try again later.\n");
        } else {
            perror("write");
        }
    }

    close(fd);

    use_num += 1;
    sem_post(&sem[0]);
}

void login_request(void * args)
{
    sem_wait(&sem[1]);
    client_info * ci = args;
    int i;
    for (i = 0; i < use_num; ++i) {
        if(strcmp(clientInfo[i].user_name,ci->user_name) == 0
           && strcmp(clientInfo[i].password,ci->password) == 0)
        {
            time_t t;
            time(&t);
            online_num ++;
            clientInfo[i].online = 1;
            //����־׷�ӵ��ļ�β
            FILE * file = fopen(clientInfo[i].log,"a");
            if(file==NULL){
                sem_post(&sem[1]);
                printf("fopen error!\n");
                exit(EXIT_FAILURE);
            }
            fprintf(file,"%s login time:%s",ci->user_name, ctime(&t));
            printf("%s login time\n",ci->user_name);
            fclose(file);

            //����¼�ɹ���Ϣд�ظ��ͻ���
            int fd = open(clientInfo[i].pri_fifo,O_WRONLY | O_NONBLOCK);
            if(fd == -1)
            {
                perror("write error");
            }

            char buf[100];
            sprintf(buf,"%s login successfully!\n",clientInfo[i].user_name);
            write(fd,buf, strlen(buf)+1);
            close(fd);

            break;
        }
    }

    //�޸��û�����
    if(i==use_num)
    {
        int fd = open(ci->pri_fifo,O_WRONLY | O_NONBLOCK);
        if(fd == -1)
        {
            perror("open pri_fifo");
        }
        char buf[100];
        sprintf(buf,"%s login failed!\n",ci->user_name);
        write(fd,buf, strlen(buf)+1);
        close(fd);
        printf("no user!\n");
    }
    else
    {
        int fd = open(clientInfo[i].pri_fifo,O_WRONLY | O_NONBLOCK);
        if(fd == -1)
        {
            perror("open pri_fifo");
        }
        if(clientInfo[i].failed == 1)
        {
            FILE *file = fopen(clientInfo[i].failed_log,"r");
            if(file == NULL)
            {
                perror("read file error!");
            }
            FILE *file1 = fopen(clientInfo[i].log,"a");
            if(file1 == NULL)
            {
                perror("read file error!");
            }

            char line[1000];
            while (fgets(line, sizeof(line),file) != NULL)
            {
                //���ɹ���Ϣд����־��
                char * send = strtok(line,",");
                char * rec = strtok(NULL,",");
                char * msg = strtok(NULL,".");
                char * time = strtok(NULL,"\n");
                fprintf(file1,"sender:%s receiver:%s time:%s true\n",send,rec,time);
                printf("sender:%s receiver:%s time:%s true\n",send,rec,time);
                //ͨ��˽�йܵ����͸��ͻ��ˡ�
                write(fd,msg, strlen(msg)+1);
                write(fd,time, strlen(time)+1);
                memset(line,'\0',strlen(line));
            }
            char buf[100] = "end_push";
            write(fd,buf, strlen(buf)+1);
            fclose(file);
            fclose(file1);
            close(fd);
            clientInfo[i].failed = 0;

            //ͨ����w�ķ�ʽ���ļ�����ʽ������ļ����ݡ�
            file = fopen(clientInfo[i].failed_log,"w");
            if(file == NULL)
            {
                perror("clean file error!");
            }
            fclose(file);
        } else{
            char buf[100] = "none_push";
            write(fd,buf, strlen(buf)+1);
            close(fd);
        }
        //�㲥��ÿ���û������������û���
        Broadcast();
    }
    sem_post(&sem[1]);
}

void msg_request(void *args)
{
    sem_wait(&sem[2]);
    client_message *c_m = args;
    int size = sizeof(c_m->m.msg);
    int i;
    time_t t;
    int flag = -1;
    for (i = 0; i < use_num; ++i)
    {
        //�ڷ�������¼�ͻ���Ϣ�Ľṹ����Ѱ��Ŀ���û�
        if(strcmp(c_m->m.target,clientInfo[i].user_name) == 0)
        {

            //��clientinfo��failed_log���Ƶ�client_message��

            strcpy(c_m->c.failed_log,clientInfo[i].failed_log);
            flag = i;
            //�ж��û��Ƿ�����
            if(clientInfo[i].online == 0)
            {
                continue;
            }
            int fd = open(clientInfo[i].pri_fifo,O_WRONLY | O_NONBLOCK);
            write(fd,c_m->m.msg, size+1);
            close(fd);

        }

        if(strcmp(c_m->c.user_name,clientInfo[i].user_name) == 0)
        {
            strcpy(c_m->c.log,clientInfo[i].log);
        }
    }
    i = flag;
    //δ�ҵ�Ŀ���û������
    if(flag == -1)
    {
        printf("no user!\n");
    }
        //Ŀ�겻���ߵ����
    else if(clientInfo[i].online == 0)
    {
        t = time(NULL);
        char* s = ctime(&t);
        s[strlen(s)-1] = '\0';
        //��¼�����ͷ�����־�ļ���
        FILE *file = fopen(c_m->c.log,"a");
        if(file == NULL)
        {
            perror("open log");
        }
        fprintf(file,"sender:%s receiver:%s time:%s false\n",c_m->c.user_name,c_m->m.target, s);
        fclose(file);

        //��¼��һ���ض����ļ��У����ڵ�¼ʱ������
        FILE *file1 = fopen(clientInfo[i].failed_log,"a");
        if(file == NULL)
        {
            perror("open failed_log");
        }
        fprintf(file1,"%s,%s,%s,%s\n",c_m->c.user_name,c_m->m.target,c_m->m.msg, s);
        printf("%s,%s,%s,%s\n",c_m->c.user_name,c_m->m.target,c_m->m.msg, s);

        fclose(file1);
        clientInfo[i].failed = 1;

        char buf[100];
        int fd = open(c_m->c.pri_fifo,O_WRONLY | O_NONBLOCK);
        if(fd == -1)
        {
            perror("open error");
        }
        sprintf(buf,"%s is not online!\n",clientInfo[i].user_name);
        write(fd,buf, strlen(buf)+1);
        close(fd);
    }
    else
    {

        FILE *file = fopen(c_m->c.log,"a");
        if(file == NULL)
        {
            perror("msg request error!");
        }
        t = time(NULL);
        char* date = ctime(&t);
        date[strlen(date)-1] = '\0';
        fprintf(file,"sender:%s receiver:%s time:%s true",c_m->c.user_name,c_m->m.target, date);
        fclose(file);

        //�����ͳɹ�����ʾ��Ϣ���ظ��ͻ���
        int fd = open(c_m->c.pri_fifo,O_WRONLY | O_NONBLOCK);
        if(fd == -1)
        {
            perror("msg request error!");
        }
        char buf[100];
        sprintf(buf,"send successfully!\n");
        write(fd,buf, strlen(buf)+1);
        close(fd);

    }
    sem_post(&sem[2]);
}

void logout_request(void * args)
{
    sem_wait(&sem[3]);
    client_info *ci = args;
    int i;
    time_t t;
    for (i = 0; i < use_num; ++i) {
        if(strcmp(ci->user_name,clientInfo[i].user_name) == 0)
        {
            printf("logout successfully!\n");
            clientInfo[i].online = 0;
            online_num--;
            break;
        }
    }
    FILE *file = fopen(clientInfo[i].log,"a");
    t = time(NULL);
    char* date = ctime(&t);
    fprintf(file,"%s logout time:%s",clientInfo[i].user_name, date);
    fclose(file);

    char buf[100] = "logout successfully!\n";
    int fd = open(ci->pri_fifo,O_WRONLY | O_NONBLOCK);
    if(fd == -1)
    {
        perror("logout");
    }
    write(fd,buf,strlen(buf)+1);
    close(fd);

    //�㲥��ʣ���û�
    Broadcast();
    sem_post(&sem[3]);
}


int main() {

    read_configure_file();

    init_daemon();

    for (int i = 0; i < 4; ++i) {
        sem_init(&sem[i],0,1);
    }

    int fifo1_fd, fifo2_fd, fifo3_fd, fifo4_fd;

    if(access(config.reg_fifo,F_OK)==-1){
        //create reg_fifo
        if (mkfifo(config.reg_fifo, 0666) == -1)
        {
            perror("reg_fifo error!");
        }
        if(chmod(config.reg_fifo,0666) == -1)
        {
            perror("chmod reg_fifo");
        }
    }

    fifo1_fd = open(config.reg_fifo, O_RDONLY | O_NONBLOCK);
    if (fifo1_fd == -1){
        printf("Could not open %s for read only access\n",config.reg_fifo);
        exit(EXIT_FAILURE);
    }

    if(access(config.login_fifo,F_OK) == -1)
    {
        //create login_fifo
        if (mkfifo(config.login_fifo, 0666) == -1)
        {
            perror("login_fifo error!");
        }
        if(chmod(config.login_fifo,0666) == -1)
        {
            perror("chmod login_fifo");
        }
    }
    fifo2_fd = open(config.login_fifo, O_RDONLY | O_NONBLOCK);
    if (fifo2_fd == -1){
        printf("Could not open %s for read only access\n",config.login_fifo);
        exit(EXIT_FAILURE);
    }

    if(access(config.msg_fifo,F_OK) == -1)
    {
        //creat msg_fifo
        if (mkfifo(config.msg_fifo, 0666))
        {
            perror("msg_fifo error!");
        }
        if(chmod(config.msg_fifo, 0666) == -1)
        {
            perror("chmod msg_fifo");
        }
    }

    fifo3_fd = open(config.msg_fifo, O_RDONLY | O_NONBLOCK);
    if (fifo3_fd == -1){
        printf("Could not open %s for read only access\n",config.msg_fifo);
        exit(EXIT_FAILURE);
    }

    if(access(config.logout_fifo,F_OK) == -1)
    {
        //create logout_fifo
        if(mkfifo(config.logout_fifo,0666) == -1)
        {
            perror("logout_fifo error!");
        }
        if(chmod(config.logout_fifo, 0666) == -1)
        {
            perror("chmod logout_fifo");
        }
    }

    fifo4_fd = open(config.logout_fifo,O_RDONLY | O_NONBLOCK);
    if (fifo4_fd == -1){
        printf("Could not open %s for read only access\n",config.logout_fifo);
        exit(EXIT_FAILURE);
    }

    int epoll_fd;
    struct epoll_event events[MAX_EVENT_NUM];

    //����epoll���
    epoll_fd = epoll_create(4);
    if(epoll_fd == -1)
    {
        perror("epoll error!");
    }

    //��������֪�ܵ�ע��Ϊ�ں��¼�
    addfd(fifo1_fd,epoll_fd);
    addfd(fifo2_fd,epoll_fd);
    addfd(fifo3_fd,epoll_fd);
    addfd(fifo4_fd,epoll_fd);

    pthread_t t1,t2,t3,t4;

    if(strcmp(config.server_version , "1.0.0" ) == 0)
    {
        while (1)
        {
            int num = epoll_wait(epoll_fd,events,MAX_EVENT_NUM,-1);
            if(num < 0 && errno != EINTR)
            {
                printf("epoll error!\n");
                break;
            }
            for (int i = 0; i < num; ++i) {
                int fd = events[i].data.fd;
                if(fd == fifo1_fd)
                {
                    client_info c;
                    if (read(fd,&c, sizeof(client_info)) > 0)
                    {
                        pthread_create(&t1,NULL,(void*) register_request,&c);
                        pthread_join(t1,NULL);
                    }
                }
                else if(fd == fifo2_fd)
                {
                    client_info c;
                    if (read(fd,&c, sizeof(client_info)) > 0)
                    {
                        pthread_create(&t2,NULL,(void*) login_request,&c);
                        pthread_join(t2,NULL);
                    }
                }
                else if(fd == fifo3_fd)
                {
                    client_message c_m;
                    if (read(fd,&c_m, sizeof(client_message)) > 0)
                    {
                        pthread_create(&t3, NULL, (void *) msg_request, &c_m);
                        pthread_join(t3,NULL);
                    }
                }
                else if(fd == fifo4_fd)
                {
                    client_info clientInfo1;
                    if(read(fd,&clientInfo1, sizeof(client_info )) > 0)
                    {
                        pthread_create(&t4, NULL,(void*)logout_request,&clientInfo1);
                        pthread_join(t4,NULL);
                    }

                }
            }
        }
    }
    else if(strcmp(config.server_version ,"2.0.0") == 0)
    {
        Thread_pool_init(config.pool_size);  //��ʼ���̳߳�
        while (1)
        {
            int num = epoll_wait(epoll_fd,events,MAX_EVENT_NUM,-1);
            if(num < 0 && errno != EINTR)
            {
                printf("epoll error!\n");
                break;
            }
            for (int i = 0; i < num; ++i) {
                int fd = events[i].data.fd;
                if(fd == fifo1_fd)
                {
                    client_info c;
                    if (read(fd,&c, sizeof(client_info)) > 0)
                    {
                        add_task(&register_request,&c);
                    }
                }
                else if(fd == fifo2_fd)
                {
                    client_info c;
                    if (read(fd,&c, sizeof(client_info)) > 0)
                    {
                        add_task(&login_request,&c);
                    }
                }
                else if(fd == fifo3_fd)
                {
                    client_message c_m;
                    if (read(fd,&c_m, sizeof(client_message)) > 0)
                    {
                        add_task(&msg_request,&c_m);
                    }
                }
                else if(fd == fifo4_fd)
                {
                    client_info clientInfo1;
                    if(read(fd,&clientInfo1, sizeof(client_info )) > 0)
                    {
                        add_task(&logout_request,&clientInfo1);
                    }
                }
            }
        }
    }

    return 0;
}