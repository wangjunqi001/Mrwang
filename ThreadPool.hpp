/**
 *@Author wangjunqi
 *@Date 2018-8-16
 *@Desc threadpool for httpserver  
 **/

#ifndef __THREADPOLL_HPP__ 
#define __THREADPOLL_HPP__ 


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
#define THREAD_POOL_DEFAULT_TIME 10               /* 检测时间*/
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

thread_pool::thread_pool(int min_thr_num , int max_thr_num , int queue_max_size) {
		if(min_thr_num > max_thr_num){
			LOG(ERROR)<<"min_thr_num should less equal than max_thr_num"<<endl;
			return;
		}
		_min_thr_num = min_thr_num;
		shutdown = false;
		
		_live_thr_num = 0;
		_busy_thr_num = 0;
		_wait_exit_thr_num = 0;
		_queue_rear = _queue_front = _queue_size = 0;

		_threads.insert(_threads.end(),max_thr_num,0);
		threadpool_task_t task;
		_task_queue.insert(_task_queue.end(),queue_max_size,task);

		if(pthread_cond_init(&queue_not_empty,NULL) != 0 ||
			 pthread_cond_init(&queue_not_full ,NULL) != 0 ||
			 pthread_mutex_init(&lock          ,NULL) != 0 ||
			 pthread_mutex_init(&thread_counter,NULL) != 0){
			LOG(ERROR)<<"thread_pool create: init lock or cond "<<endl;
			return;
		}
		/* 启动_min_thr_num个worker线程 */
		for(int i = 0 ; i < _min_thr_num ; ++i){
			pthread_create(&_threads[i],NULL,worker_thread,(void*)this);
			pthread_detach( _threads[i]);
#ifdef debug
			LOG(DEBUG)<<"start thread 0x"<<_threads[i]<<" ..."<<endl;
#endif
		}
		pthread_create(&_manager_tid,NULL,manager_thread,(void*)this);
		return;
}

int thread_pool::Add(void*(*function)(void*),void* arg){ 
	pthread_mutex_lock(&lock);
	//队列满时需要等待
	while(_queue_size == _task_queue.size() && !shutdown)
		pthread_cond_wait(&queue_not_full,&lock);

	if(shutdown){
		pthread_mutex_unlock(&lock);
		return 0;
	}

  //添加任务至队列
	_task_queue[_queue_rear].ThreadRoutine = function;
	_task_queue[_queue_rear].arg = arg;
	_queue_rear = (_queue_rear + 1) % _task_queue.size();
	_queue_size++;
 
	//通知条件成熟
	pthread_cond_signal(&queue_not_empty);
	pthread_mutex_unlock(&lock);
	return 0;
}

void* thread_pool::worker_thread(void* arg){
	thread_pool* pool = reinterpret_cast<thread_pool*>(arg);
	threadpool_task_t task;
	while(true){
		/* 刚创建线程，等待任务队列中有任务 ，否则等待任务队列中有任务再唤醒线程 */
		pthread_mutex_lock(&pool->lock);
		/* 队列中没有任务就阻塞等待条件变量 */
		while((pool->_queue_size == 0) && !pool->shutdown){
#ifdef debug 
			LOG(INFO)<<"thread 0x"<<pthread_self()<<" is waiting..."<<endl;
#endif 
			pthread_cond_wait(&pool->queue_not_empty,&pool->lock);
		
    	if(pool->_wait_exit_thr_num > 0){
				/* 如果存活线程数等于最小线程数，我们可以‘假装’释放了一个线程资源 */
				pool->_wait_exit_thr_num--;
				/* 如果存活线程数大于最小任务数时，则让当前线程退出,因为我们不需要那么多的线程执行任务 */
				if(pool->_live_thr_num > pool->_min_thr_num){
#ifdef debug
					LOG(DEBUG)<<"thread 0x"<<pthread_self()<<" is exiting!"<<endl;
#endif 
					--pool->_live_thr_num;
					pthread_mutex_unlock(&pool->lock);
					pthread_exit(NULL);
				}
			}
		}
		
		/* 指定了shutdown 则进行回收处理 */
		if(pool->shutdown){
#ifdef debug
				LOG(DEBUG)<<"thread 0x"<<pthread_self()<<" is exiting!"<<endl;
#endif 
				pthread_mutex_unlock(&pool->lock);
				pthread_exit(NULL);
		}
		/* 从队列里拿出一个任务 */
		task.ThreadRoutine = pool->_task_queue[pool->_queue_front].ThreadRoutine;
		task.arg           = pool->_task_queue[pool->_queue_front].arg;
		pool->_queue_front = (pool->_queue_front + 1)%pool->_task_queue.size(); 
		--pool->_queue_size;
		/* 通知其他所有线程条件成熟 */
		pthread_cond_broadcast(&pool->queue_not_empty);
		/* 获取到任务时立刻讲锁资源释放*/
		pthread_mutex_unlock(&pool->lock);
		
		/* 执行任务 */
		pthread_mutex_lock(&pool->thread_counter);
#ifdef debug 
		cout<<"thread 0x"<<pthread_self()<<" is working..."<<endl;
#endif 
		++pool->_busy_thr_num;
		pthread_mutex_unlock(&pool->thread_counter);
		task.ThreadRoutine(task.arg); /* 执行回调函数 */
		pthread_mutex_lock(&pool->thread_counter);
#ifdef debug
		cout<<"thread 0x"<<pthread_self()<<" end working!"<<endl;
#endif 
		--pool->_busy_thr_num;
		pthread_mutex_unlock(&pool->thread_counter);
	}
	pthread_exit(NULL);
}

