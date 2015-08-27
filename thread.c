#include <stdio.h>  
#include <stdlib.h>  
#include <unistd.h>  
#include <sys/types.h>  
#include <pthread.h>  
#include <assert.h>  

//任务线程
typedef struct worker
{
	void * (*process)(void *arg);//回调函数入口 返回值为空指针，参数为空指针的函数指针
	void *arg;//回调函数参数
	struct worker *next;//下一个元素
}threadWork;

//线程池  为链表队列结构 每个任务构成一个节点元素
typedef struct pool
{
	threadWork *queueHead;//链表头
	pthread_mutex_t queue_lock;//互斥锁
    pthread_cond_t queue_ready;//条件锁
	int poolSize;//元素个数
	pthread_t *threadid;//线程标示
	int threadMaxNum; //线程最大数量
	int waitTaskNun;//当前等待任务数量
	int isShutDown;//是否销毁线程池
}threadPool;

static threadPool *pool = NULL;
void destroyPool(void);
void * doTask(void *);
void initThreadPool(int);
int addTaskToPool(void *(*process)(void *), void *);
void * thread_routine(void *);

int main (int argc, char **argv) {
	//初始化线程池
	initThreadPool(3);
	int t[10] = {0};
	int i = 0;
	for (i = 0; i < 10; ++i)
	{
		t[i] = i;
		addTaskToPool(doTask, &t[i]);
	}
	sleep(10);//测试，等待子线程结束
	destroyPool();
	return 0;

	//加入任务
	//退出 释放互斥量 清理内存
}

/**
 * [destroyPool 释放线程池]
 */
void destroyPool() {
	if(pool -> isShutDown) {
		printf("redestroy pool\n");
		exit(-2);
	}
	pool -> isShutDown = 1;
	pthread_cond_broadcast (&(pool->queue_ready));//唤醒所有线程
	int i;
	for (i = 0; i < pool -> threadMaxNum; ++i)
	{
		pthread_join(pool -> threadid[i], NULL);//等待其他线程结束
	}
	threadWork * tmp = pool -> queueHead;
	while(pool -> queueHead != NULL) {//任务没有被执行的 释放任务队列
		tmp = pool -> queueHead;
		pool -> queueHead = pool -> queueHead -> next;
		free(tmp);
	}
	pthread_mutex_destroy(&(pool->queue_lock));  
    pthread_cond_destroy(&(pool->queue_ready));  
      
    free (pool);  
    pool=NULL;
}

void * doTask(void *arg) {
	//printf("Thread %x doTask %d\n", pthread_self (), *(int *)arg);
	sleep (1);
	return NULL;
}

/**
 * 线程池初始化
 * @param num [初始化生成线程的数量]
 */
void initThreadPool(int num) {
	if(num <= 0) {
		printf("init thread num error\n");
		exit(-1);
	}

	pool = (threadPool *)calloc(sizeof(threadPool), 1);
	pthread_mutex_init (&(pool->queue_lock), NULL);  
    pthread_cond_init (&(pool->queue_ready), NULL);
	pool -> queueHead = NULL;
	pool -> threadMaxNum = num;
	pool -> waitTaskNun = 0;
	pool -> threadid = (pthread_t *)calloc(num, sizeof(pthread_t));

	int i;
	for (i = 0; i < num; ++i)
	{
		pthread_create(&(pool->threadid[i]), NULL, thread_routine, NULL);
		printf("initThreadPool %d\n", i);
	}
}

/**
 * 添加任务到线程池  process线程函数起始地址  arg传递给函数的参数
 */
int addTaskToPool(void *(*process)(void *), void *arg) {
	threadWork * worker = (threadWork *) calloc(1, sizeof(threadWork));
	worker -> next = NULL;
	worker -> process = process;
	worker -> arg = arg;
	pthread_mutex_lock (&(pool->queue_lock));
	threadWork * tmp = pool -> queueHead;
	if(tmp != NULL) {//此时队列中有任务
		while(tmp -> next) {
			tmp = tmp -> next;
		}
		tmp -> next = worker;
	}else{
		pool -> queueHead = worker;
	}
	pool -> waitTaskNun ++;
	printf("addTaskToPool %d\n", *(int *)arg);
    //等待队列中有任务了，唤醒一个等待线程
	pthread_mutex_unlock (&(pool->queue_lock)); 
	pthread_cond_signal (&(pool->queue_ready)); 
	return 0; 
}

/**
 * 线程函数起始地址
 * @param arg [description]
 */
void * thread_routine(void * arg) {
	while(1) {
		pthread_mutex_lock (&(pool->queue_lock));
		if(pool -> waitTaskNun == 0 && !pool -> isShutDown) {
			pthread_cond_wait(&(pool->queue_ready), &(pool->queue_lock));
		}
		if(pool -> isShutDown) {
			pthread_mutex_unlock (&(pool->queue_lock));
			printf ("thread %x will exit\n", pthread_self ());  
            pthread_exit (NULL);
		}
		printf("thread_routine %x\n", pthread_self ());
		pool -> waitTaskNun --;
		threadWork * pop = pool -> queueHead;
		pool -> queueHead = pop -> next;
		pthread_mutex_unlock (&(pool->queue_lock));
		//sleep(1);
		(*(pop -> process))(pop -> arg);
		free(pop);
		pop = NULL;
	}
	return NULL;
}
