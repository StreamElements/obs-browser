/*
 * MacOS X Crash Handler.
 *
 * The following code was used as reference when writing this module:
 * https://github.com/danzimm/mach_fun/blob/master/exceptions.c
 *
 * The best description of this code can be obtained here:
 * https://stackoverflow.com/questions/2824105/handling-mach-exceptions-in-64bit-os-x-application
 *
 * To handle mach exceptions, you have to register a mach port for the exceptions you are interested in.
 * You then wait for a message to arrive on the port in another thread. When a message arrives,
 * you call exc_server() whose implementation is provided by System.library. exec_server() takes the
 * message that arrived and calls one of three handlers that you must provide. catch_exception_raise(),
 * catch_exception_raise_state(), or catch_exception_raise_state_identity() depending on the
 * arguments you passed to task_set_exception_ports(). This is how it is done for 32 bit apps.
 *
 * For 64 bit apps, the 32 bit method still works but the data passed to you in your handler may be
 * truncated to 32 bits. To get 64 bit data passed to your handlers requires a little extra work that
 * is not very straight forward and as far as I can tell not very well documented. I stumbled onto the
 * solution by looking at the sources for GDB.
 *
 * Instead of calling exc_server() when a message arrives at the port, you have to call mach_exc_server()
 * instead. The handlers also have to have different names as well catch_mach_exception_raise(),
 * catch_mach_exception_raise_state(), and catch_mach_exception_raise_state_identity(). The parameters
 * for the handlers are the same as their 32 bit counterparts. The problem is that mach_exc_server() is
 * not provided for you the way exc_server() is. To get the implementation for mach_exc_server() requires
 * the use of the MIG (Mach Interface Generator) utility. MIG takes an interface definition file and
 * generates a set of source files that include a server function that dispatches mach messages to handlers
 * you provide. The 10.5 and 10.6 SDKs include a MIG definition file <mach_exc.defs> for the exception
 * messages and will generate the mach_exc_server() function. You then include the generated source files
 * in your project and then you are good to go.
 *
 * The nice thing is that if you are targeting 10.6+ (and maybe 10.5) you can use the same exception
 * handling for both 32 and 64 bit. Just OR the exception behavior with MACH_EXCEPTION_CODES when you set
 * your exception ports. The exception codes will come through as 64 bit values but you can truncate them
 * to 32 bits in your 32 bit build.
 *
 * I took the mach_exc.defs file and copied it to my source directory, opened a terminal and used the
 * command mig -v mach_exc.defs. This generated mach_exc.h, mach_excServer.c, and mach_excUser.c. I then
 * included those files in my project, added the correct declaration for the server function in my source
 * file and implemented my handlers. I then built my app and was good to go.
 */

#include "StreamElementsCrashHandler.hpp"
#include "StreamElementsGlobalStateManager.hpp"

//#include <stdio.h>
#include <stdlib.h>
#include <mach/mach.h>
#include <pthread.h>

#include <mutex>

//#include "mach_exc.h"

static bool initialized = false;

static void TrackCrash(const char* caller_reference)
{
    if (!initialized) return;

    blog(LOG_ERROR, "TrackCrash: %s", caller_reference);

    // This will also report the platform and a bunch of other props
    StreamElementsGlobalStateManager::GetInstance()
        ->GetAnalyticsEventsManager()
    ->trackSynchronousEvent("OBS Studio Crashed", json11::Json::object{
        { "platformCallerReference", caller_reference }
    });

    blog(LOG_ERROR, "TrackCrash: AFTER REPORT");
    
    initialized = false;
}