void* thread_pool::manager_thread(void* arg){
	thread_pool* pool = reinterpret_cast<thread_pool*>(arg);
	while(!pool->shutdown){
		
		sleep(THREAD_POOL_DEFAULT_TIME); /* 隔一定时间间隔调整一次 */

		pthread_mutex_lock(&pool->lock);
		int queue_size = pool->_queue_size;
		int live_thr_num = pool->_live_thr_num;
		pthread_mutex_unlock(&pool->lock);

		pthread_mutex_lock(&pool->thread_counter);
		int busy_thr_num = pool->_busy_thr_num;
		pthread_mutex_unlock(&pool->thread_counter);
		
		/* 任务队列中任务超过最大等待任务时，并且worker线程没有超过上限，则添加 worker线程 */
		if(queue_size >= THREAD_POOL_MIN_WAIT_TASK_NUM && live_thr_num < (int)pool->_threads.size()){
			pthread_mutex_lock(&pool->lock);
			int add = 0;
			/* 在没有满的情况下 一次增加default个线程 */
			for(size_t i = 0 ; i < pool->_threads.capacity() && add < THREAD_POOL_DEFAULT_THREAD_VARY && 
					pool->_live_thr_num < (int)pool->_threads.size() ; ++i){
				if(pool->_threads[i] == 0 && !pool->is_thread_alive(pool->_threads[i])){
					pthread_create(&pool->_threads[i],NULL,worker_thread,pool);
					pthread_detach( pool->_threads[i]);
					++add;
					pool->_live_thr_num++;
				}
			}
			pthread_mutex_unlock(&pool->lock);
		}
		
		/* 销毁空闲线程 忙线程数*2 < 存活线程数时 一次销毁default个线程 随机10个空闲线程即可 */
		if(busy_thr_num * 2 < live_thr_num && live_thr_num > pool->_min_thr_num){
			pthread_mutex_lock(&pool->lock);
			pool->_wait_exit_thr_num -= THREAD_POOL_DEFAULT_THREAD_VARY;
			pthread_mutex_unlock(&pool->lock);
      
			/* 唤醒正在等待任务队列的空闲线程 */
			for(int i = 0 ; i < THREAD_POOL_DEFAULT_THREAD_VARY ; ++i)
				pthread_cond_signal(&pool->queue_not_empty);
		}
		 
	}
	return NULL;
}

thread_pool::~thread_pool(){

	shutdown = true;
	
	pthread_join(_manager_tid,NULL);
	

	/* 唤醒所有在等待条件的线程 */
	for(int i = 0 ; i < _live_thr_num ; ++i)
		pthread_cond_broadcast(&queue_not_empty);

	pthread_mutex_lock(&lock);
	pthread_mutex_destroy(&lock);
	pthread_mutex_lock(&thread_counter);
	pthread_mutex_destroy(&thread_counter);
	pthread_cond_destroy(&queue_not_empty);
	pthread_cond_destroy(&queue_not_full);
}

bool thread_pool::is_thread_alive(pthread_t tid){
	/* 给tid发送0信号探测是否存活 */
	int kill_rc = pthread_kill(tid,0);
	if(kill_rc == ESRCH)
		return false;
	return true;
} 

int thread_pool::GetAllThreadNum(){
	int thread_num = -1;
	pthread_mutex_lock(&thread_counter);
	thread_num = _live_thr_num;
	pthread_mutex_unlock(&thread_counter);
	return thread_num;
}

int thread_pool::GetBusyThreadNum(){
	int busy_thr_num = -1;
	pthread_mutex_lock(&thread_counter);
	busy_thr_num = _busy_thr_num;
	pthread_mutex_lock(&thread_counter);
	return _busy_thr_num;
}

#endif









