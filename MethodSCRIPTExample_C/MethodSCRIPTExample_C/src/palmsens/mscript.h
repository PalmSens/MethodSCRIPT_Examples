/** 
 * \file
 * MethodSCRIPT communication functions.
 * 
 * This header file contains constants and types used in the example. It also
 * contains the function prototypes for MethodSCRIPT-related functions.
 * The implementation of these functions is separated in three files:
 *   - mscript.c: generic high-level communication functions
 *   - mscript_parsing.c: functions for parsing the received data packages
 *   - mscript_formatting.c: functions to format
 * See the documentation of above-mentioned source files for more information.
 *
 * Note: The parsing and formatting modules are independent of the physical
 * interface, i.e., they do not perform any communication with the device but
 * process data that has been received already. The generic functions in 
 * "mscript.c" do communicate with the device, but in a way that is independent
 * of the operating system (since the OS-specific implementations of the serial
 * port are implemented in separate files, using a common interface).
 *
 * ----------------------------------------------------------------------------
 *
 *	\copyright (c) 2021 PalmSens BV
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
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "mscript_serial_port.h"

/**
 * The size of the serial read buffer in bytes. This is also the maximum length
 * of a package.
 */
#define MSCRIPT_READ_BUFFER_SIZE 1000

/* The first character of the response of the EmStat Pico identifies the
 * type of message. The following defines define the most commonly used
 * message types.
 */
#define MSCRIPT_REPLY_ID_FIRMWARE_VERSION 't'  //!< Reply of firmware version command
#define MSCRIPT_REPLY_ID_DATA_PACKAGE     'P'  //!< Start of data package
#define MSCRIPT_REPLY_ID_LOOP_START       'L'  //!< Start of loop command
#define MSCRIPT_REPLY_ID_LOOP_END         '+'  //!< End of loop command
#define MSCRIPT_REPLY_ID_MEAS_LOOP_START  'M'  //!< Start of a measurement loop
#define MSCRIPT_REPLY_ID_MEAS_LOOP_END    '*'  //!< End of a measurement loop
#define MSCRIPT_REPLY_ID_NSCANS_START     'C'  //!< Start of a scan (when nscans > 1)
#define MSCRIPT_REPLY_ID_NSCANS_END       '-'  //!< End of a scan (when nscans > 1)
#define MSCRIPT_REPLY_ID_EXECUTE_SCRIPT   'e'  //!< Start execution of script
#define MSCRIPT_REPLY_ID_END_OF_SCRIPT    '\n' //!< Empty line = end of script execution
#define MSCRIPT_REPLY_ID_TEXT             'T'  //!< Response of "send_string" command
#define MSCRIPT_REPLY_ID_ERROR            '!'  //!< An error occurred during script execution

/// Convert a MethodSCRIPT variable type to an integer.
/// For example: "aa" -> 0, "ab" -> 1, "ba" -> 26 and "zz" -> 675.
#define MSCRIPT_VARTYPE(ch1, ch2) (((ch1) - 'a') * 26 + (ch2 - 'a'))

/// Convert a MethodSCRIPT variable type string to an integer.
/// For example: "aa" -> 0, "ab" -> 1, "ba" -> 26 and "zz" -> 675.
#define MSCRIPT_VARTYPE_STR_TO_INT(str) (MSCRIPT_VARTYPE(str[0], str[1]))

/* The following defines are the possible values for the variable type in a
 * MethodSCRIPT data package.
 */
