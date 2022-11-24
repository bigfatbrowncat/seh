#include <stdio.h>
#include <math.h>

#include <memory>

#ifdef _WIN32
#include <float.h>
#else
#include <fenv.h>
#endif

#define SEH_IMPL
#include "seh.h"

#define	_EM_INVALID	0x00000010

#define REPORT(T, MSG) if (T) { printf("%s: SUCCESS\n", MSG); } else { printf("%s: FAIL\n", MSG); }

void enable_fp_traps() {
#ifdef _WIN32
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
    seh_handle (seh_get() == SEH_MEMORYACCESS)
    {
        s1 = true;
    }
    seh_exit
    {
        s2 = true;
    }

    return s1 && s2;
}

bool test_div_int_zero() {
    bool s1 = false, s2 = false;

    seh_enter
    {
        int a = 3, b = 0, c = -1;
        c = a / b;
        printf("%d\n", c);
    }
    seh_handle (seh_get() == SEH_ARITHMETICS)
    {
        s1 = true;
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
    seh_handle (seh_get() == SEH_ARITHMETICS)
    {
        s1 = true;
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
    seh_handle (seh_get() == SEH_ARITHMETICS)
    {
        s1 = true;
    }
    seh_exit
    {
        s2 = true;
    }

    return s1 && s2;
}

void recursive_stack_overflow() {
    printf("%s", "");
    recursive_stack_overflow();
}

bool test_stack_overflow() {
    bool s1 = false, s2 = false;

    seh_enter
    {
        recursive_stack_overflow();
    }
    seh_handle (seh_get() == SEH_STACKERROR)
    {
        s1 = true;
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
        seh_handle (seh_get() == SEH_MEMORYACCESS)
        {
            s1 = true;
        }
        seh_exit
        {
            s2 = seh_stack_pointer == ptr_start + 1;
        }
    }
    seh_handle (seh_get() == SEH_MEMORYACCESS)
    {
        s1 = false;
    }
    seh_exit
    {
        s3 = seh_stack_pointer == ptr_start;
    }

    return s1 && s2 && s3;
}


int main(int argc, char* argv[])
{
    enable_fp_traps();

    REPORT(test_segfault(), "Single segmentation fault");
    REPORT(test_thousand_segfaults(), "Thousand segmentation faults");
    REPORT(test_div_int_zero(), "Integer division by zero");
    REPORT(test_div_float_zero(), "Floating-point division by zero");
    REPORT(test_denormal(), "Denormalization");
    REPORT(test_check_inside_check_segfault(), "Check inside the check");
    REPORT(test_stack_overflow(), "Stack overflow");
	
    return 0;
}
