//
// Created by Guy Achiam on 14/04/2023.
//

#include <new>
#include <iostream>
#include "Thread.h"

#ifdef __x86_64__
/* code for 64 bit Intel arch */

typedef unsigned long address_t;
#define JB_SP 6
#define JB_PC 7

/* A translation is required when using an address of a variable.
   Use this as a black box in your code. */
address_t translate_address(address_t addr)
{
    address_t ret;
    asm volatile("xor    %%fs:0x30,%0\n"
                 "rol    $0x11,%0\n"
            : "=g" (ret)
            : "0" (addr));
    return ret;
}

#else
/* code for 32 bit Intel arch */

typedef unsigned int address_t;
#define JB_SP 4
#define JB_PC 5


/* A translation is required when using an address of a variable.
   Use this as a black box in your code. */
address_t translate_address(address_t addr)
{
    address_t ret;
    asm volatile("xor    %%gs:0x18,%0\n"
                 "rol    $0x9,%0\n"
    : "=g" (ret)
    : "0" (addr));
    return ret;
}


#endif


Thread::Thread(int id,int quantum_usecs, thread_entry_point entry_point) {

    this->_id = id;
    this->_quantum_counter = quantum_usecs;
    this->_entry_point = entry_point;



    if(id == 0){
        this->_state = RUNNING;
    }

    else{
        this->_state = READY;
        this->_stack = new char[STACK_SIZE];
        address_t sp = (address_t) this->_stack + STACK_SIZE - sizeof(address_t);
        auto pc = (address_t) entry_point;
        sigsetjmp(this->_environment, 1);
        (_environment->__jmpbuf)[JB_SP] = translate_address(sp);
        (_environment->__jmpbuf)[JB_PC] = translate_address(pc);
        if(sigemptyset(&_environment->__saved_mask) == FAIL){
            delete[] this->_stack;
            throw std::bad_alloc();
        }
    }




}

State Thread::get_state() const {return  this->_state;}

void Thread::set_state(State state_to_set) { this->_state = state_to_set;}

int Thread::get_id() const {return this->_id;}

int Thread::get_quantum_counter() const {return this->_quantum_counter;}

thread_entry_point Thread::get_initial_task() const {return this->_entry_point;}

void Thread::load_environment() { siglongjmp(this->_environment, 1);}

int Thread::update_environment() { return sigsetjmp(this->_environment, 1);}

jmp_buf& Thread::get_env() {return this->_environment;}

int Thread::get_wake_me_at() const {return this->_wake_me_at;}

void Thread::set_wake_me_at(int wake_me_at) {this->_wake_me_at = wake_me_at;}

Thread::~Thread() {if(this->_id != 0) delete[] _stack;}

void Thread::add_quantum_counter() {
    this->_quantum_counter++;
}



