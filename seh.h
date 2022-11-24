#ifndef __SEH_H__
#define __SEH_H__

#include <setjmp.h>

#ifdef _WIN32
#include <Windows.h>
#else
#include <signal.h>
#include <pthread.h>
#endif

#ifndef SEH_API
#define SEH_API
#endif

/**
 * Exception code
 */
#define SEH_OTHER           -0x9
#define SEH_LEAVE           -0x9999
#define SEH_ABORT           -0x1
#define SEH_ARITHMETICS     -0x2
#define SEH_USER_INTERRUPT  -0x3
#define SEH_ILLCODE         -0x4
#define SEH_MISALIGN        -0x5
#define SEH_MEMORYACCESS    -0x6
#define SEH_OUTBOUNDS       -0x7
#define SEH_STACKERROR   -0x8

typedef struct seh
{
    void*   saved;
    jmp_buf jmpbuf;
} seh_t;

#define seh_enter { seh_t* seh_local_ctx = (seh_t*) malloc(sizeof(seh_t)); seh_begin(seh_local_ctx); if (setjmp(seh_local_ctx->jmpbuf) == 0)
#define seh_handle else if (seh_get() != SEH_LEAVE)
#define seh_exit seh_end(seh_local_ctx); }

SEH_API int  seh_get(void);
SEH_API void seh_leave(void);
SEH_API void seh_throw(int value);

// Internal functions

SEH_API void seh_begin(seh_t* ctx);
SEH_API void seh_end(seh_t* ctx);

#endif /* __SEH_H__ */

#ifdef SEH_IMPL

#ifndef SEH_STACK_SIZE
#define SEH_STACK_SIZE 64
#endif

static int    seh_value;
static int    seh_stack_pointer = 0;
static seh_t* seh_stack[SEH_STACK_SIZE];

#if defined(_WIN32)
static LONG WINAPI seh_sighandler(EXCEPTION_POINTERS* info)
{
    switch (info->ExceptionRecord->ExceptionCode)
    {
    case EXCEPTION_FLT_OVERFLOW:
    case EXCEPTION_INT_OVERFLOW:
    case EXCEPTION_FLT_UNDERFLOW:
    case EXCEPTION_FLT_STACK_CHECK:
    case EXCEPTION_FLT_DIVIDE_BY_ZERO:
    case EXCEPTION_INT_DIVIDE_BY_ZERO:
    case EXCEPTION_FLT_INEXACT_RESULT:
    case EXCEPTION_FLT_DENORMAL_OPERAND:
    case EXCEPTION_FLT_INVALID_OPERATION:
        seh_throw(SEH_ARITHMETICS);
        //seh_value = SEH_FLOAT;
        break;

    case EXCEPTION_ILLEGAL_INSTRUCTION:
    case EXCEPTION_PRIV_INSTRUCTION:
        seh_throw(SEH_ILLCODE);
        //seh_value = SEH_ILLCODE;
        break;

    case EXCEPTION_STACK_OVERFLOW:
        seh_throw(SEH_STACKERROR);
        //seh_value = SEH_STACKERROR;
        break;
	
    case EXCEPTION_ACCESS_VIOLATION:
    case EXCEPTION_IN_PAGE_ERROR:
    case EXCEPTION_GUARD_PAGE:
        seh_throw(SEH_MEMORYACCESS);
        //seh_value = SEH_SEGFAULT;
        break;
	
    case EXCEPTION_ARRAY_BOUNDS_EXCEEDED:
        seh_throw(SEH_OUTBOUNDS);
        //seh_value = SEH_OUTBOUNDS;
        break;

    case EXCEPTION_DATATYPE_MISALIGNMENT:
        seh_throw(SEH_MISALIGN);
        //seh_value = SEH_MISALIGN;
        break;

    case EXCEPTION_NONCONTINUABLE_EXCEPTION:
        seh_throw(SEH_OTHER);
        break;

    default:
        //seh_throw(SEH_NONE);      // Do nothing
        //seh_value = SEH_NONE;
        break;
    }

    return seh_value != SEH_LEAVE 
        ? EXCEPTION_CONTINUE_EXECUTION 
        : EXCEPTION_CONTINUE_SEARCH;
}

#else // defined(_WIN32)

static stack_t* seh_signal_stack;

int check_stack_error(siginfo_t* info) {
    pthread_attr_t attr;
    void * stack_addr;
    int * plocal_var;
    size_t stack_size;
    void* sig_addr;
    sig_addr = info->si_addr;

#ifdef __APPLE__
    // pthread_get_stacksize_np() returns a value too low for the main thread on
    // OSX 10.9, http://mail.openjdk.java.net/pipermail/hotspot-dev/2013-October/011369.html
    //
    // Multiple workarounds possible, adopt the one made by https://github.com/robovm/robovm/issues/274
    // https://developer.apple.com/library/mac/documentation/Cocoa/Conceptual/Multithreading/CreatingThreads/CreatingThreads.html
    // Stack size for the main thread is 8MB on OSX excluding the guard page size.
    pthread_t thread = pthread_self();
    stack_size = pthread_main_np() ? (8 * 1024 * 1024) : pthread_get_stacksize_np(thread);

    // stack address points to the start of the stack, not the end how it's returned by pthread_get_stackaddr_np
    stack_addr = reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(pthread_get_stackaddr_np(thread)) - stack_size);
