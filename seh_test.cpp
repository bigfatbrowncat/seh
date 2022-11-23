#include <stdio.h>
#include <math.h>
#include <float.h>

#include <memory>

#define SEH_IMPL
#include "seh.h"

#define REPORT(T, MSG) if (T) { printf("%s: SUCCESS\n", MSG); } else { printf("%s: FAIL\n", MSG); }

BOOL test_segfault() {
    BOOL s1 = FALSE, s2 = FALSE;

    seh_enter
    {
        int* ptr = NULL;
        *ptr = 0; /* Throw exception here */
    }
    seh_handle (seh_get() == SEH_MEMORYACCESS)
    {
        s1 = TRUE;
    }
    seh_exit
    {
        s2 = TRUE;
    }

    return s1 && s2;
}

BOOL test_div_int_zero() {
    BOOL s1 = FALSE, s2 = FALSE;

    seh_enter
    {
        int a = 3, b = 0, c = -1;
        c = a / b;
        printf("%d\n", c);
    }
    seh_handle (seh_get() == SEH_ARITHMETICS)
    {
        s1 = TRUE;
    }
    seh_exit
    {
        s2 = TRUE;
    }

    return s1 && s2;
}

BOOL test_div_float_zero() {
    _control87(_EM_INVALID, _EM_ZERODIVIDE);

    BOOL s1 = FALSE, s2 = FALSE;

    seh_enter
    {
        float a = 3, b = 0, c = -1;
        c = a / b;
        printf("%f\n", c);
    }
    seh_handle (seh_get() == SEH_ARITHMETICS)
    {
        s1 = TRUE;
    }
    seh_exit
    {
        s2 = TRUE;
    }

    return s1 && s2;
}


BOOL test_denormal() {
    BOOL s1 = FALSE, s2 = FALSE;

    seh_enter
    {
        _control87(_EM_INVALID, _EM_DENORMAL);

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
        s1 = TRUE;
    }
    seh_exit
    {
        s2 = TRUE;
    }

    return s1 && s2;
}

void recursive_stack_overflow() {
    recursive_stack_overflow();
}

BOOL test_stack_overflow() {
    BOOL s1 = FALSE, s2 = FALSE;

    seh_enter
    {
        recursive_stack_overflow();
    }
    seh_handle (seh_get() == SEH_STACKOVERFLOW)
    {
        s1 = TRUE;
    }
    seh_exit
    {
        s2 = TRUE;
    }

    return s1 && s2;
}

BOOL test_thousand_segfaults() {
    for (int i = 0; i < 1000; i++) {
        if (!test_segfault()) return FALSE;
    }
    return TRUE;
}

BOOL test_check_inside_check_segfault() {
    BOOL s1 = FALSE, s2 = FALSE, s3 = FALSE;

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
            s1 = TRUE;
        }
        seh_exit
        {
            s2 = seh_stack_pointer == ptr_start + 1;
        }
    }
    seh_handle (seh_get() == SEH_MEMORYACCESS)
    {
        s1 = FALSE;
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
    REPORT(test_stack_overflow(), "Stack overflow");
    REPORT(test_check_inside_check_segfault(), "Check inside the check");

    //recursive_stack_overflow();

    return 0;
}
