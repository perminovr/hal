
#include <stdio.h>
#include "hal_time.h"

int main(int argc, const char **argv)
{
    switch (atoi(argv[1])) {
        case 1: {
            if (Hal_getTimeInMs() != 0) return 0;
        } break;
    }

    return 1;
}