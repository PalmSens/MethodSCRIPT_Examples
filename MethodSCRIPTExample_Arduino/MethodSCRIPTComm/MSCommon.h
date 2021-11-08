/**
 * PalmSens MethodSCRIPT SDK example for Arduino
 *
 * Commonly used stuff.
 *
 * ----------------------------------------------------------------------------
 *
 *	\copyright (c) 2019-2021 PalmSens BV
 *	All rights reserved.
 *
 *	Redistribution and use in source and binary forms, with or without
 *	modification, are permitted provided that the following conditions are met:
 *
 *		- Redistributions of source code must retain the above copyright notice,
 *		  this list of conditions and the following disclaimer.
 *		- Neither the name of PalmSens BV nor the names of its contributors
 *		  may be used to endorse or promote products derived from this software
 *		  without specific prior written permission.
 *		- This license does not release you from any requirement to obtain separate 
 *		  licenses from 3rd party patent holders to use this software.
 *		- Use of the software either in source or binary form must be connected to, 
 *		  run on or loaded to an PalmSens BV component.
 *
 *	DISCLAIMER: THIS SOFTWARE IS PROVIDED BY PALMSENS "AS IS" AND ANY EXPRESS OR
 *	IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 *	MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO
 *	EVENT SHALL THE REGENTS AND CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 *	INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *	LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA,
 *	OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 *	LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 *	NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 *	EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * ----------------------------------------------------------------------------
 */

#ifndef MSCOMMON_H
#define MSCOMMON_H

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
/// Note values < 0 indicate an error
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


#endif //MSCOMMON_H
