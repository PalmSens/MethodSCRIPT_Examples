/**
 * \file
 * PalmSens MethodSCRIPT example in C.
 * 
 * This example demonstrates how to connect to a MethodSCRIPT device (such as
 * the EmStat4 or EmStat Pico) from a Windows or Linux computer.
 * 
 * The following topics are demonstrated in this example:
 *   - Connecting to the serial port (including configuration of the port).
 *   - Reading the device firmware version.
 *   - Sending a MethodSCRIPT from file to the device.
 *   - Receiving and parsing the results from the MethodSCRIPT.
 * 
 * The measurement data is displayed on the standard output (the console) and
 * stored in CSV files (*1). The MethodSCRIPT can be supplied by the user. It is
 * assumed that the script contains one or more measurement loops, and that the
 * data packages in each loop contain the same variables. Of course, the code
 * could be adjusted for other scenarios.
 * 
 * (*1) Although the extension is CSV, which stands for Comma Separated Values,
 *      a semicolon (';') is used as delimiter. This is done to be compatible
 *      with MS Excel, which may use a comma (',') as decimal separator in some
 *      regions, depending on regional settings on the computer. If you use
 *      anoter program to read the CSV files, you may need to change this.
 * 
 * The following example scripts are shipped with this demo:
 *   - example_EIS
 *   - example_LSV_10k
 *   - example_SWV_10k
 * 
 * The code is organized in the following "modules":
 *   - mscript_serial_port:
 *         Low-level serial port functions. This module is platform-dependent
 *         but has a common interface. Implementations are provided for Linux
 *         and Windows.
 *   - mscript:
 *         Higher-level (platform independent) communication functions and
 *         generic functions to communicate with MethodSCRIPT devices and to
 *         parse responses.
 *   - example:
 *         A custom application to demonstrate the use of the above-mentioned
 *         modules.
 * 
 * ----------------------------------------------------------------------------
 *
 * \copyright (c) 2019-2021, PalmSens BV
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *  - Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *
 *  - The name of PalmSens may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
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
 *
 * ----------------------------------------------------------------------------
 */

#include <errno.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "palmsens/mscript.h"
#include "palmsens/mscript_serial_port.h"

/// Timeout (in ms) for reading responses.
// NOTE: If you're doing long measurements, you might need to increase this value.
#define READ_TIMEOUT 5000

// Set the following macro to 1 to add a Microsoft Excel specific header line to
// the CSV file. This might improve importing the CSV file using Excel,
// depending on your regional settings.
// Set to 0 if NOT using Excel to read the CSV file
#define SET_SEPARATOR_FOR_MS_EXCEL 1

/** Maximum length of the script name. */
#define MAX_SCRIPT_NAME_LENGTH 40

#define FIRMWARE_STRING_LENGTH 80

/**
 * Maximum buffer size necessary to hold path to script file.
 * (The path will be "scripts/NAME.mscr")
 */
#define MAX_SCRIPT_FILE_PATH_SIZE (8 + MAX_SCRIPT_NAME_LENGTH + 5 + 1)

/**
 * Maximum buffer size necessary to hold path to script file.
 * (The path will be "results/NAME-0000-M0000.csv")
 */
#define MAX_CSV_FILE_PATH_SIZE (8 + MAX_SCRIPT_NAME_LENGTH + 15 + 1)

static char const help_text[] = 
	"USAGE: %s PORT SCRIPT_NAME\n" // %s -> argv[0]
	"\n"
	"with:\n"
	"    PORT       : the serial port (e.g. COM1 on Windows or /dev/ttyUSB0 on Linux\n"
	"    SCRIPT_NAME: name of the MethodSCRIPT file to send to the device.\n"
	"                 The script should be located in the 'scripts' directory and\n"
	"                 have a '.mscr' extension.\n"
	"\n"
	;

// Forward declarations.
static bool identify_device(SerialPortHandle_t handle);
static bool execute_script(SerialPortHandle_t handle, char const * script_name);
static bool process_response(SerialPortHandle_t handle, char const * script_name);
static FILE * create_csv_file(char const * script_name, unsigned index, char const * response);
static void print_data_package(MscriptDataPackage_t * package);
static void write_csv_header_row(FILE * fp, MscriptDataPackage_t * package);
static void write_csv_data_row(FILE * fp, unsigned int index, MscriptDataPackage_t * package);

