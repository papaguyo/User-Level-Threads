//
// Created by Guy Achiam on 10/04/2023.
//


#include <set>
#include <setjmp.h>
#include <iostream>
#include <cstdlib>
#include "Thread.h"
#include <queue>
#include <stdio.h>
#include <setjmp.h>
#include <signal.h>
#include <unistd.h>
#include <sys/time.h>
#include <stdbool.h>
#include <list>
#include <algorithm>






#define FALSE (0)
#define TRUE (1)
#define SECOND 1000000
#define SUCCESS 0





struct sigaction sa = {0};
struct itimerval timer;

void timer_handler(int);
void threads_wake_check ();
void remove_tid_from_sleep(int tid);

sigset_t _mask_set;
Thread* threads[MAX_THREAD_NUM];
std::set<int> free_tids;
std::list<int> ready;//queue of the id of the threads that are in ready stste, first one here in the running one
std::list<int> sleep_lst;
int _quantum_usecs;
int total_running_quantums = 0;

Thread *running_thread;


int cycle_is_finished = FALSE;


//Private Methods


void terminate_all_threads(){
    for (int i = 0; i < MAX_THREAD_NUM; ++i) {
        if (free_tids.count(i) == 0) {
            delete threads[i];
        }
    }
}

void initialize_timer(){
    // Install timer_handler as the signal handler for SIGVTALRM.
    sa.sa_handler = &timer_handler;
    if (sigaction(SIGVTALRM, &sa, NULL) < 0)
    {
        printf("sigaction error.");
        terminate_all_threads();
        exit(1);
    }

    // Configure the timer to expire after 1 sec... */
    timer.it_value.tv_sec =  _quantum_usecs / SECOND;        // first time interval, seconds part
    timer.it_value.tv_usec =  _quantum_usecs % SECOND;        // first time interval, microseconds part

    // configure the timer to expire every 3 sec after that.
    timer.it_interval.tv_sec = _quantum_usecs / SECOND;    // following time intervals, seconds part
    timer.it_interval.tv_usec = _quantum_usecs % SECOND;    // following time intervals, microseconds part

    // Start a virtual timer. It counts down whenever this process is executing.
    if (setitimer(ITIMER_VIRTUAL, &timer, NULL))
    {
        printf("setitimer error.");
        terminate_all_threads();
        exit(1);
    }
}






void mask_checking(int blocked_check){
    if(sigprocmask(blocked_check, &_mask_set, nullptr) == FAIL){
        std::cerr << "system error: mask process was unsuccessful";
        terminate_all_threads();
        exit(1);
    }
}


void jump_to_next_thread(){
    total_running_quantums += 1;

    threads_wake_check();

    if (running_thread->update_environment() == 1){
        return;
    }

    if(cycle_is_finished == FALSE){
        initialize_timer();
    }

    int id_to_spawn = ready.front();
    ready.pop_front();
    running_thread = threads[id_to_spawn];
    running_thread ->set_state(RUNNING);
    running_thread->add_quantum_counter();
    running_thread->load_environment();
}

/**
 * remove the thread with tid from the ready list if it blocked
*/
void remove_tid_from_ready(int tid){
    for (auto it = ready.begin(); it != ready.end(); ++it) {
        if (*it == tid) {
            ready.erase(it);
            break;
        }
    }
}
void remove_tid_from_sleep(int tid) {
    for (auto it = sleep_lst.begin(); it != sleep_lst.end(); ++it) {
        if (*it == tid) {
            sleep_lst.erase(it);
            break;
        }
    }
}


/**
 * implement a for loop that go through every thread in the sleep list
 * compare the threads[id].get_wake_me_at() with the total_running_quantums.
 * if they match wake the thread up
 */
void threads_wake_check()
{
    std::vector<int> id_to_delete;
    for (auto it = sleep_lst.begin(); it != sleep_lst.end(); it++ ) {
        int thread_id = *it;
        State cur = threads[thread_id]->get_state();
        if (threads[thread_id]->get_wake_me_at() <= total_running_quantums && cur != BLOCKED) {
            // Wake up thread
            threads[thread_id]->set_state(READY);
            ready.push_back(thread_id);
            id_to_delete.push_back(thread_id);
        }
    }

    //erases the ids from sleep.
    for(auto it = id_to_delete.begin(); it != id_to_delete.end(); it++){
        for(auto it2 = sleep_lst.begin(); it2 != sleep_lst.end(); it2++){
            if(*it == *it2) {
                sleep_lst.erase(it2);
                break;
            }
        }
    }
}


/**
 * This function is called every end of quantum
 * @param sig
 */
void timer_handler(int sig) {
    cycle_is_finished = TRUE;
    running_thread->set_state(READY);
    ready.push_back(running_thread->get_id());
    jump_to_next_thread();

}


