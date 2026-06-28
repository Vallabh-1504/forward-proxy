#include "Threadpool.hpp"

#include <iostream>

namespace miniCDN{

Threadpool::Threadpool(size_t numThreads) : m_thread_count(numThreads), m_stop(false){
    for(size_t i = 0; i < m_thread_count; i++){
        m_workers.emplace_back([this] {
            while(true){
                std::function<void()> threadTask;

                {
                    // Scope lock
                    std::unique_lock lock(this->m_queue_mutex);

                    this->m_condition.wait(lock, [this]{
                        return !m_tasks.empty() || this->m_stop;
                    });

                    // if shutting down and no tasks left, exit thread
                    if(this->m_stop && m_tasks.empty()){
                        return;
                    }

                    if(m_tasks.empty()){
                        continue;
                    }

                    // Take the task from queue
                    threadTask = std::move(this->m_tasks.front());
                    this->m_tasks.pop();
                }

                try{
                    threadTask();
                }
                catch(...){
                    std::cerr << "[Threadpool] worker thread caught exception" << "\n";
                }
            }
        });
    }
}

void Threadpool::enqueue(std::function<void()> task){
    {
        std::unique_lock lock(m_queue_mutex);

        if(m_stop){
            throw std::runtime_error("[Threadpool] enqueue on stopped threadpool");
        }

        m_tasks.push(task);

        m_condition.notify_one();
    }
}

Threadpool::~Threadpool(){
    {
        std::unique_lock lock(m_queue_mutex);
        m_stop = true;
    }

    m_condition.notify_all();

    for(std::thread &worker: m_workers){
        if(worker.joinable()){
            worker.join();
        }
    }
}

} // namespace miniCDN