static DeviceType_t device_type = UNKNOWN_DEVICE;

/**
 * Example application.
 * 
 * \return EXIT_SUCCESS or EXIT_FAILURE
 */
int main(int argc, char * argv[])
{
	// Check the number of command-line arguments.
	// Display help text if number of arguments is not 2 or 3.
	if ((argc < 2) || (argc > 3)) {
		printf(help_text, argv[0]);
		return EXIT_FAILURE;
	}

	// Set port and script name to supplied arguments.
	char const * port = argv[1];
	char const * script_name = (argc >= 3) ? argv[2] : NULL;

	// Open the serial port on the requested port.
	SerialPortHandle_t h_device = mscript_serial_port_open(port);
	if (h_device == BAD_HANDLE) {
		return EXIT_FAILURE;
	}

	// Flush communication. This is required for some Bluetooth devices.
	mscript_flush_communication(h_device);

	// Identify the device: print the firmware version. The type of device
	// can also be derived from the version information. Note that this also
	// tests if communication is working correctly and the device is not busy.
	bool success = identify_device(h_device);

	if (success) {
		if (script_name == NULL) {
			printf("No script name supplied. Quitting.\n");
		} else {
			// Execute the script
			success = execute_script(h_device, script_name);
		}
	}

	// Close the serial port and exit program.
	mscript_serial_port_close(h_device);
	return success ? EXIT_SUCCESS : EXIT_FAILURE;
}

/**
 * Request and print the firmware version from the device.
 * 
 * This also prints the device type, which can be derived from the serial
 * number. Note that this function is also useful to test the communication
 * with the device. If the serial number can be read successfully, this
 * means the communication is working and the device is ready to respond.
 *
 * \param handle Handle to the serial port.
 * 
 * \return `true` on success, `false` on failure
 */
static bool identify_device(SerialPortHandle_t handle)
{
	// Request the firmware version.
	char firmware_version[FIRMWARE_STRING_LENGTH];
	bool success = mscript_get_firmware_version(handle, firmware_version,
		FIRMWARE_STRING_LENGTH);
	if (!success) {
		return false;
	}

	// Derive device type from version string.
	device_type = mscript_get_device_type(firmware_version);
	char const * device_type_name = mscript_get_device_type_name(device_type);

	// Print results.
	printf("Connected to %s with firmware version: %s\n", device_type_name,
		firmware_version);
	return true;
}

/**
 * Send and execute a MethodSCRIPT and parse the results.
 * 
 * NOTE: The first line of the MethodSCRIPT file must be the "e" command and
 * the script must end with an empty line (meaning the file ends with "\n\n"
 * or "\r\n\r\n").
 * 
 * After the script has been sent to the device, `process_response()` is called,
 * which receives and processes the response of the device.
 * 
 * \return `true` on success, `false` on failure
 */
static bool execute_script(SerialPortHandle_t handle, char const * script_name)
{
	if (strlen(script_name) > MAX_SCRIPT_NAME_LENGTH) {
		printf("ERROR: script name should be at most %d characters long.\n", 
			MAX_SCRIPT_NAME_LENGTH);
		return false;
	}

	char script_file_path[MAX_SCRIPT_FILE_PATH_SIZE];
	strcpy(script_file_path, "scripts/");
	strcat(script_file_path, script_name);
	strcat(script_file_path, ".mscr");

	bool success = mscript_send_file(handle, script_file_path);
	if (!success) {
		return false;
	}

	success = process_response(handle, script_name);
	return success;
}

/**
 * Read and process the output of the device until end of script.
 * 
 * This function reads responses from the device, line by line, until the end
 * of script. Data packages sent from within a measurement loop are stored in
 * a CSV file. For each measurement, a new CSV file is created. The type of
 * measurement (and, optionally, the number of the scan) is stored as part of
 * the file name.
 * 
 * \return `true` on success, `false` on failure
 */