extern "C" {
    boolean_t mach_exc_server(mach_msg_header_t *, mach_msg_header_t *);

    kern_return_t catch_mach_exception_raise(mach_port_t exception_port, mach_port_t thread, mach_port_t task, exception_type_t type, exception_data_t code, mach_msg_type_number_t code_count) {

        TrackCrash("catch_mach_exception_raise");

        return KERN_INVALID_ADDRESS;
    }


    kern_return_t catch_mach_exception_raise_state(mach_port_t exception_port, exception_type_t exception, exception_data_t code, mach_msg_type_number_t code_count, int *flavor, thread_state_t in_state, mach_msg_type_number_t in_state_count, thread_state_t out_state, mach_msg_type_number_t *out_state_count) {

        TrackCrash("catch_mach_exception_raise_state");

        return KERN_INVALID_ADDRESS;
    }

    kern_return_t catch_mach_exception_raise_state_identity(mach_port_t exception_port, mach_port_t thread, mach_port_t task, exception_type_t exception, exception_data_t code, mach_msg_type_number_t code_count, int *flavor, thread_state_t in_state, mach_msg_type_number_t in_state_count, thread_state_t out_state, mach_msg_type_number_t *out_state_count) {
        TrackCrash("catch_mach_exception_raise_state_identity");

        return KERN_INVALID_ADDRESS;
    }

    // TODO: TBD: Copy message pump from:
    // https://gist.github.com/0x75/5884457
    static void *server_thread(void *arg) {
        mach_port_t exc_port = *(mach_port_t *)arg;
        kern_return_t kr;

        //blog(LOG_INFO, "exc_port: %d", exc_port);
        
        while(initialized) {
            //MACH_RCV_TIMED_OUT
            if ((kr = mach_msg_server_once(mach_exc_server, 4096, exc_port, 0)) != KERN_SUCCESS) {
                blog(LOG_ERROR, "mach_msg_server_once: error %#x\n", kr);
                break;
            }
        }
        return (NULL);
    }

    static void exn_init() {
      kern_return_t kr = 0;
      static mach_port_t exc_port;
      mach_port_t task = mach_task_self();
      pthread_t exception_thread;
      int err;
      
        mach_msg_type_number_t maskCount = 1;
        exception_mask_t mask;
        exception_handler_t handler;
        exception_behavior_t behavior;
        thread_state_flavor_t flavor;

        if ((kr = mach_port_allocate(task, MACH_PORT_RIGHT_RECEIVE, &exc_port)) != KERN_SUCCESS) {
            blog(LOG_ERROR, "mach_port_allocate: %#x\n", kr);
            return;
        }

        //blog(LOG_INFO, "exc_port: %d", exc_port);
        
        if ((kr = mach_port_insert_right(task, exc_port, exc_port, MACH_MSG_TYPE_MAKE_SEND)) != KERN_SUCCESS) {
            blog(LOG_ERROR, "mach_port_allocate: %#x\n", kr);
            return;
        }

        if ((kr = task_get_exception_ports(task, EXC_MASK_ALL, &mask, &maskCount, &handler, &behavior, &flavor)) != KERN_SUCCESS) {
            blog(LOG_ERROR, "task_get_exception_ports: %#x\n", kr);
            return;
        }

        if ((err = pthread_create(&exception_thread, NULL, server_thread, &exc_port)) != 0) {
            blog(LOG_ERROR, "pthread_create server_thread: %s\n", strerror(err));
            return;
        }
      
        pthread_detach(exception_thread);

        if ((kr = task_set_exception_ports(task, EXC_MASK_ALL, exc_port, EXCEPTION_DEFAULT | MACH_EXCEPTION_CODES, flavor)) != KERN_SUCCESS) {
            blog(LOG_ERROR, "task_set_exception_ports: %#x\n", kr);
            return;
        }
        
        blog(LOG_INFO, "Crash Handler Initialized");
    }
}

StreamElementsCrashHandler::StreamElementsCrashHandler()
{
    static std::mutex mutex;

    std::lock_guard<std::mutex> guard(mutex);

    if (!initialized) {
        initialized = true;
        
        exn_init();
        
        //exit(0);
    }
}

StreamElementsCrashHandler::~StreamElementsCrashHandler()
{
    // We never destroy the exception handler
    initialized = false;
}
