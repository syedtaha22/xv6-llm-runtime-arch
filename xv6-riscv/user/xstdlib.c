/**
 * @file xstdlib.c
 * @brief Minimal standard library extensions for xv6 user programs.
 * 
 * @author Hadiya Muneeb
 * @date 9th November 2025
 *
 * @details
 * Implements safe memory allocation, basic numeric conversions, and
 * general-purpose sorting and searching routines usable in xv6 userland.
 * Function-level documentation is provided in the header `xstdlib.h`.
 */

#include "kernel/types.h"
#include "user/user.h"
#include "user/xstdlib.h"

void* xcalloc(uint num, uint size) {
    if (size != 0 && num > (uint)-1 / size) return 0;
    uint total_size = num * size;
    void* ptr = malloc(total_size);
    if (ptr) memset(ptr, 0, total_size);
    return ptr;
}

void* xbsearch(const void* key, const void* base, uint nmemb, uint size, int (*compar)(const void*, const void*)) {
    if (nmemb == 0) return 0;
    const char* low = (const char*)base;
    const char* high = low + (nmemb - 1) * size;

    while (low <= high) {
        int mid_offset = ((high - low) / size) / 2;
        const char* mid_ptr = low + mid_offset * size;
        int cmp = compar(key, mid_ptr);
        if (cmp == 0) return (void*)mid_ptr;
        else if (cmp < 0) high = mid_ptr - size;
        else low = mid_ptr + size;
    }
    return 0;
}

static void swap(char* a, char* b, uint size) {
    for (int i = 0; i < size; i++) {
        char tmp = a[i]; a[i] = b[i]; b[i] = tmp;
    }
}

static char* median_of_three(char* base, int low, int high, uint size, int (*compar)(const void*, const void*)) {
    int mid = low + (high - low) / 2;
    char* a = base + low * size;
    char* b = base + mid * size;
    char* c = base + high * size;

    if (compar(a, b) > 0) { char* tmp = a; a = b; b = tmp; }
    if (compar(a, c) > 0) { char* tmp = a; a = c; c = tmp; }
    if (compar(b, c) > 0) { char* tmp = b; b = c; c = tmp; }

    return b;
}

static int partition(char* base, int low, int high, uint size, int (*compar)(const void*, const void*)) {
    char* pivot = median_of_three(base, low, high, size, compar);
    char* low_elem = base + low * size;
    if (pivot != low_elem) swap(pivot, low_elem, size);
    pivot = low_elem;

    int i = low - 1, j = high + 1;
    while (1) {
        do { i++; } while (i <= high && compar(base + (i * size), pivot) < 0);
        do { j--; } while (j >= low && compar(base + (j * size), pivot) > 0);
        if (i >= j) return j;
        swap(base + (i * size), base + (j * size), size);
    }
}

static void qsort_helper(char* base, int low, int high, uint size, int (*compar)(const void*, const void*)) {
    if (low < high) {
        int p = partition(base, low, high, size, compar);
        qsort_helper(base, low, p, size, compar);
        qsort_helper(base, p + 1, high, size, compar);
    }
}

void xqsort(void* base, uint nmemb, uint size, int (*compar)(const void*, const void*)) {
    if (nmemb <= 1) return;
    qsort_helper((char*)base, 0, nmemb - 1, size, compar);
}

int xatoi(const char* str) {
    if (!str) return 0;
    int result = 0, sign = 1;
    while (*str == ' ' || *str == '\t' || *str == '\n' || *str == '\r') str++;
    if (*str == '+') str++;
    else if (*str == '-') { sign = -1; str++; }
    while (*str >= '0' && *str <= '9') { result = result * 10 + (*str - '0'); str++; }
    return sign * result;
}

static double xpowf(double base, int exponent) {
    if (exponent == 0) return 1.0f;
    double result = 1.0f;
    int abs_exponent = (exponent < 0) ? -exponent : exponent;
    for (int i = 0; i < abs_exponent; i++) result *= base;
    return (exponent < 0) ? 1.0f / result : result;
}

double xatof(const char* str) {
    if (!str) return 0.0f;
    double result = 0.0f, fraction = 1.0f;
    int exponent = 0, sign = 1, exp_sign = 1;

    while (*str == ' ' || *str == '\t' || *str == '\n' || *str == '\r') str++;
    if (*str == '+') str++;
    else if (*str == '-') { sign = -1; str++; }

    while (*str >= '0' && *str <= '9') { result = result * 10.0f + (*str - '0'); str++; }
    if (*str == '.') {
        str++;
        while (*str >= '0' && *str <= '9') { result = result * 10.0f + (*str - '0'); fraction *= 10.0f; str++; }
    }
    result = sign * result / fraction;

    if (*str == 'e' || *str == 'E') {
        str++;
        if (*str == '+') str++;
        else if (*str == '-') { exp_sign = -1; str++; }
        while (*str >= '0' && *str <= '9') { exponent = exponent * 10 + (*str - '0'); str++; }
        if (exponent != 0) result *= xpowf(10.0f, exp_sign * exponent);
    }

    return result;
}
