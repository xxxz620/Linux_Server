#ifndef _CLIENTINFO_H
#define _CLIENTINFO_H

typedef struct {
    char my_fifo[100];
    char username[50];
    char password[50];
    int login;
}  CLIENTINFO;

#endif