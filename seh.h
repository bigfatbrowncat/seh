#ifndef __SEH_H__
#define __SEH_H__

#include <setjmp.h>

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
//#define SEH_SYSCALL         -0x3
#define SEH_ILLCODE         -0x4
#define SEH_MISALIGN        -0x5
#define SEH_MEMORYACCESS    -0x6
#define SEH_OUTBOUNDS       -0x7
#define SEH_STACKOVERFLOW   -0x8

typedef struct seh
{
    void*   saved;
    jmp_buf jmpbuf;
} seh_t;

#define seh_enter { seh_t* seh_local_ctx = (seh_t*) malloc(sizeof(seh_t)); seh__begin(seh_local_ctx); if (setjmp(seh_local_ctx->jmpbuf) == 0)
#define seh_handle(exp)  else if ((seh_get() != SEH_LEAVE) && (exp))
#define seh_exit seh__end(seh_local_ctx); }
//#define seh_throw(i)     cur_value = i; longjmp(ctx, 1)

SEH_API int  seh_get(void);
SEH_API void seh_leave(void);
SEH_API void seh_throw(int value);

// Internal functions

SEH_API void seh__begin(seh_t* ctx);
SEH_API void seh__end(seh_t* ctx);

#endif /* __SEH_H__ */

#ifdef SEH_IMPL

#ifndef SEH_STACK_SIZE
#define SEH_STACK_SIZE 64
#endif

static int    seh_value;
static int    seh_stack_pointer = 0;
static seh_t* seh_stack[SEH_STACK_SIZE];

#if defined(_WIN32)
#include <Windows.h>
static LONG WINAPI seh__sighandler(EXCEPTION_POINTERS* info)
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
        seh_throw(SEH_STACKOVERFLOW);
        //seh_value = SEH_STACKOVERFLOW;
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
#else
static void seh__sighandler(int sig, siginfo_t* info, void* context)
{
    (void)info;
    (void)context;
    switch (sig)
    {
    case SIGBUS:
        seh_throw(SEH_MISALIGN);
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

    case SIGSEGV:
        seh_throw(SEH_SEGFAULT);
        break;
	
    default:
        //seh_throw(SEH_NONE);   // Do nothing
        break;
    }
}
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

void seh__end(seh_t* ctx)
{
    if (ctx == seh_stack[seh_stack_pointer - 1])
    {
    #if defined(_WIN32)
        SetUnhandledExceptionFilter((LPTOP_LEVEL_EXCEPTION_FILTER)ctx->saved);
    #else
        int idx;
        for (idx = 0; idx < sizeof(seh_signals) / sizeof(seh_signals[0]); idx++)
        {
            if (sigaction(seh_signals[idx], ((struct sigaction*)ctx->saved)[i], NULL) != 0)
            {
                break;
            }
        }
        free(ctx->saved);
    #endif

        seh_stack_pointer -= 1;
    }
}

#if !defined(_WIN32)
const int seh_signals[] = {
    SIGABRT, SIGFPE, SIGSEGV, SIGILL, SIGSYS, SIGBUS,
};
#endif

void seh__begin(seh_t* ctx)
{
    if (ctx == seh_stack[seh_stack_pointer])
    {
        return;
    }

#if defined(_WIN32)
    ctx->saved = (void*)SetUnhandledExceptionFilter(seh__sighandler);
#else
    ctx->saved = malloc(sizeof(struct sigaction) * (sizeof(seh_signals) / sizeof(seh_signals[0])));

    int idx;
    struct sigaction sa, old;
    sigemptyset(&sa.sa_mask);
    sa.sa_handler   = NULL;
    sa.sa_sigaction = seh__sighandler;
    sa.sa_flags     = SA_SIGINFO | SA_RESTART | SA_NODEFER;
    for (idx = 0; idx < sizeof(seh_signals) / sizeof(seh_signals[0]); idx++)
    {
        if (sigaction(seh_signals[idx], &sa, ((struct sigaction*)ctx->saved)[i]) != 0)
        {
            free(ctx->saved);
            return;
        }
    }
#endif

    seh_stack[seh_stack_pointer++] = ctx;
}

#endif /* SEH_IMPL */