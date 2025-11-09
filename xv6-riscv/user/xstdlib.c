/**
 * @file xstdlib.c
 * @brief Minimal standard library extensions for xv6 user programs.
 *
 * @details
 * Implements safe memory allocation, basic numeric conversions, and
 * general-purpose sorting and searching routines usable in xv6 userland.
 * 
 * Provided functions:
 *  - xcalloc: zero-initialized allocation with overflow protection
 *  - xbsearch: recursive binary search
 *  - xqsort: quicksort using Hoare partition scheme
 *  - xatoi, xatof: ASCII to integer/float conversions
 *
 * @date 9 Nov 2025
 * @author
 * Hadiya Muneeb
 */

#include "kernel/types.h"
#include "user/user.h"
#include "user/xstdlib.h"

/**
 * @brief Allocate and zero-initialize an array.
 *
 * @param num  Number of elements.
 * @param size Size of each element in bytes.
 * @return Pointer to zeroed memory, or NULL on failure or overflow.
 */
void* xcalloc(uint num, uint size) {
    if (size != 0 && num > (uint)-1 / size) {
        return 0;
    }
    
    uint total_size = num * size;
    void* ptr = malloc(total_size);
    
    if (ptr) {
        memset(ptr, 0, total_size); 
    }
    
    return ptr;
}

/**
 * @brief Perform binary search on a sorted array (iterative version).
 *
 * @param key     Pointer to search key.
 * @param base    Pointer to base of array.
 * @param nmemb   Number of elements.
 * @param size    Size of each element in bytes.
 * @param compar  Comparison function: returns <0, 0, >0.
 * @return Pointer to found element or NULL if not found.
 */
void* xbsearch(const void* key, const void* base, uint nmemb, uint size, int (*compar)(const void*, const void*)) {
    if (nmemb == 0) return 0;

    const char* low = (const char*)base;
    const char* high = low + (nmemb - 1) * size;

    while (low <= high) {
        int mid_offset = ((high - low) / size) / 2;
        const char* mid_ptr = low + mid_offset * size;

        int cmp = compar(key, mid_ptr);
        if (cmp == 0) {
            return (void*)mid_ptr;
        } else if (cmp < 0) {
            high = mid_ptr - size;
        } else {
            low = mid_ptr + size;
        }
    }

    return 0;
}

/**
 * @brief Swap two elements in memory.
 *
 * @param a    Pointer to first element.
 * @param b    Pointer to second element.
 * @param size Element size in bytes.
 */
static void swap(char* a, char* b, uint size) {
    for (int i = 0; i < size; i++) {
        char tmp = a[i];
        a[i] = b[i];
        b[i] = tmp;
    }
}


/**
 * @brief Selects the median of three elements (low, mid, high) in an array. Used to select pivot for partition.
 *
 * @param base   Pointer to the base of the array.
 * @param low    Index of the first element.
 * @param high   Index of the last element.
 * @param size   Size of each element in bytes.
 * @param compar Comparison function.
 * @return Pointer to the element chosen as the median (pivot).
 */
static char* median_of_three(char* base, int low, int high, uint size, int (*compar)(const void*, const void*)) {
    int mid = low + (high - low) / 2;
    char* a = base + low * size;
    char* b = base + mid * size;
    char* c = base + high * size;

    // a <= b
    if (compar(a, b) > 0) { char* tmp = a; a = b; b = tmp; }
    // a <= c
    if (compar(a, c) > 0) { char* tmp = a; a = c; c = tmp; }
    // b <= c
    if (compar(b, c) > 0) { char* tmp = b; b = c; c = tmp; }

    return b;
}

/**
 * @brief Partition array using Hoare partition scheme with median-of-three pivot.
 *
 * @param base   Pointer to array base.
 * @param low    Lower index.
 * @param high   Upper index.
 * @param size   Size of each element.
 * @param compar Comparison callback.
 * @return Partition index.
 */
