#ifndef RANDOM_UTILS_H
#define RANDOM_UTILS_H

#include <stddef.h>
#include <stdint.h>

int secure_random_bytes(unsigned char *buffer, size_t size);
int secure_rand_range(int max);

#endif
