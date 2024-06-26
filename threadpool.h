//
// Created by xietangzhen on 2024-06-23.
//
#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#ifndef UNTITLED2_THREADPOOL_H
#define UNTITLED2_THREADPOOL_H

#endif //UNTITLED2_THREADPOOL_H

#define log_path "/var/log/chat-logs/server/threads.log"

typedef struct
{
    void (*callback)(void *); //������
    void *argument;     //�������
    void *next;         //ָ����һ������ָ��
} task_t;

const int NUM = 2;

pthread_mutex_t m_mutex_pool; //������
pthread_cond_t m_not_empty; //��������
pthread_t* thread_compose;
pthread_t manager_thread;

static int m_pool_size;    //�̳߳ش�С
static int m_live_num;     //����߳���
static int m_busy_num;     //�����߳���
static int m_destroy_num;      //�������߳���
static int shutdown = 0;   //�Ƿ�����
static int queue_size = 0;      //������г���
static task_t *head;             //�������ͷָ��

static void *manager();   //�����̵߳�����
static void *worker();    //�����̵߳�����
static void destroy_thread();   //�����̳߳�
void add_task(void (*p)(),void *args);
static void* get_task();



void Thread_pool_init(int size)
{

    m_pool_size = size;
    m_live_num = size;
    m_busy_num = 0;

    thread_compose = malloc(sizeof(pthread_t) * m_pool_size);
    head = malloc(sizeof(task_t));
    head->next = NULL;

    if(thread_compose == NULL)
    {
        printf("thread pool create failed!\n");
    }

    memset(thread_compose,0, sizeof(pthread_t) * m_pool_size);

    pthread_mutex_init(&m_mutex_pool,NULL); //��ʼ��������
    pthread_cond_init(&m_not_empty,NULL);   //��ʼ����������

    pthread_create(&manager_thread,NULL,manager,NULL); //�����������߳�

    for (int i = 0; i < m_pool_size; ++i) {
        pthread_create(&thread_compose[i],NULL,worker,NULL);
        FILE *file = fopen(log_path,"a");   //����־�м�¼�̱߳����ɵ�ʱ��
        if(file == NULL)
        {
            printf("open log path error!\n");
        }
        time_t t;
        t = time(NULL);
        fprintf(file,"allocate thread , time : %s", ctime(&t));
        fclose(file);
    }
}

static void *manager()
{
    while (!shutdown)
    {
        //ÿ������һ��
        sleep(3);

        pthread_mutex_lock(&m_mutex_pool);
        //���������еĳ��ȴ��ڴ���̵߳�����
        //��δ�̳߳������̣߳�ÿ����ѯֻ���������߳�
        if(queue_size > m_live_num && m_live_num < m_pool_size)
        {
            int count = NUM;
            for (int i = 0; i < m_pool_size && count < NUM
                            && m_live_num < m_pool_size; ++i) {
                if(thread_compose[i] == 0)
                {
                    pthread_create(&thread_compose[i],NULL,worker,NULL);

                    FILE *file = fopen(log_path,"a");   //����־�м�¼�̱߳����ɵ�ʱ��
                    if(file == NULL)
                    {
                        printf("open log path error!\n");
                    }
                    time_t t;
                    t = time(NULL);
                    fprintf(file,"allocate thread , time : %s", ctime(&t));
                    fclose(file);

                    count++;
                    m_live_num++;
                }
            }
        }
        pthread_mutex_unlock(&m_mutex_pool);


        //�����̲߳���
        if(m_busy_num*2 < m_live_num && m_live_num > NUM)
        {
            pthread_mutex_lock(&m_mutex_pool);
            m_destroy_num = NUM;
            pthread_mutex_unlock(&m_mutex_pool);


            //�����̣߳�����߳�û�����ͻ���ɱ��������worker����
            //��Ϊû�¸ɵ��̱߳������ˣ��ȴ�����
            //һ�����Ѿ�ִ���˳�
            for (int i = 0; i < NUM; ++i) {
                pthread_cond_signal(&m_not_empty);
            }
        }

        pthread_mutex_lock(&m_mutex_pool);
        if(queue_size > 0 && m_live_num > m_busy_num)
        {
            for(int i=0;i < m_live_num-m_busy_num; i++)
            {
                pthread_cond_signal(&m_not_empty);
            }
        }
        pthread_mutex_unlock(&m_mutex_pool);
    }
    return NULL;
}

static void * worker()
{
    while (1)
    {
        pthread_mutex_lock(&m_mutex_pool);
        while(queue_size == 0)
        {
            //�������̷߳�����������
            //�����߳̿���ͨ��������������
            pthread_cond_wait(&m_not_empty,&m_mutex_pool);

            //����������ж��Ƿ���Ҫ�����̣߳�
            //��������߳���������Ҫ���������ٸ��߳�
            if(m_destroy_num > 0)
            {
                m_destroy_num--;
                if(m_live_num > 0)
                {
                    m_live_num--;

                    FILE *file = fopen(log_path,"a");   //����־�м�¼�̱߳����ٵ�ʱ��
                    if(file == NULL)
                    {
                        printf("open log path error!\n");
                    }
                    time_t t;
                    t = time(NULL);
                    fprintf(file,"destroy thread , time : %s", ctime(&t));
                    fclose(file);

                    pthread_mutex_unlock(&m_mutex_pool);
                    destroy_thread();//�����߳�
                }
            }
        }

        if(shutdown)
        {
            pthread_mutex_unlock(&m_mutex_pool);
            destroy_thread();
        }
        printf("111");
        task_t *task = get_task();
        if(task == NULL){
            pthread_mutex_unlock(&m_mutex_pool);
            continue;
        }
        m_busy_num++;
        pthread_mutex_unlock(&m_mutex_pool);
        task->callback(task->argument);//ͨ������ָ��ִ������
        task->argument = NULL;  //��ֹ�ڴ�й©

        pthread_mutex_lock(&m_mutex_pool);
        m_busy_num--;
        pthread_mutex_unlock(&m_mutex_pool);

    }
    return NULL;
}

void add_task(void (*p)(), void *args){
    pthread_mutex_lock(&m_mutex_pool);
    task_t * new_task = malloc(sizeof(task_t));
    new_task->callback = p;
    new_task->argument = args;
    task_t *q = head->next;
    head->next = new_task;
    new_task->next = q; //�����ͷ�巨
    queue_size++;
    pthread_mutex_unlock(&m_mutex_pool);
}

static void* get_task()
{
    if(head->next != NULL){
        task_t * p = head->next;
        head->next = p->next;
        p->next = NULL;         //ȡ���������Ҫ����������������ɾ��
        queue_size--;
        return p;
    }
    else
    {
        return NULL;
    }
}

void destroy_thread(){
    pthread_t th = pthread_self();
    for (int i = 0; i < m_pool_size; ++i) {
        if(thread_compose[i] == th)
        {
            thread_compose[i] = 0;
            break;
        }
    }
    pthread_exit(NULL);
}