// undefined / not set
#define MSCRIPT_VARTYPE_UNKNOWN             MSCRIPT_VARTYPE('a', 'a')
// 'a' category: potential
#define MSCRIPT_VARTYPE_POTENTIAL           MSCRIPT_VARTYPE('a', 'b') // WE / SE vs RE
#define MSCRIPT_VARTYPE_POTENTIAL_CE        MSCRIPT_VARTYPE('a', 'c') // CE vs GND
#define MSCRIPT_VARTYPE_POTENTIAL_SE        MSCRIPT_VARTYPE('a', 'd') // SE vs GND
#define MSCRIPT_VARTYPE_POTENTIAL_RE        MSCRIPT_VARTYPE('a', 'e') // RE vs GND
#define MSCRIPT_VARTYPE_POTENTIAL_WE        MSCRIPT_VARTYPE('a', 'f') // WE vs GND
#define MSCRIPT_VARTYPE_POTENTIAL_WE_VS_CE  MSCRIPT_VARTYPE('a', 'g') // WE / SE vs CE
#define MSCRIPT_VARTYPE_POTENTIAL_AIN0      MSCRIPT_VARTYPE('a', 's')
#define MSCRIPT_VARTYPE_POTENTIAL_AIN1      MSCRIPT_VARTYPE('a', 't')
#define MSCRIPT_VARTYPE_POTENTIAL_AIN2      MSCRIPT_VARTYPE('a', 'u')
#define MSCRIPT_VARTYPE_POTENTIAL_AIN3      MSCRIPT_VARTYPE('a', 'v')
#define MSCRIPT_VARTYPE_POTENTIAL_AIN4      MSCRIPT_VARTYPE('a', 'w')
#define MSCRIPT_VARTYPE_POTENTIAL_AIN5      MSCRIPT_VARTYPE('a', 'x')
#define MSCRIPT_VARTYPE_POTENTIAL_AIN6      MSCRIPT_VARTYPE('a', 'y')
#define MSCRIPT_VARTYPE_POTENTIAL_AIN7      MSCRIPT_VARTYPE('a', 'z')
// 'b' category: current
#define MSCRIPT_VARTYPE_CURRENT             MSCRIPT_VARTYPE('b', 'a') // WE current
// 'c' category: impedance
#define MSCRIPT_VARTYPE_PHASE               MSCRIPT_VARTYPE('c', 'a')
#define MSCRIPT_VARTYPE_IMP                 MSCRIPT_VARTYPE('c', 'b')
#define MSCRIPT_VARTYPE_ZREAL               MSCRIPT_VARTYPE('c', 'c')
#define MSCRIPT_VARTYPE_ZIMAG               MSCRIPT_VARTYPE('c', 'd')
#define MSCRIPT_VARTYPE_EIS_TDD_E           MSCRIPT_VARTYPE('c', 'e')
#define MSCRIPT_VARTYPE_EIS_TDD_I           MSCRIPT_VARTYPE('c', 'f')
#define MSCRIPT_VARTYPE_EIS_FS              MSCRIPT_VARTYPE('c', 'g')
#define MSCRIPT_VARTYPE_EIS_E_AC            MSCRIPT_VARTYPE('c', 'h')
#define MSCRIPT_VARTYPE_EIS_E_DC            MSCRIPT_VARTYPE('c', 'i')
#define MSCRIPT_VARTYPE_EIS_I_AC            MSCRIPT_VARTYPE('c', 'j')
#define MSCRIPT_VARTYPE_EIS_I_DC            MSCRIPT_VARTYPE('c', 'k')
// 'd' category: applied types
#define MSCRIPT_VARTYPE_CELL_SET_POTENTIAL  MSCRIPT_VARTYPE('d', 'a')
#define MSCRIPT_VARTYPE_CELL_SET_CURRENT    MSCRIPT_VARTYPE('d', 'b')
#define MSCRIPT_VARTYPE_CELL_SET_FREQUENCY  MSCRIPT_VARTYPE('d', 'c')
#define MSCRIPT_VARTYPE_CELL_SET_AMPLITUDE  MSCRIPT_VARTYPE('d', 'd')
// 'e' category: other types
#define MSCRIPT_VARTYPE_CHANNEL             MSCRIPT_VARTYPE('e', 'a')
#define MSCRIPT_VARTYPE_TIME                MSCRIPT_VARTYPE('e', 'b')
#define MSCRIPT_VARTYPE_PIN_MSK             MSCRIPT_VARTYPE('e', 'c')
#define MSCRIPT_VARTYPE_TEMPERATURE         MSCRIPT_VARTYPE('e', 'd')
// Generic types (reserved but not implemented)
#define MSCRIPT_VARTYPE_CURRENT_GENERIC1    MSCRIPT_VARTYPE('h', 'a')
#define MSCRIPT_VARTYPE_CURRENT_GENERIC2    MSCRIPT_VARTYPE('h', 'b')
#define MSCRIPT_VARTYPE_CURRENT_GENERIC3    MSCRIPT_VARTYPE('h', 'c')
#define MSCRIPT_VARTYPE_CURRENT_GENERIC4    MSCRIPT_VARTYPE('h', 'd')
#define MSCRIPT_VARTYPE_POTENTIAL_GENERIC1  MSCRIPT_VARTYPE('i', 'a')
#define MSCRIPT_VARTYPE_POTENTIAL_GENERIC2  MSCRIPT_VARTYPE('i', 'b')
#define MSCRIPT_VARTYPE_POTENTIAL_GENERIC3  MSCRIPT_VARTYPE('i', 'c')
#define MSCRIPT_VARTYPE_POTENTIAL_GENERIC4  MSCRIPT_VARTYPE('i', 'd')
#define MSCRIPT_VARTYPE_MISC_GENERIC1       MSCRIPT_VARTYPE('j', 'a')
#define MSCRIPT_VARTYPE_MISC_GENERIC2       MSCRIPT_VARTYPE('j', 'b')
#define MSCRIPT_VARTYPE_MISC_GENERIC3       MSCRIPT_VARTYPE('j', 'c')
#define MSCRIPT_VARTYPE_MISC_GENERIC4       MSCRIPT_VARTYPE('j', 'd')

