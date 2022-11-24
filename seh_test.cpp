#include <stdio.h>
#include <math.h>

#ifdef _WIN32
#include <float.h>
#else
#include <fenv.h>
#endif

#define SEH_IMPL
#include "seh.h"

#define REPORT(T, MSG) if (T) { printf("%s: SUCCESS\n", MSG); } else { printf("%s: FAIL\n", MSG); }

#ifdef __APPLE__ // && defined(__MACH__)
// Public domain polyfill for feenableexcept on OS X
// http://www-personal.umich.edu/~williams/archive/computation/fe-handling-example.c

inline int feenableexcept(unsigned int excepts)
{
    static fenv_t fenv;
    unsigned int new_excepts = excepts & FE_ALL_EXCEPT;
    // previous masks
    unsigned int old_excepts;

    if (fegetenv(&fenv)) {
        return -1;
    }
    old_excepts = fenv.__control & FE_ALL_EXCEPT;

    // unmask
    fenv.__control &= ~new_excepts;
    fenv.__mxcsr   &= ~(new_excepts << 7);

    return fesetenv(&fenv) ? -1 : old_excepts;
}

inline int fedisableexcept(unsigned int excepts)
{
    static fenv_t fenv;
    unsigned int new_excepts = excepts & FE_ALL_EXCEPT;
    // all previous masks
    unsigned int old_excepts;

    if (fegetenv(&fenv)) {
        return -1;
    }
    old_excepts = fenv.__control & FE_ALL_EXCEPT;

    // mask
    fenv.__control |= new_excepts;
    fenv.__mxcsr   |= new_excepts << 7;

    return fesetenv(&fenv) ? -1 : old_excepts;
}

#endif

void enable_fp_traps() {
#if defined(_WIN32)
    _control87(_EM_INVALID, _EM_OVERFLOW | _EM_UNDERFLOW | _EM_INEXACT | _EM_ZERODIVIDE | _EM_DENORMAL);
#else
    feenableexcept(FE_DIVBYZERO);
    feenableexcept(FE_INEXACT);
    feenableexcept(FE_INVALID); 
    feenableexcept(FE_OVERFLOW);
    feenableexcept(FE_UNDERFLOW); 
#endif
}

bool test_segfault() {
    bool s1 = false, s2 = false;

    seh_enter
    {
        int* ptr = NULL;
        *ptr = 0; /* Throw exception here */
    }
    seh_handle
    {
        switch (seh_get()) {
            case SEH_MEMORYACCESS: s1 = true; break;
        }
    }
    seh_exit
    {
        s2 = true;
    }

    return s1 && s2;
}

bool test_div_int_zero() {
    bool s1 = false, s2 = false;

    enable_fp_traps();

    seh_enter
    {
        int a = 3, b = 0, c = -1;
        c = a / b;
        printf("%d\n", c);
    }
    seh_handle
    {
        switch (seh_get()) {
            case SEH_ARITHMETICS: s1 = true; break;
        }
    }
    seh_exit
    {
        s2 = true;
    }

    return s1 && s2;
}

bool test_div_float_zero() {
    bool s1 = false, s2 = false;

    enable_fp_traps();

    seh_enter
    {
        float a = 3, b = 0, c = -1;
        c = a / b;
        printf("%f\n", c);
    }
    seh_handle
    {
        switch (seh_get()) {
            case SEH_ARITHMETICS: s1 = true; break;
        }
    }
    seh_exit
    {
        s2 = true;
    }

    return s1 && s2;
}


bool test_denormal() {
    bool s1 = false, s2 = false;

    enable_fp_traps();

    seh_enter
    {
        const unsigned ux = 0xf; // denormal as float.
        const unsigned uy = 0x7; // denormal as float.

        //auto x = (float)ux; // this obviouly doesn't work as you end up with 15.f.

        float* px = (float*)&ux;
        float* py = (float*)&uy;

        float x = *px; // denormal
        float y = *py; // denormal

        float sum = 0.0f;
        for (int i = 0; i < 1000; i++) {
            sum += x / y;
        }

        printf("%f", sum);
    }
    seh_handle
    {
        switch (seh_get()) {
            case SEH_ARITHMETICS: s1 = true; break;
        }
    }
    seh_exit
    {
        s2 = true;
    }

    return s1 && s2;
}

void recursive_stack_overflow() {
    printf("%s", "");       // Doing literally nothing, but fooling the optimizer
    recursive_stack_overflow();
}

bool test_stack_overflow() {
    bool s1 = false, s2 = false;

    seh_enter
    {
        recursive_stack_overflow();
    }
    seh_handle
    {
        switch (seh_get()) {
            case SEH_STACKERROR: s1 = true; break;
        }
    }
    seh_exit
    {
        s2 = true;
    }

    return s1 && s2;
}

bool test_thousand_segfaults() {
    for (int i = 0; i < 1000; i++) {
        if (!test_segfault()) return false;
    }
    return true;
}

bool test_check_inside_check_segfault() {
    bool s1 = false, s2 = false, s3 = false;

    int ptr_start = seh_stack_pointer;

    seh_enter
    {
        seh_enter
        {
            int* ptr = NULL;
            *ptr = 0; /* Throw exception here */
        }
        seh_handle
        {
            switch (seh_get()) {
                case SEH_MEMORYACCESS: s1 = true; break;       // It should be handled here
            }
        }
        seh_exit
        {
            s2 = seh_stack_pointer == ptr_start + 1;
        }
    }
    seh_handle
    {
        switch (seh_get()) {
            case SEH_MEMORYACCESS: s1 = false; break;       // It should NOT be handled here
        }
    }
    seh_exit
    {
        s3 = seh_stack_pointer == ptr_start;
    }

    return s1 && s2 && s3;
}


int main(int argc, char* argv[])
{
    REPORT(test_segfault(), "Single segmentation fault");
    REPORT(test_thousand_segfaults(), "Thousand segmentation faults");
    REPORT(test_div_int_zero(), "Integer division by zero");
    REPORT(test_div_float_zero(), "Floating-point division by zero");
    REPORT(test_denormal(), "Denormalization");
    REPORT(test_check_inside_check_segfault(), "Check inside the check");
    REPORT(test_stack_overflow(), "Stack overflow");
	
    return 0;
}
