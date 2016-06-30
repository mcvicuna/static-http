//
//  HttpPool.h
//  shttp
//
//  Created by Mark Vicuna on 6/28/16.
//  Copyright © 2016 Mark Vicuna. All rights reserved.
//

#ifndef HttpPool_h
#define HttpPool_h

#include <iostream>
#include <thread>
#include <vector>
#include <deque>
#include <mutex>

const int MAX_FD = 256; // Guess at maximum number of file descriptors per process

//TODO: Tune the three following constants to find optimal number of open connections and concurrent streaming files
// based on the maximum number of open file descriptors
const int MAX_CONNECTION_BACKLOG = 64;// MAX_FD/3;  // use only a portion for the backlog
const int MAX_WORKER_QUEUE_LENGTH = (MAX_FD-MAX_CONNECTION_BACKLOG)/2; // each request consumes two FD

class HttpPool {
private:
    std::vector<std::thread> workers;
    std::deque<std::function<void()> > requests;
    std::condition_variable request_ready;
    std::atomic<bool> shutdown;
    std::mutex task_queue_mutex;
    
    static std::unique_ptr<HttpPool> singleton;
    static std::once_flag once;

    HttpPool(size_t number_threads) : shutdown(false) {
        
        // lambdas for easy coding
        for(int i=0; i < number_threads; i++) {
            workers.push_back(std::thread([this]() {
                std::function<void()> request;
                // loop infinitely since we will be notified of exit by another thread
                for(;;) {
                    // for safety do the locking with a control block
                    {
                        std::unique_lock<std::mutex> lock(task_queue_mutex);
                        
                        request_ready.wait(lock, [this]() {
                            return !(requests.size() == 0 && !shutdown);
                        });
                        
                        if ( shutdown )
                            return;
                        
                        request = requests.front();
                        requests.pop_front();
                        
                    } // lock released
                    
                    request();
                }
                
                return;
            }));
        }
    }
public:
    static HttpPool & get() {
        std::call_once(HttpPool::once,[]() {
            std::cerr << "Creating Pool" << std::endl;
            HttpPool::singleton.reset(new HttpPool(8)); // TODO: make this tunable
        });
        return *singleton;
    }
    
    bool enqueue(std::function<void()> func) {
        bool retVal = false;
        std::unique_lock<std::mutex> lock(task_queue_mutex);
        
        if ( requests.size() > MAX_WORKER_QUEUE_LENGTH )
            std::cerr << "request size larger then anticipated " <<  requests.size() << std::endl;
        
        requests.push_back(func);
        retVal = true;
        request_ready.notify_one();
        
        return retVal;
    }
    
};


#endif /* HttpPool_h */
