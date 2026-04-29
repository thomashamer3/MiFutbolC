#include "random_utils.h"
#include <stdio.h>
#include <stdlib.h>

#ifdef _WIN32
#include <Windows.h>
#include <bcrypt.h>
#else
#include <unistd.h>
#endif

#ifdef _WIN32
int secure_random_bytes(unsigned char *buffer, size_t size)
{
    BCRYPT_ALG_HANDLE hAlgorithm = NULL;
    NTSTATUS status = BCryptOpenAlgorithmProvider(&hAlgorithm, BCRYPT_RNG_ALGORITHM, NULL, 0);
    if (!BCRYPT_SUCCESS(status))
    {
        return -1;
    }
    status = BCryptGenRandom(hAlgorithm, buffer, (ULONG)size, 0);
    BCryptCloseAlgorithmProvider(hAlgorithm, 0);
    return BCRYPT_SUCCESS(status) ? 0 : -1;
}
#else
int secure_random_bytes(unsigned char *buffer, size_t size)
{
    FILE *f = fopen("/dev/urandom", "rb");
    if (!f)
    {
        return -1;
    }
    size_t read_total = 0;
    while (read_total < size)
    {
        size_t r = fread(buffer + read_total, 1, size - read_total, f);
        if (r == 0)
        {
            fclose(f);
            return -1;
        }
        read_total += r;
    }
    fclose(f);
    return 0;
}
#endif

int secure_rand_range(int max)
{
    if (max <= 0)
    {
        return 0;
    }

    unsigned char rand_bytes[4];
    if (secure_random_bytes(rand_bytes, sizeof(rand_bytes)) == 0)
    {
        unsigned int r = (rand_bytes[0] << 24) | (rand_bytes[1] << 16) |
                         (rand_bytes[2] << 8) | rand_bytes[3];
        return r % max;
    }
    return rand() % max;
}