static bool process_response(SerialPortHandle_t handle, char const * script_name)
{
	MscriptDataPackage_t package;
	FILE *fp = NULL;
	unsigned int meas_index = 0;
	unsigned int data_index = 0;

	printf("Receiving results...\n");
	for (;;) {
		char response[MSCRIPT_READ_BUFFER_SIZE];
		// Read one complete line from the device.
		bool success = mscript_serial_port_read_line(handle, response, MSCRIPT_READ_BUFFER_SIZE,
			READ_TIMEOUT);
		if (!success) {
			printf("Communication error or timeout.\n");
			return false;
		}
		// Uncomment the following line to see all MethodSCRIPT response lines
		// printf("RX: %s", response);

		// Check the first character to determine the type of response.
		switch (response[0]) {

		case MSCRIPT_REPLY_ID_MEAS_LOOP_START:
			// This denotes the start of a measurement loop.
			if (strlen(response) != 6) { // "Mxxxx\n"
				printf("ERROR: invalid response: %s", response);
				return false;
			}
			printf("Started measurement loop.\n");
			fp = create_csv_file(script_name, ++meas_index, response);
			if (fp == NULL) {
				printf("ERROR: Could not create output file: %s\n", strerror(errno));
				return false;
			}
			data_index = 0;
			break;

		case MSCRIPT_REPLY_ID_MEAS_LOOP_END:
			// This denotes the end of a measurement loop.
			printf("Finished measurement loop.\n");
			if (fp != NULL) {
				if (fclose(fp)) {
					printf("ERROR: Failed to close CSV file: %s\n", strerror(errno));
				}
				fp = NULL;
			}
			break;

		case MSCRIPT_REPLY_ID_DATA_PACKAGE:
			// This denotes a data package.
			// Parse the data package, i.e. extract the variables from the package.
			success = parse_data_package(response, &package);
			if (!success) {
				printf("ERROR: failed to parse data package.\n");
				return false;
			}
			print_data_package(&package);
			if (fp != NULL) {
				if (data_index == 0) {
					write_csv_header_row(fp, &package);
				}
				write_csv_data_row(fp, ++data_index, &package);
			}
			break;

		case MSCRIPT_REPLY_ID_END_OF_SCRIPT:
			printf("Script finished successfully.\n");
			// This denotes the end of the script.
			return true;

		case MSCRIPT_REPLY_ID_EXECUTE_SCRIPT:
			// This denotes the start of the script.
			break;

		case MSCRIPT_REPLY_ID_TEXT:
			// This denotes the response of a "send_string" command.
			printf("Text message: %s", response + 1);
			break;

		case MSCRIPT_REPLY_ID_ERROR:
			// An error occurred during execution of the MethodSCRIPT.
			// The error message contains the error code and line number.
			printf("ERROR during MethodSCRIPT execution: %s", response);
			return false;

		case MSCRIPT_REPLY_ID_NSCANS_START:
			break;
		case MSCRIPT_REPLY_ID_NSCANS_END:
			// These replies are only applicable when the optional argument
			// "nscans" is used (for Cyclic Voltammetry measurements, i.e. the
			// "meas_loop_cv" command, only).
			// They can be used to process each scan separately, or just be
			// ignored to get one long measurement including all scans.
			// In this example, we print an empty line after each scan so the
			// separate scans can be easily distinguished in the output file.
			if (fp != NULL) {
				fprintf(fp, "\n");
			}
			break;

		case MSCRIPT_REPLY_ID_LOOP_START:
		case MSCRIPT_REPLY_ID_LOOP_END:
			// These replies denote the start and end of a "loop" command.
			break;

		default:
			// Ignore other responses
			printf("Ignored unexpected response line: %s", response);
			break;
		}
	}

	// Make sure file is closed.
	if (fp != NULL) {
		if (fclose(fp)) {
			printf("ERROR: Failed to close CSV file: %s\n", strerror(errno));
		}
		fp = NULL;
	}
	return false;
}

/**
 * Create and open a CSV file with file name based on supplied parameters.
 * 
 * \return A `FILE` pointer to the open file on success, or NULL on failure.
 */
