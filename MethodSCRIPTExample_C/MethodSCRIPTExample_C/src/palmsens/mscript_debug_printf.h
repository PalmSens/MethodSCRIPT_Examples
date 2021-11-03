/** 
 * \file
 * MethodSCRIPT debug printf function.
 */
#pragma once

#include <stdio.h>

// Uncomment the following line to enable logging in the MethodSCRIPT
// example library:
//#define DEBUG_PRINTF(...) printf(__VA_ARGS__)


// Make sure the macro is defined to prevent compiler errors.
#ifndef DEBUG_PRINTF
#define DEBUG_PRINTF(...)
#endif
