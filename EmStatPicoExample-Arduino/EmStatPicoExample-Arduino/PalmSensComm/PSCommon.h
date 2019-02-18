/* ----------------------------------------------------------------------------
 *         PalmSens EmStat SDK
 * ----------------------------------------------------------------------------
 * Copyright (c) 2016, PalmSens BV
 *
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * - Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the disclaimer below.
 *
 * PalmSens's name may not be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * DISCLAIMER: THIS SOFTWARE IS PROVIDED BY PALMSENS "AS IS" AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT ARE
 * DISCLAIMED. IN NO EVENT SHALL PALMSENS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA,
 * OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 * ----------------------------------------------------------------------------
 */
 
/**
 *  Contains commonly used stuff.
 *
 *  Author(s): Hielke Veringa (hielke@palmsens.com)
 * 	Version: 1.23
 */
 
#ifndef PSCOMMON_H
#define PSCOMMON_H

#include <stdint.h>

//////////////////////////////////////////////////////////////////////////////
// Defines
//////////////////////////////////////////////////////////////////////////////

#define SIGN(x) ((x) > 0) - ((x) < 0)

//////////////////////////////////////////////////////////////////////////////
// Types
//////////////////////////////////////////////////////////////////////////////

//Templates for communication functions

//Function in the form of "int function(char c);"
typedef int (*WriteCharFunc)(char c);
//Function in the form of "int function();"
typedef int (*ReadCharFunc)();
//Function in the form of "unsigned long function();"
typedef unsigned long (*TimeMsFunc)();

///
/// Function return codes.
///
typedef enum _RetCode
{
	CODE_MEASURING			= 3,
	CODE_MEASUREMENT_DONE	= 2,
	CODE_RESPONSE_END		= 1,
	CODE_OK 				= 0,
	CODE_NULL 				= -1,
	CODE_TIMEOUT			= -2,
	CODE_OUT_OF_RANGE		= -3,
	CODE_UNEXPECTED_DATA	= -4,
	CODE_NOT_IMPLEMENTED 	= -5,
} RetCode;

//////////////////////////////////////////////////////////////////////////////
// Util Functions
//////////////////////////////////////////////////////////////////////////////

///
/// Converts a uint8_t to a hex string.
/// Make sure to allocate the string beforehand (with at least 2 chars of space).
///
void ToHex(uint8_t in, char * buf);

///
/// Converts the next 2 chars from the "in" string to a uint8_t.
///
uint8_t FromHex(const char* in);

//#define DEFINE_DOUBLE_FUNC  //uncomment this if your compiler does not provice a round function
#ifdef DEFINE_DOUBLE_FUNC
static inline double round(double number)
{
	return (double)((number >= 0) ? (long)(number + 0.5) : (long)(number - 0.5));
}
#endif

#endif //PSCOMMON_H