static FILE * create_csv_file(char const * script_name, unsigned index,
	char const * response)
{
	char csv_file_path[MAX_CSV_FILE_PATH_SIZE];
	char M[6] = {0};
	strncpy(M, response, 5);
	snprintf(csv_file_path, MAX_CSV_FILE_PATH_SIZE, "results/%s-%04u-%s.csv",
		script_name, index, M);
	printf("CSV file: %s\n", csv_file_path);
	return fopen(csv_file_path, "wb");
}

static void print_sub_package(MscriptSubPackage_t const * sub_package)
{
	// Print the variable type (shortened/abbreviated) and value.
	switch (sub_package->variable_type) {
	case MSCRIPT_VARTYPE_POTENTIAL:
	case MSCRIPT_VARTYPE_POTENTIAL_CE:
	case MSCRIPT_VARTYPE_POTENTIAL_SE:
	case MSCRIPT_VARTYPE_POTENTIAL_RE:
	case MSCRIPT_VARTYPE_POTENTIAL_WE:
	case MSCRIPT_VARTYPE_POTENTIAL_WE_VS_CE:
	case MSCRIPT_VARTYPE_POTENTIAL_AIN0:
	case MSCRIPT_VARTYPE_POTENTIAL_AIN1:
	case MSCRIPT_VARTYPE_POTENTIAL_AIN2:
	case MSCRIPT_VARTYPE_POTENTIAL_AIN3:
	case MSCRIPT_VARTYPE_POTENTIAL_AIN4:
	case MSCRIPT_VARTYPE_POTENTIAL_AIN5:
	case MSCRIPT_VARTYPE_POTENTIAL_AIN6:
	case MSCRIPT_VARTYPE_POTENTIAL_AIN7:
	case MSCRIPT_VARTYPE_POTENTIAL_GENERIC1:
	case MSCRIPT_VARTYPE_POTENTIAL_GENERIC2:
	case MSCRIPT_VARTYPE_POTENTIAL_GENERIC3:
	case MSCRIPT_VARTYPE_POTENTIAL_GENERIC4:
		printf("   E[V]: %6.3f", sub_package->value);
		break;
	case MSCRIPT_VARTYPE_CURRENT:
	case MSCRIPT_VARTYPE_CURRENT_GENERIC1:
	case MSCRIPT_VARTYPE_CURRENT_GENERIC2:
	case MSCRIPT_VARTYPE_CURRENT_GENERIC3:
	case MSCRIPT_VARTYPE_CURRENT_GENERIC4:
		printf("   I[A]: %11.3E", sub_package->value);
		break;
	case MSCRIPT_VARTYPE_PHASE:
		printf("   phase[degrees]: %f", sub_package->value);
		break;
	case MSCRIPT_VARTYPE_IMP:
		printf("   Z[ohm]: %16.3f", sub_package->value);
		break;
	case MSCRIPT_VARTYPE_ZREAL:
		printf("   Z_real[ohm]: %16.3f", sub_package->value);
		break;
	case MSCRIPT_VARTYPE_ZIMAG:
		printf("   Z_imag[ohm]: %16.3f", sub_package->value);
		break;
	case MSCRIPT_VARTYPE_CELL_SET_POTENTIAL:
		printf("   E_set[V]: %6.3f", sub_package->value);
		break;
	case MSCRIPT_VARTYPE_CELL_SET_CURRENT:
		printf("   I_set[A]: %11.3E", sub_package->value);
		break;
	case MSCRIPT_VARTYPE_CELL_SET_FREQUENCY:
		printf("   F_set[Hz]: %6.3E", sub_package->value);
		break;
	case MSCRIPT_VARTYPE_CELL_SET_AMPLITUDE:
		printf("   A_set[V]: %6.3f", sub_package->value);
		break;
	case MSCRIPT_VARTYPE_TIME:
		printf("  time[s]: %6.3f", sub_package->value);
		break;
	case MSCRIPT_VARTYPE_MISC_GENERIC1:
	case MSCRIPT_VARTYPE_MISC_GENERIC2:
	case MSCRIPT_VARTYPE_MISC_GENERIC3:
	case MSCRIPT_VARTYPE_MISC_GENERIC4:
		printf("   misc: %6.3f", sub_package->value);
		break;
	default:
		printf("   ?%d?[?] %16.3f ", sub_package->variable_type, sub_package->value);
	}

	// Print the metadata. Note that a value < 0 indicates that the variable
	// was not present in the MethodSCRIPT output, so it should be ignored.

	// Print the status metadata.
	if (sub_package->metadata.status >= 0)
	{
		// The status field is a bit mask, containing at most 4 bit fields.
		// If the value is 0 (no bits set), print the status for value 0 (OK).
		// If the value is non-zero, each bit should be tested separately.
		// In this example only the first status flag is printed to keep the
		// output readable.
		char const * status_str;
		if (sub_package->metadata.status == 0) {
			status_str = mscript_metadata_status_to_string(0);
		} else {
			for (unsigned int i = 0; i < 4; ++i) {
				if ((sub_package->metadata.status & (1 << i)) != 0) {
					status_str = mscript_metadata_status_to_string(1 << i);
					break;
				}
			}
		}
		printf("  status: %-16s", status_str);
	}

	// Print the range metadata.
	if (sub_package->metadata.range >= 0) {
		printf("  range: %-19s", mscript_metadata_range_to_string(device_type,
			sub_package->variable_type, sub_package->metadata.range));
	}
}

