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
 *  Implementation of common functions.
 *
 *  Author(s): Hielke Veringa (hielke@palmsens.com)
 */
 
#include "PSCommon.h"

static const char HexTable[] = "0123456789ABCDEF";

void ToHex(uint8_t in, char * buf)
{
	buf[0] = HexTable[(in>>4) & 0xF];
	buf[1] = HexTable[in & 0xF];
}

uint8_t FromHex(const char* in)
{
	uint8_t ret = 0;
	if (in[0] >= 'A' && in[0] <= 'F')
		ret += (in[0] - 'A' + 10);
	else
		ret += in[0] - '0';

	ret <<= 4;

	if (in[1] >= 'A' && in[1] <= 'F')
		ret += (in[1] - 'A' + 10);
	else
		ret += in[1] - '0';
	
	return ret;
}
