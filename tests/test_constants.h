#ifndef IK_TEST_CONSTANTS_H
#define IK_TEST_CONSTANTS_H

// Default timeout for test cases (in seconds)
#ifdef VALGRIND_BUILD
#define IK_TEST_TIMEOUT 300  // 5 minutes for Valgrind/Helgrind (slow)
#else
#define IK_TEST_TIMEOUT 90   // 90 seconds for normal builds
#endif

#endif // IK_TEST_CONSTANTS_H