/**
 * Print the contents of a data package to the console.
 */
static void print_data_package(MscriptDataPackage_t * package)
{
	if (package->nr_of_sub_packages == 0) {
		printf("Empty data package.");
	}
	for (size_t i = 0; i < package->nr_of_sub_packages; ++i) {
		print_sub_package(&package->sub_packages[i]);
	}
	printf("\n");
}

/**
 * Write the CSV header row to file.
 */
static void write_csv_header_row(FILE * fp, MscriptDataPackage_t * package)
{
	if (SET_SEPARATOR_FOR_MS_EXCEL) {
		fprintf(fp, "sep=;\n");
	}

	// The first column is always the package index.
	fprintf(fp, "Index");

	// Add one column for each variable in the package, plus a column for each
	// metadata variable if present.
	// NOTE: It is assumed that all data packages in a measurement loop contain the
	// same variables!
	for (size_t i = 0; i < package->nr_of_sub_packages; ++i) {
		fprintf(fp, ";%s", mscript_vartype_to_string(package->sub_packages[i].variable_type));
		if (package->sub_packages[i].metadata.status >= 0) {
			fprintf(fp, ";Status");
		}
		if (package->sub_packages[i].metadata.range >= 0) {
			fprintf(fp, ";Current Range");
		}
	}
	fprintf(fp, "\r\n");
}

/**
 * Print the status metadata to file.
 * 
 * This function is called by `write_csv_data_row()` and writes one field of
 * the CSV file. All status flags are written. If multiple status flags are
 * set, they are concatenated with " + ".
 */
static void print_metadata_status(FILE * fp, int status) {
	// The metadata status consists of 4 flags that could be set.
	unsigned int num_flags = 0;
	for (unsigned int i = 0; i < 4; ++i) {
		int mask = 1 << i;
		if (status & mask) {
			if (num_flags++ == 0) {
				fprintf(fp, ";%s", mscript_metadata_status_to_string(mask));
			} else {
				fprintf(fp, " + %s", mscript_metadata_status_to_string(mask));
			}
		}
	}
	if (num_flags == 0) {
		fprintf(fp, ";%s", mscript_metadata_status_to_string(0));
	}
}

/**
 * Write a CSV data row to file.
 */
static void write_csv_data_row(FILE * fp, unsigned int index, MscriptDataPackage_t * package)
{
	// The first column is always the package index.
	fprintf(fp, "%u", index);

	// Add all sub packages, i.e. the value and metadata of each variable.
	for (size_t i = 0; i < package->nr_of_sub_packages; ++i) {
		fprintf(fp, ";%.15lf", package->sub_packages[i].value);
		if (package->sub_packages[i].metadata.status >= 0) {
			print_metadata_status(fp, package->sub_packages[i].metadata.status);
		}
		if (package->sub_packages[i].metadata.range >= 0) {
			fprintf(fp, ";%s", mscript_metadata_range_to_string(device_type,
				package->sub_packages[i].variable_type,
				package->sub_packages[i].metadata.range));
		}
	}
	fprintf(fp, "\r\n");
}