static int partition(char* base, int low, int high, uint size, int (*compar)(const void*, const void*)) {
    char* pivot = median_of_three(base, low, high, size, compar);

    // Move pivot to start
    char* low_elem = base + low * size;
    if (pivot != low_elem) swap(pivot, low_elem, size);
    pivot = low_elem; 

    int i = low - 1;
    int j = high + 1;
    
    while (1) {
        do { i++; } while (i <= high && compar(base + (i * size), pivot) < 0);
        do { j--; } while (j >= low && compar(base + (j * size), pivot) > 0);
        
        if (i >= j) {
            return j;
        }
        swap(base + (i * size), base + (j * size), size);
    }
}

/**
 * @brief Recursive quicksort helper.
 */
static void qsort_helper(char* base, int low, int high, uint size, int (*compar)(const void*, const void*)) {
    if (low < high) {
        int p = partition(base, low, high, size, compar);
        qsort_helper(base, low, p, size, compar);
        qsort_helper(base, p + 1, high, size, compar);
    }
}

/**
 * @brief Sort array using quicksort.
 *
 * @param base   Pointer to array base.
 * @param nmemb  Number of elements.
 * @param size   Size of each element.
 * @param compar Comparison callback.
 */
void xqsort(void* base, uint nmemb, uint size, int (*compar)(const void*, const void*)) {
    if (nmemb <= 1) {
        return;
    }
    qsort_helper((char*)base, 0, nmemb - 1, size, compar);
}

/**
 * @brief Convert string to integer.
 *
 * @param str Null-terminated string.
 * @return Parsed integer value, or 0 on invalid input.
 */
int xatoi(const char* str) {
    if (str == 0) {
        return 0;
    }
    
    int result = 0;
    int sign = 1;
    
    while (*str == ' ' || *str == '\t' || *str == '\n' || *str == '\r') {
        str++;
    }
    
    if (*str == '+') {
        str++;
    } else if (*str == '-') {
        sign = -1;
        str++;
    }
    
    while (*str >= '0' && *str <= '9') {
        result = result * 10 + (*str - '0');
        str++;
    }
    
    return sign * result;
}

/**
 * @brief Compute base^exponent for double values.
 *
 * @param base      Base value.
 * @param exponent  Integer exponent.
 * @return Result of base raised to exponent.
 */
static double xpowf(double base, int exponent) {
    if (exponent == 0) return 1.0f;
    
    double result = 1.0f;
    int abs_exponent = (exponent < 0) ? -exponent : exponent;
    
    for (int i = 0; i < abs_exponent; i++) {
        result *= base;
    }
    
    return (exponent < 0) ? 1.0f / result : result;
}

/**
 * @brief Convert string to floating-point number.
 *
 * @param str Null-terminated string.
 * @return Parsed double value, or 0.0 on invalid input.
 */
double xatof(const char* str) {
    if (str == 0) {
        return 0.0f;
    }
    
    double result = 0.0f;
    double fraction = 1.0f;
    int exponent = 0;
    int sign = 1;
    int exp_sign = 1;
    
    while (*str == ' ' || *str == '\t' || *str == '\n' || *str == '\r') {
        str++;
    }
    
    if (*str == '+') {
        str++;
    } else if (*str == '-') {
        sign = -1;
        str++;
    }
    
    while (*str >= '0' && *str <= '9') {
        result = result * 10.0f + (*str - '0');
        str++;
    }
    
    if (*str == '.') {
        str++;
        while (*str >= '0' && *str <= '9') {
            result = result * 10.0f + (*str - '0');
            fraction *= 10.0f;
            str++;
        }
    }
    
    result = sign * result / fraction;
    
    if (*str == 'e' || *str == 'E') {
        str++;
        
        if (*str == '+') {
            str++;
        } else if (*str == '-') {
            exp_sign = -1;
            str++;
        }
        
        while (*str >= '0' && *str <= '9') {
            exponent = exponent * 10 + (*str - '0');
            str++;
        }

        if (exponent != 0) {
            double exp_value = xpowf(10.0f, exp_sign * exponent);
            result *= exp_value;
        }
    }
    
    return result; 
}
