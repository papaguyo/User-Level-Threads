//
// Created by Guy Achiam on 14/04/2023.
//

#include <stdio.h>
#include <setjmp.h>
#include <signal.h>
#include <unistd.h>
#include <sys/time.h>
#include <stdbool.h>

typedef void (*thread_entry_point)(void);
#define MAX_THREAD_NUM 100 /* maximal number of threads */
#define STACK_SIZE 4096 /* stack size per thread (in bytes) */
#define FAIL (-1)



#ifndef EX2_THREAD_H
#define EX2_THREAD_H


enum State {READY, RUNNING, BLOCKED, SLEEP};

class Thread{
private:
    State _state;
    int _id;
    char *_stack;
    int _quantum_counter;
    int _quantum_usecs;
    int _wake_me_at;
    thread_entry_point _entry_point;
    sigjmp_buf _environment;

public:
    Thread(int id,int quantum_usecs,  thread_entry_point entry_point);
    State get_state() const;
    void set_state(State state_to_set);
    int get_id() const;
    int get_quantum_counter() const;
    void add_quantum_counter();
    thread_entry_point get_initial_task() const;
    void load_environment();
    int update_environment();
    int get_wake_me_at() const;
    sigjmp_buf& get_env();
    void set_wake_me_at(int num_of_quantum_to_sleep);
    ~Thread();
};
#endif //EX2_THREAD_H
