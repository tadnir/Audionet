#ifndef AUDIONET_UTILS_H
#define AUDIONET_UTILS_H

#include <stdbool.h>
#include <stdint.h>
#include <math.h>
#include <stddef.h>

/**
 * Maximum value macro.
 *
 * @param a The first element.
 * @param b The second element.
 * @return The larger element.
 */
#ifndef max
#define max(a,b) (((a) > (b)) ? (a) : (b))
#endif


/**
 * Minimum value macro.
 *
 * @param a The first element.
 * @param b The second element.
 */
#ifndef min
#define min(a,b) (((a) < (b)) ? (a) : (b))
#endif

/**
 * Round macro to the nearest rounder value.
 * For example:
 *    number=6, rounder=5 => out=5
 *    number=15.1, rounder=10 => out=20
 *
 * @param number The number to round.
 * @param rounder The number to round to.
 * @return The rounded number.
 */
#define ROUND_TO(number, rounder) (((float)rounder) * floor((((float)number)/((float)rounder))+0.5))

/**
 * Checks whether needle is in haystack.
 *
 * @param needle The number to search.
 * @param haystack_size The amount of elements in the collection to search in.
 * @param haystack The collection of elements to search needle in.
 * @return Whether needle is contained in the haystack.
 */
bool array_contains(unsigned int needle, size_t haystack_size, unsigned int* haystack);

/**
 * The combinatorics choose function.
 *
 * @param n
 * @param r
 * @return The calculated choose result.
 */
uint64_t comb(int n, int r);

/**
 * Compare callback for sorting unsigned ints.
 *
 * @param va First value pointer.
 * @param vb Second value pointer.
 * @return The order indicator between va and vb.
 */
int compare_unsigned_ints(const unsigned int *va, const unsigned int *vb);

/**
 * Compare callback for sorting floats.
 * @param a First value pointer.
 * @param b Second value pointer.
 * @return The order indicator between a and b.
 */
int compare_floats(const float* a, const float* b);

/**
 * Finds the index of the largest element in the array.
 *
 * @param size The size of the array.
 * @param array The array to search in.
 * @return The index of the largest element.
 */
int find_max_index(size_t size, int array[]);

#endif //AUDIONET_UTILS_H