/**
 * @brief initializes the thread library.
 *
 * Once this function returns, the main thread (tid == 0) will be set as RUNNING. There is no need to
 * provide an entry_point or to create a stack for the main thread - it will be using the "regular" stack and PC.
 * You may assume that this function is called before any other thread library function, and that it is called
 * exactly once.
 * The input to the function is the length of a quantum in micro-seconds.
 * It is an error to call this function with non-positive quantum_usecs.
 *
 * @return On success, return 0. On failure, return -1.
*/
int uthread_init(int quantum_usecs){
    if(quantum_usecs <= 0){
        std::cerr << "thread library error: non-positive quantum_usecs\n";
        return FAIL;
    }


    _quantum_usecs = quantum_usecs;
    threads[0] = new Thread(0, 1, nullptr);
    threads[0]->set_state(RUNNING);
    running_thread = threads[0];
    for (int i = 1; i < MAX_THREAD_NUM; i++) {// insert all the free tids to the set
        free_tids.insert(i);
    }

    if(sigemptyset(&_mask_set) == FAIL){
        terminate_all_threads();
        exit(1);
    }
    if(sigaddset(&_mask_set, SIGVTALRM) == FAIL){
        terminate_all_threads();
        exit(1);
    }

    total_running_quantums = 1;
    initialize_timer();
    return SUCCESS;
}



/**
 * @brief Terminates the thread with ID tid and deletes it from all relevant control structures.
 *
 * All the resources allocated by the library for this thread should be released. If no thread with ID tid exists it
 * is considered an error. Terminating the main thread (tid == 0) will result in the termination of the entire
 * process using exit(0) (after releasing the assigned library memory).
 *
 * @return The function returns 0 if the thread was successfully terminated and -1 otherwise. If a thread terminates
 * itself or the main thread is terminated, the function does not return.
*/
int uthread_terminate(int tid){

    mask_checking(SIG_SETMASK);

    int success;
    if(tid == 0){
        terminate_all_threads();
        exit(0);
    }
    else if(free_tids.find(tid) != free_tids.end() || tid < 0 || tid > 99){
        std::cerr << "thread library error: no thread with ID tid exists\n";
        success = -1;
    }

    else{
        if(threads[tid]->get_state() == RUNNING){// checking if the current running thread needs to be terminated, if so initial the next one in the que first. // steel needs to think about the case that there is no such a thread to spawn.
            jump_to_next_thread();
        }

        remove_tid_from_ready(tid);
        remove_tid_from_sleep(tid);
        delete threads[tid];
//        delete (threads[tid]);
        threads[tid] = nullptr;
        free_tids.insert(tid);
        success = 0;
    }
    mask_checking(SIG_UNBLOCK);
    return success;
}




/**
 * @brief Creates a new thread, whose entry point is the function entry_point with the signature
 * void entry_point(void).
 *
 * The thread is added to the end of the READY threads list.
 * The uthread_spawn function should fail if it would cause the number of concurrent threads to exceed the
 * limit (MAX_THREAD_NUM).
 * Each thread should be allocated with a stack of size STACK_SIZE bytes.
 * It is an error to call this function with a null entry_point.
 *
 * @return On success, return the ID of the created thread. On failure, return -1.
*/
int uthread_spawn(thread_entry_point entry_point){

    mask_checking(SIG_BLOCK);

    if(entry_point == nullptr){
        std::cerr << "thread library error: It is an error to call this function with a null entry_point\n";
        return -1;
    }

    int new_id;

    if(free_tids.empty()){
        std::cerr << "thread library error: number of concurrent threads exceed limit\n";
        return -1;
    }

    else{
        new_id = *(free_tids.begin());
        free_tids.erase(free_tids.begin());

        try{
            threads[new_id] = new Thread(new_id, 0, entry_point);
        }
        catch(std::bad_alloc&){
            std::cerr << "system error: memory allocation failed or mask failing in the constructor";
            terminate_all_threads();
            exit(1);
        }
        ready.push_back(new_id);

    }
    mask_checking(SIG_UNBLOCK);
    return new_id;
}


