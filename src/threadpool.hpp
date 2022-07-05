#include <thread>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <assert.h>

template <typename T>
class ThreadPool{
public:
    ThreadPool(int num_threads);
    ~ThreadPool();
    bool append(T&& task);

private:
    struct Pool{
        std::queue<T> tasks;
        std::mutex mutex;
        std::condition_variable cond;
        bool isClosed;
    };
    std::shared_ptr<Pool> pool;
};

template <typename T>
ThreadPool<T>::ThreadPool(int threadCount): pool(std::make_shared<Pool>()){
    assert(threadCount>0);
    for(int i=0; i<threadCount; i++){
        std::thread([pool_=pool]{
            std::unique_lock<std::mutex> locker(pool_->mutex);
            while(!pool_->isClosed){
                if(!pool_->tasks.empty()){
                    auto task = pool_->tasks.front();
                    pool_->tasks.pop();
                    locker.unlock();
                    task();
                    locker.lock();
                }
                else{
                    pool_->cond.wait(locker);
                }
            }
        }).detach();
    }
}

template <typename T>
ThreadPool<T>::~ThreadPool(){
    if(pool){
        pool->isClosed = true;
        pool->cond.notify_all();
    }
}

// #note 没有对任务队列的最大数量做出限制，如果客户端发来了巨量请求，是否会挤爆内存？
template <typename T>
bool ThreadPool<T>::append(T&& task){
    {
        std::lock_guard<std::mutex> locker(pool->mutex);
        pool->tasks.push(std::forward<T>(task));
    }
    pool->cond.notify_one();
    return true;
}