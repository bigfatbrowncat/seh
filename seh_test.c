#include <stdio.h>

#define SEH_IMPL
#include "seh.h"

int main(int argc, char* argv[])
{
    int count = 1000;
    while (count-- > 0)
    {
		seh_t* seh = (seh_t*) malloc(sizeof(seh_t));
        seh_enter (*seh)
		{
			int* ptr = NULL;
			*ptr = 0; /* Throw exception here */
		}
		seh_catch (seh_get() == SEH_SEGFAULT)
		{
			printf("Segment fault exception has been thrown\n");
		}
		seh_exit (*seh)
		{
			printf("Finally of try/catch: SP=%d\n", seh_stack_pointer);
		}
    }
    
    return 0;
}
