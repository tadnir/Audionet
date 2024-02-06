#ifndef AUDIONET_UTILS_H
#define AUDIONET_UTILS_H

#include <stdbool.h>
#include <stdint.h>
#include <math.h>
#include <stddef.h>

#define ROUND_TO(number, rounder) (((float)rounder) * floor((((float)number)/((float)rounder))+0.5))

bool array_contains(unsigned int needle, size_t haystack_size, unsigned int* haystack);
uint64_t comb(int n, int r);
int compare_unsigned_ints(const unsigned int *va, const unsigned int *vb);
int compare_floats(const float* a, const float* b);
int find_max_index(size_t size, int array[]);

#endif //AUDIONET_UTILS_H
