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
    void (*callback)(void *); //任务函数
    void *argument;     //任务参数
    void *next;         //指向下一个任务指针
} task_t;

const int NUM = 2;

pthread_mutex_t m_mutex_pool; //互斥量
pthread_cond_t m_not_empty; //条件变量
pthread_t* thread_compose;
pthread_t manager_thread;

static int m_pool_size;    //线程池大小
static int m_live_num;     //存活线程数
static int m_busy_num;     //工作线程数
static int m_destroy_num;      //待销毁线程数
static int shutdown = 0;   //是否销毁
static int queue_size = 0;      //任务队列长度
static task_t *head;             //任务队列头指针

static void *manager();   //管理线程的任务
static void *worker();    //工作线程的任务
static void destroy_thread();   //销毁线程池
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

    pthread_mutex_init(&m_mutex_pool,NULL); //初始化互斥量
    pthread_cond_init(&m_not_empty,NULL);   //初始化条件变量

    pthread_create(&manager_thread,NULL,manager,NULL); //创建管理者线程

    for (int i = 0; i < m_pool_size; ++i) {
        pthread_create(&thread_compose[i],NULL,worker,NULL);
        FILE *file = fopen(log_path,"a");   //在日志中记录线程被分派的时间
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
        //每五秒检测一次
        sleep(3);

        pthread_mutex_lock(&m_mutex_pool);
        //如果任务队列的长度大于存活线程的数量
        //则未线程池增加线程，每次轮询只增加两个线程
        if(queue_size > m_live_num && m_live_num < m_pool_size)
        {
            int count = NUM;
            for (int i = 0; i < m_pool_size && count < NUM
                            && m_live_num < m_pool_size; ++i) {
                if(thread_compose[i] == 0)
                {
                    pthread_create(&thread_compose[i],NULL,worker,NULL);

                    FILE *file = fopen(log_path,"a");   //在日志中记录线程被分派的时间
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


        //销毁线程部分
        if(m_busy_num*2 < m_live_num && m_live_num > NUM)
        {
            pthread_mutex_lock(&m_mutex_pool);
            m_destroy_num = NUM;
            pthread_mutex_unlock(&m_mutex_pool);


            //唤醒线程，如果线程没事做就会自杀，定义在worker里面
            //因为没事干的线程被阻塞了，等待唤醒
            //一旦唤醒就执行退出
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
            //将调用线程放入阻塞队列
            //管理线程可以通过条件变量唤醒
            pthread_cond_wait(&m_not_empty,&m_mutex_pool);

            //解除阻塞后判断是否需要销毁线程，
            //如果管理线程设置了需要销毁则销毁该线程
            if(m_destroy_num > 0)
            {
                m_destroy_num--;
                if(m_live_num > 0)
                {
                    m_live_num--;

                    FILE *file = fopen(log_path,"a");   //在日志中记录线程被销毁的时间
                    if(file == NULL)
                    {
                        printf("open log path error!\n");
                    }
                    time_t t;
                    t = time(NULL);
                    fprintf(file,"destroy thread , time : %s", ctime(&t));
                    fclose(file);

                    pthread_mutex_unlock(&m_mutex_pool);
                    destroy_thread();//销毁线程
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
        task->callback(task->argument);//通过函数指针执行任务
        task->argument = NULL;  //防止内存泄漏

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
    new_task->next = q; //链表的头插法
    queue_size++;
    pthread_mutex_unlock(&m_mutex_pool);
}

static void* get_task()
{
    if(head->next != NULL){
        task_t * p = head->next;
        head->next = p->next;
        p->next = NULL;         //取走任务后需要将任务从任务队列里删除
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