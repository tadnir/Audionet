#include "utils.h"

bool array_contains(unsigned int needle, size_t haystack_size, unsigned int* haystack) {
    for (int i = 0; i < haystack_size; ++i) {
        if (needle == haystack[i]) {
            return true;
        }
    }

    return false;
}

uint64_t comb(int n, int r) {
    uint64_t sum = 1;
    /* Calculate the value of n choose r using the binomial coefficient formula */
    for(int i = 1; i <= r; i++) {
        sum = sum * (n - r + i) / i;
    }

    return sum;
}

int compare_unsigned_ints(const unsigned int *a, const unsigned int *b) {
    unsigned int av = *a, bv = *b;
    return av < bv ? -1 : av > bv ? 1 : 0;
}

int compare_floats(const float* a, const float* b) {
    if ((*(float*)a - *(float*)b) > 0) {
        return 1;
    } else if ((*(float*)a - *(float*)b) < 0) {
        return -1;
    }

    return 0;
}

int find_max_index(size_t size, int array[]) {
    int max = 0;
    int index = 0;
    for (int i = 0; i < size; ++i) {
        if (array[i] > max) {
            max = array[i];
            index = i;
        }
    }

    return index;
}