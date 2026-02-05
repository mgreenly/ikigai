/**
 * @file poison.h
 * @brief Compile-time enforcement of banned unsafe C functions
 *
 * This header uses #pragma GCC poison to prevent use of unsafe C functions.
 * Include this as the LAST #include in .c files to ensure it doesn't affect
 * system headers or vendor code.
 *
 * DO NOT include this in .h files - it should only apply to our implementation code.
 */

#ifndef IK_POISON_H
#define IK_POISON_H

// Ban unsafe string functions
#pragma GCC poison sprintf strcpy strcat gets strncpy strtok

// Ban unsafe conversion functions
#pragma GCC poison atoi atol atof

#endif // IK_POISON_H
