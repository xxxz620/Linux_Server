#ifndef _CLIENTINFO_H
#define _CLIENTINFO_H

typedef struct {
    char user_name[40];
    char password[40];
    char log[100];
    char pri_fifo[100];
    char failed_log[100];
    int failed;
    int online;
}client_info;

#endif
