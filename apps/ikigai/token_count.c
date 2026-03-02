/**
 * @file token_count.c
 * @brief Token count estimation from byte count
 */

#include "token_count.h"

#include <stddef.h>
#include <stdint.h>


#include "shared/poison.h"
int32_t ik_token_count_from_bytes(size_t byte_count)
{
    return (int32_t)(byte_count / 4);
}