/**
 * @brief Blocks the thread with ID tid. The thread may be resumed later using uthread_resume.
 *
 * If no thread with ID tid exists it is considered as an error. In addition, it is an error to try blocking the
 * main thread (tid == 0). If a thread blocks itself, a scheduling decision should be made. Blocking a thread in
 * BLOCKED state has no effect and is not considered an error.
 *
 * @return On success, return 0. On failure, return -1.
*/
int uthread_block(int tid){

    mask_checking(SIG_BLOCK);

    if(tid == 0){
        std::cerr << "thread library error: the main thread cannot be blocked\n";
        return -1;
    }
    if(free_tids.find(tid) != free_tids.end() || tid < 0 || tid > 99){
        std::cerr << "thread library error: no thread with ID tid exists\n";
        return -1;
    }

    State cur = threads[tid]->get_state();
    switch (cur) {
        case READY:
        {
            remove_tid_from_ready(tid);
            threads[tid]->set_state(BLOCKED);
            break;
        }
        case RUNNING:
        {
            cycle_is_finished = FALSE;
            running_thread->set_state(BLOCKED);
            jump_to_next_thread();
            break;
        }
        case BLOCKED:
        {
            break;
        }
        case SLEEP:
        {
            threads[tid]->set_state(BLOCKED);
            //todo, deal with sleep data structure, make sure when the thread ends sleeping it needs to be blocked.
            break;
        }
    }

    mask_checking(SIG_UNBLOCK);
    return 0;

}


/**
 * @brief Resumes a blocked thread with ID tid and moves it to the READY state.
 *
 * Resuming a thread in a RUNNING or READY state has no effect and is not considered as an error. If no thread with
 * ID tid exists it is considered an error.
 *
 * @return On success, return 0. On failure, return -1.
*/
int uthread_resume(int tid){

    mask_checking(SIG_BLOCK);

    if(free_tids.find(tid) != free_tids.end() || tid < 0 || tid > 99){
        std::cerr << "thread library error: no thread with ID tid exists\n";
        return -1;
    }


    State cur = threads[tid]->get_state();
    switch (cur) {
        case BLOCKED:
        {
            auto it = std::find(sleep_lst.begin(), sleep_lst.end(), tid);
            if(it != sleep_lst.end()){
                threads[tid]->set_state(SLEEP);
                mask_checking(SIG_UNBLOCK);
                return 0;
            }
            threads[tid]->set_state(READY);
            ready.push_back(tid);
            mask_checking(SIG_UNBLOCK);
            return 0;
        }
        default:
        {
            mask_checking(SIG_UNBLOCK);
            return 0;
        }
    }
}


/**
 * @brief Blocks the RUNNING thread for num_quantums quantums.
 *
 * Immediately after the RUNNING thread transitions to the BLOCKED state a scheduling decision should be made.
 * After the sleeping time is over, the thread should go back to the end of the READY queue.
 * If the thread which was just RUNNING should also be added to the READY queue, or if multiple threads wake up
 * at the same time, the order in which they're added to the end of the READY queue doesn't matter.
 * The number of quantums refers to the number of times a new quantum starts, regardless of the reason. Specifically,
 * the quantum of the thread which has made the call to uthread_sleep isn’t counted.
 * It is considered an error if the main thread (tid == 0) calls this function.
 *
 * @return On success, return 0. On failure, return -1.
*/
int uthread_sleep(int num_quantums){
    mask_checking(SIG_BLOCK);
    if(running_thread->get_id() == 0){
        std::cerr << "thread library error: main thread can't call this function uthread_sleep\n";
        return -1;
    }



    running_thread->set_wake_me_at(total_running_quantums + num_quantums);
    sleep_lst.push_back(running_thread->get_id());
    cycle_is_finished = FALSE;
    running_thread->set_state(SLEEP);
    jump_to_next_thread();

    mask_checking(SIG_UNBLOCK);
    return 0;

}


/**
 * @brief Returns the thread ID of the calling thread.
 *
 * @return The ID of the calling thread.
*/
int uthread_get_tid(){
    return running_thread->get_id();
}


/**
 * @brief Returns the total number of quantums since the library was initialized, including the current quantum.
 *
 * Right after the call to uthread_init, the value should be 1.
 * Each time a new quantum starts, regardless of the reason, this number should be increased by 1.
 *
 * @return The total number of quantums.
*/
int uthread_get_total_quantums(){
    return total_running_quantums;
}


/**
 * @brief Returns the number of quantums the thread with ID tid was in RUNNING state.
 *
 * On the first time a thread runs, the function should return 1. Every additional quantum that the thread starts should
 * increase this value by 1 (so if the thread with ID tid is in RUNNING state when this function is called, include
 * also the current quantum). If no thread with ID tid exists it is considered an error.
 *
 * @return On success, return the number of quantums of the thread with ID tid. On failure, return -1.
*/
int uthread_get_quantums(int tid){
    mask_checking(SIG_BLOCK);
    if(free_tids.find(tid) != free_tids.end() || tid < 0 || tid > 99){
        std::cerr << "thread library error: no thread with ID tid exists\n";
        return -1;
    }
    mask_checking(SIG_UNBLOCK);
    return threads[tid]->get_quantum_counter();
}

