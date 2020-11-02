/*
 * MacOS X Crash Handler.
 *
 * Portions of code are Copyright (c) 2003, Brian Alliet. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * 
 * 1. Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 * 
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 * 
 * 3. Neither the name of the author nor the names of contributors may be used
 * to endorse or promote products derived from this software without specific
 * prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS `AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "StreamElementsCrashHandler.hpp"

#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <mach/mach.h>
#include <mach/mach_error.h>
#include <mach/thread_status.h>
#include <mach/exception.h>
#include <mach/task.h>
#include <pthread.h>
#include <sys/mman.h>

#include "mach_exc.h"
     

#define DIE(x) do { fprintf(stderr,"%s failed at %d\n",x,__LINE__); exit(1); } while(0)
#define ABORT(x) do { fprintf(stderr,"%s at %d\n",x,__LINE__); } while(0)

/* this is not specific to mach exception handling, its just here to separate required mach code from YOUR code */
static int my_handle_exn(char *addr, integer_t code);


/* These are not defined in any header, although they are documented */
extern boolean_t mach_exc_server(mach_msg_header_t *,mach_msg_header_t *);
extern kern_return_t exception_raise(
    mach_port_t,mach_port_t,mach_port_t,
    exception_type_t,exception_data_t,mach_msg_type_number_t);
extern kern_return_t exception_raise_state(
    mach_port_t,mach_port_t,mach_port_t,
    exception_type_t,exception_data_t,mach_msg_type_number_t,
    thread_state_flavor_t*,thread_state_t,mach_msg_type_number_t,
    thread_state_t,mach_msg_type_number_t*);
extern kern_return_t exception_raise_state_identity(
    mach_port_t,mach_port_t,mach_port_t,
    exception_type_t,exception_data_t,mach_msg_type_number_t,
    thread_state_flavor_t*,thread_state_t,mach_msg_type_number_t,
    thread_state_t,mach_msg_type_number_t*);

#define MAX_EXCEPTION_PORTS 16


static mach_port_t exception_port;

static void *exc_thread(void *junk) {
    mach_msg_return_t r;
    /* These two structures contain some private kernel data. We don't need to
       access any of it so we don't bother defining a proper struct. The
       correct definitions are in the xnu source code. */
    struct {
        mach_msg_header_t head;
        char data[256];
    } reply;
    struct {
        mach_msg_header_t head;
        mach_msg_body_t msgh_body;
        char data[1024];
    } msg;
    
    for(;;) {
        r = mach_msg(
            &msg.head,
            MACH_RCV_MSG|MACH_RCV_LARGE,
            0,
            sizeof(msg),
            exception_port,
            MACH_MSG_TIMEOUT_NONE,
            MACH_PORT_NULL);
        if(r != MACH_MSG_SUCCESS) DIE("mach_msg");
        
        /* Handle the message (calls catch_exception_raise) */
        if(!mach_exc_server(&msg.head,&reply.head)) DIE("mach_exc_server");
        
        /* Send the reply */
        r = mach_msg(
            &reply.head,
            MACH_SEND_MSG,
            reply.head.msgh_size,
            0,
            MACH_PORT_NULL,
            MACH_MSG_TIMEOUT_NONE,
            MACH_PORT_NULL);
        if(r != MACH_MSG_SUCCESS) DIE("mach_msg");
    }
    /* not reached */
}

static void exn_init() {
    kern_return_t r;
    mach_port_t me;
    pthread_t thread;
    pthread_attr_t attr;
    exception_mask_t mask;
    
    me = mach_task_self();
    r = mach_port_allocate(me,MACH_PORT_RIGHT_RECEIVE,&exception_port);
    if(r != MACH_MSG_SUCCESS) DIE("mach_port_allocate");
    
    r = mach_port_insert_right(me,exception_port,exception_port,
      MACH_MSG_TYPE_MAKE_SEND);
    if(r != MACH_MSG_SUCCESS) DIE("mach_port_insert_right");
    
    /* for others see mach/exception_types.h */
    mask = EXC_MASK_BAD_ACCESS;
    
    /* set the new exception ports */
    r = task_set_exception_ports(me,mask,exception_port,EXCEPTION_DEFAULT,MACHINE_THREAD_STATE);
    if(r != MACH_MSG_SUCCESS) DIE("task_set_exception_ports");
    
    if(pthread_attr_init(&attr) != 0) DIE("pthread_attr_init");
    if(pthread_attr_setdetachstate(&attr,PTHREAD_CREATE_DETACHED) != 0) 
        DIE("pthread_attr_setdetachedstate");
    
    if(pthread_create(&thread,&attr,exc_thread,NULL) != 0)
        DIE("pthread_create");

    pthread_attr_destroy(&attr);
}

