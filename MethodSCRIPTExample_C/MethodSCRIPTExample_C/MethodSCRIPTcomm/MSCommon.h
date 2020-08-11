/* ----------------------------------------------------------------------------
 *         PalmSens MethodSCRIPT SDK
 * ----------------------------------------------------------------------------
 * Copyright (c) 2019-2020, PalmSens BV
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
 */

#ifndef PSCOMMON_H
#define PSCOMMON_H

#include <stdint.h>

//////////////////////////////////////////////////////////////////////////////
// Types
//////////////////////////////////////////////////////////////////////////////

//Templates for communication functions

//Function in the form of "int function(char c);"
typedef int (*WriteCharFunc)(char c);
//Function in the form of "int function();"
typedef int (*ReadCharFunc)();

///
/// Function return codes.
///
typedef enum _RetCode
{
   CODE_VERSION_RESPONSE = 5,
   CODE_RESPONSE_BEGIN   = 4,
   CODE_MEASURING        = 3,
   CODE_MEASUREMENT_DONE = 2,
   CODE_RESPONSE_END     = 1,
   CODE_OK               = 0,
   CODE_NULL             = -1,
   CODE_TIMEOUT          = -2,
   CODE_OUT_OF_RANGE     = -3,
   CODE_UNEXPECTED_DATA  = -4,
   CODE_NOT_IMPLEMENTED  = -5,
   CODE_ERROR            = -1024,
} RetCode;

#endif //PSCOMMON_H
