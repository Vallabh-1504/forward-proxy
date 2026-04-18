#ifndef THREADPOOL_HPP
#define THREADPOOL_HPP

#include <vector>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <functional>

namespace miniCDN{

class Threadpool{
public: 
    Threadpool(size_t numThreads);
    ~Threadpool();

    // Add a job to queue
    void enqueue(std::function<void()> task);

private:
    int m_thread_count;

    std::vector<std::thread> m_workers;
    std::queue<std::function<void()>> m_tasks;

    std::mutex m_queue_mutex;
    std::condition_variable m_condition;

    // flag to signal threads to stop when destructor is called
    bool m_stop;
};

} // namespace miniCDN

#endif