// Measurement status values, e.g. underload or overload
#define MSCRIPT_STATUS_OK               0   //!< OK (no error / warning)
#define MSCRIPT_STATUS_TIMING_ERROR     1   //!< Loop timing not met
#define MSCRIPT_STATUS_OVERLOAD         2   //!< Current overload
#define MSCRIPT_STATUS_UNDERLOAD        4   //!< Current underload
#define MSCRIPT_STATUS_OVERLOAD_WARNING 8   //!< Current overload warning

/// The maximum number of sub packages to be expected in a MethodSCRIPT data
/// package.
// NOTE: Adjust this variable to your needs. Increasing it also increases the
// memory usage, but if set to low, not all data can't be processed.
#define MSCRIPT_MAX_SUB_PACKAGES_PER_LINE 8

/// Device type (instrument type)
typedef enum {
	UNKNOWN_DEVICE,
	EMSTAT_PICO,
	EMSTAT4_LR,
	EMSTAT4_HR,
	MULTI_EMSTAT4_LR,
	MULTI_EMSTAT4_HR,
} DeviceType_t;

/// MethodSCRIPT sub package metadata. This is part of a subpackage.
/// Metadata fields that are not given should be set to -1.
/// One sub package can contain any combination of metadata fields.
typedef struct {
	int status;
	int range;
} MScriptMetadata_t;

/** Structure to store one MethodSCRIPT sub-package. */
typedef struct {
	double value;
	unsigned int variable_type;
	MScriptMetadata_t metadata;
} MscriptSubPackage_t;

/** Structure to store a complete MethodSCRIPT data package. */
typedef struct {
	/** The number of valid sub packages in this package. */
	size_t nr_of_sub_packages;
	MscriptSubPackage_t sub_packages[MSCRIPT_MAX_SUB_PACKAGES_PER_LINE];
} MscriptDataPackage_t;

#ifdef __cplusplus
extern "C" {
#endif

// Function prototypes
void mscript_flush_communication(SerialPortHandle_t handle);
bool mscript_serial_port_read_line(SerialPortHandle_t handle, char * buf, size_t buf_size,
	uint32_t timeout_ms);
bool mscript_get_firmware_version(SerialPortHandle_t handle, char * buf, size_t buf_size);
DeviceType_t mscript_get_device_type(char const * firmware_version);
char const * mscript_get_device_type_name(DeviceType_t device_type);
bool mscript_send_file(SerialPortHandle_t handle, char const * path);
bool parse_data_package(char const * response, MscriptDataPackage_t * package);
char const * mscript_vartype_to_string(unsigned int vartype);
char const * mscript_metadata_status_to_string(unsigned int status_flag);
char const * mscript_metadata_range_to_string(DeviceType_t device_type,
	unsigned int variable_type, int current_range);

#ifdef __cplusplus
} // extern "C"
#endif