#else
    pthread_getattr_np(pthread_self(), &attr);
    pthread_attr_getstack( &attr, &stack_addr, &stack_size );

    if (pthread_attr_getstackaddr(&attr, &stack_addr) != 0) {
        exit(1);
    }
#endif

    //printf( "stackaddr = %p, stacksize = %lu, signal address = %p\n", stack_addr, stack_size, sig_addr );
        
    // The error has occurred inside the stack
    return (size_t)sig_addr <= (size_t)stack_addr && (size_t)sig_addr >= (size_t)stack_addr - (size_t)stack_size;
}

static void seh_sighandler(int sig, siginfo_t* info, void* context)
{
    (void)info;
    (void)context;
    switch (sig)
    {
    case SIGBUS:
        if (check_stack_error(info)) {
            // According to https://stackoverflow.com/questions/62432847/is-there-a-way-to-catch-stack-overflow-in-a-process-c-linux
            // sometimes stack overflow in linux could raise SIGBUS
            // Hopefully in this case the error is still inside the stack...
            seh_throw(SEH_STACKERROR);
        } else {
            seh_throw(SEH_MISALIGN);
        }
        break;

//    case SIGSYS:
//        seh_throw(SEH_SYSCALL);
//        break;

    case SIGFPE:
        seh_throw(SEH_ARITHMETICS);
        break;
	
    case SIGILL:
        seh_throw(SEH_ILLCODE);
        break;

    case SIGABRT:
        seh_throw(SEH_ABORT);
        break;

    case SIGINT:
        seh_throw(SEH_USER_INTERRUPT);
        break;

    case SIGSEGV:
        if (check_stack_error(info)) {
            seh_throw(SEH_STACKERROR);
        } else {
            seh_throw(SEH_MEMORYACCESS);
        }
        break;
	
    default:
        //seh_throw(SEH_NONE);   // Do nothing
        break;
    }
}
#endif

#if !defined(_WIN32)
const int seh_signals[] = {
    SIGABRT, SIGINT, SIGFPE, SIGSEGV, SIGILL, SIGSYS, SIGBUS,
};
#endif

int seh_get(void)
{
    return seh_value;
}

void seh_leave(void)
{
    seh_throw(SEH_LEAVE);
}

void seh_throw(int value)
{
    seh_value = value;
    seh_t* ctx = seh_stack[seh_stack_pointer - 1];
    longjmp(ctx->jmpbuf, 1);
}

void seh_end(seh_t* ctx)
{
    if (ctx == seh_stack[seh_stack_pointer - 1])
    {
    #if defined(_WIN32)
        SetUnhandledExceptionFilter((LPTOP_LEVEL_EXCEPTION_FILTER)ctx->saved);
    #else
        int idx;
        for (idx = 0; idx < sizeof(seh_signals) / sizeof(seh_signals[0]); idx++)
        {
            if (sigaction(seh_signals[idx], &((struct sigaction*)ctx->saved)[idx], NULL) != 0)
            {
                break;
            }
        }
        free(ctx->saved);
    #endif

        seh_stack_pointer -= 1;
    }
}

void seh_begin(seh_t* ctx)
{
    if (ctx == seh_stack[seh_stack_pointer])
    {
        return;
    }

#if defined(_WIN32)
    ctx->saved = (void*)SetUnhandledExceptionFilter(seh_sighandler);
#else
    if (seh_signal_stack == NULL) {
        // Creating a special stack for signal processing (to handle SIGSEGV correctly)

        char* stack_buffer = (char*)malloc(SIGSTKSZ);

        seh_signal_stack = (stack_t*)malloc(sizeof(stack_t));
        seh_signal_stack->ss_size = SIGSTKSZ;
        seh_signal_stack->ss_sp = stack_buffer;

        if (sigaltstack(seh_signal_stack, 0) < 0) {
            exit(1);
        }
    }

    ctx->saved = malloc(sizeof(struct sigaction) * (sizeof(seh_signals) / sizeof(seh_signals[0])));

    int idx;
    struct sigaction sa, old;
    sigemptyset(&sa.sa_mask);
    sa.sa_handler   = NULL;
    sa.sa_sigaction = seh_sighandler;
    sa.sa_flags     = SA_SIGINFO | SA_RESTART | SA_NODEFER | SA_ONSTACK;
    for (idx = 0; idx < sizeof(seh_signals) / sizeof(seh_signals[0]); idx++)
    {
        if (sigaction(seh_signals[idx], &sa, &((struct sigaction*)ctx->saved)[idx]) != 0)
        {
            free(ctx->saved);
            return;
        }
    }
#endif

    seh_stack[seh_stack_pointer++] = ctx;
}

#endif /* SEH_IMPL */