kern_return_t catch_mach_exception_raise(mach_port_t exception_port,mach_port_t thread,mach_port_t task,exception_type_t exception,exception_data_t code,mach_msg_type_number_t code_count) {
    kern_return_t r;
    char *addr;

    thread_state_flavor_t flavor = x86_EXCEPTION_STATE;
    mach_msg_type_number_t exc_state_count = x86_EXCEPTION_STATE_COUNT;
    x86_exception_state_t exc_state;

    /* we should never get anything that isn't EXC_BAD_ACCESS, but just in case */
    if(exception != EXC_BAD_ACCESS) {
        /* We aren't interested, pass it on to the old handler */
        fprintf(stderr,"Exception: 0x%x Code: 0x%x 0x%x in catch....\n",
            exception,
            code_count > 0 ? code[0] : -1,
            code_count > 1 ? code[1] : -1); 
        return 1;
    }

    r = thread_get_state(thread,flavor, (natural_t*)&exc_state,&exc_state_count);
    if (r != KERN_SUCCESS) DIE("thread_get_state");
    
    /* This is the address that caused the fault */
    addr = (char*) exc_state.ues.es64.__faultvaddr;

    /* you could just as easily put your code in here, I'm just doing this to 
       point out the required code */
    if(!my_handle_exn(addr, code[0])) return 1;
    
    return KERN_SUCCESS;
}
kern_return_t catch_mach_exception_raise_state(mach_port_name_t exception_port,
    int exception, exception_data_t code, mach_msg_type_number_t codeCnt,
    int flavor, thread_state_t old_state, int old_stateCnt,
    thread_state_t new_state, int new_stateCnt)
{
    ABORT("catch_exception_raise_state");
    return(KERN_INVALID_ARGUMENT);
}
kern_return_t catch_mach_exception_raise_state_identity(
    mach_port_name_t exception_port, mach_port_t thread, mach_port_t task,
    int exception, exception_data_t code, mach_msg_type_number_t codeCnt,
    int flavor, thread_state_t old_state, int old_stateCnt,
    thread_state_t new_state, int new_stateCnt)
{
    ABORT("catch_exception_raise_state_identity");
    return(KERN_INVALID_ARGUMENT);
}

static char *data;

static int my_handle_exn(char *addr, integer_t code) {
    if(code == KERN_INVALID_ADDRESS) {
        fprintf(stderr,"Got KERN_INVALID_ADDRESS at %p\n",addr);
        exit(1);
    }
    if(code == KERN_PROTECTION_FAILURE) {
        fprintf(stderr,"Got KERN_PROTECTION_FAILURE at %p\n",addr);
        if(addr == NULL) {
            fprintf(stderr,"Tried to dereference NULL");
            exit(1);
        }
        if(addr == data) {
            fprintf(stderr,"Making data (%p) writeable\n",addr);
            if(mprotect(addr,4096,PROT_READ|PROT_WRITE) < 0) DIE("mprotect");
            return 1; // we handled it
        }
        fprintf(stderr,"Got KERN_PROTECTION_FAILURE at %p\n",addr);
        return 0; // forward it
    }

    /* You should filter out anything you don't want in the catch_exception_raise... above
        and forward it */
    fprintf(stderr,"Got unknown code %d at %p\n",(int)code,addr);
    return 0;
}

StreamElementsCrashHandler::StreamElementsCrashHandler()
{
  exn_init();
}

StreamElementsCrashHandler::~StreamElementsCrashHandler()
{

}
