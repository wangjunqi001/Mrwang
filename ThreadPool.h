/**
 *@Author wangjunqi
 *@Date 2018-8-16
 *@Desc threadpool for httpserver  
 **/

#ifndef __THREADPOLL_H__ 
#define __THREADPOLL_H__ 


#include <iostream>
#include <unistd.h>
#include <string>
#include <signal.h>
#include <errno.h>
#include <pthread.h>
#include <vector>
#include "util.hpp"
/**
 * 以下三个宏和性能有极大关系，需要根据实际测试调整
 **/
#define THREAD_POOL_DEFAULT_TIME 4               /* 检测时间*/
#define THREAD_POOL_MIN_WAIT_TASK_NUM 10          /* 如果_queue_size > MIN_WAIT_TASK_NUM则向线程池添加一个线程*/
#define THREAD_POOL_DEFAULT_THREAD_VARY 10        /* 每次添加和销毁的线程数量*/

struct threadpool_task_t{
	void*(*ThreadRoutine)(void* arg); /* callback function of each thread*/
	void* arg;                        /* callback function's paramter    */
};

class thread_pool{
public:
	/**
	 *@Function all public 
	 *@Desc thread_pool object interface
	 *@OP function Return 0 if all goes well , else return -1 , NUM function will return num of threads
	 **/
	thread_pool(int min_thr_num,int max__thr_num,int queue_max_size);
	~thread_pool();
	int Add(void*(*ThreadRoutine)(void*),void* arg);
	//int Destroy();
	int GetAllThreadNum();
	int GetBusyThreadNum();
private:
	static void* worker_thread(void* arg);  /* static 阻止this传递导致不匹配 */
	static void* manager_thread(void* arg); /* 管理线程 */
	bool is_thread_alive(pthread_t tid);
  int thread_pool_free();
private:
	pthread_mutex_t lock;             /* 用于锁住整个对象                */ 
	pthread_mutex_t thread_counter;   /* 记录整个线程池忙碌的线程个数    */
	pthread_cond_t  queue_not_full;   /* 任务队列满时，添加线程的任务阻塞，等待此条件*/
	pthread_cond_t  queue_not_empty;  /* 任务不为空时，通知等待任务的线程*/

	vector<pthread_t> _threads;              /* 线程tid，线程数组*/
	pthread_t  _manager_tid;                 /* 线程池管理线程id */
	vector<threadpool_task_t> _task_queue;  /* 任务队列*/
	size_t _queue_front;                     /* 队首元素下标*/
	size_t _queue_rear;                      /* 队尾元素下标*/
  size_t _queue_size;
	int _min_thr_num;                 /* 线程池最小线程数 */
	int _live_thr_num;                /* 存活线程数*/
	int _busy_thr_num;                /* 已投入运行线程数 */
	int _wait_exit_thr_num;           /* 等待销毁线程数 */ 

	bool shutdown;                    /* 线程池使用状态 */ 
};

#endif







