/* ----------------------------------------------------------------------------
 *         PalmSens MethodSCRIPT SDK Example
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
 *  Methodscript C example for the EmStat Pico.
 *
 *  This example demonstrates how to use the MethodSCRIPT C SDK for on EmStat Pico on Windows/Linux.
 *  It connects to one EmStat Pico, sends a MethodSCRIPT file and processes the response.
 *  The parsed output from the EmStat is displayed on the console and stored in a CSV file.
 *
 *  This example is shipped with a few demo scripts that can be selected with the value of `SCRIPT_SELECT`
 *  To use this with a custom script, change the value of `SCRIPT_SELECT` to 100 and modify
 *  the appropriate filenames in `METHODSCRIPT_FILEPATHNAME` and `RESULT_FILEPATHNAME`.
 */


#include "MethodSCRIPTExample.h"
#include "SerialPort.h"


// This file is shared between Windows an Linux examples, so we need to add the missing links.
#ifdef __WIN32
	#include <windows.h>
	#define SET_SEPARATOR_FOR_MS_EXCEL 1 // Set to 0 if NOT using Excel
#else
	#define Sleep(time) usleep(time * 1000)
	#define SET_SEPARATOR_FOR_MS_EXCEL 0
#endif


// Maximum number of characters that the EmStat Pico can receive in one line
#define MS_MAX_LINECHARS	128

// MethodScript communication interface
MSComm msComm;
// CSV File pointer
FILE *pFCsv;


// Script select
// 0   = DEMO LSV (with 10k Ohm)
// 1   = DEMO EIS
// 2   = DEMO SWV (with 10k Ohm)
// 100 = custom script
#define SCRIPT_SELECT	100


// Set the script and output filenames according to the value of SCRIPT_SELECT
#if SCRIPT_SELECT == 0
	//
	// LSV demo
	//
	const char* METHODSCRIPT_FILEPATHNAME = "/../ScriptFiles/MSExample_LSV_10k.mscr";
	const char* RESULT_FILEPATHNAME = 		"../Results/MSExample_LSV_10k.csv";
#elif SCRIPT_SELECT == 1
	//
	// EIS demo
	//
	const char* METHODSCRIPT_FILEPATHNAME = "/../ScriptFiles/MSExample_EIS.mscr";
	const char* RESULT_FILEPATHNAME = 		"../Results/MSExample_EIS.csv";
#elif SCRIPT_SELECT == 2
	//
	// SWV demo
	//
	const char* METHODSCRIPT_FILEPATHNAME = "/../ScriptFiles/MSExample_SWV_10k.mscr";
	const char* RESULT_FILEPATHNAME = 		"../Results/MSExample_SWV_10k.csv";
#elif SCRIPT_SELECT == 100
	//
	// Set the paths to a custom script. Replace `INSERT_FILENAME_HERE` with the desired filenames for
	// the script and output files.
	//
	//const char* METHODSCRIPT_FILEPATHNAME = "/../ScriptFiles/INSERT_FILENAME_HERE.mscr";
	//const char* RESULT_FILEPATHNAME       = "../Results/INSERT_FILENAME_HERE.csv";

	const char* METHODSCRIPT_FILEPATHNAME = "C:\\Users\\RobertPaauw\\Downloads\\OCVthenCAbipot.txt";
	const char *RESULT_FILEPATHNAME       = "C:\\Users\\RobertPaauw\\Downloads\\result_OCVthenCAbipot.csv";
#endif


//
// Check if the the connected device is an EmStat Pico.
//
// return:
//    true    The other side of the serial port is an EmStat Pico
//    false   The connected device is not supported by this example.
//
bool VerifyEmStatPico()
{
	char versionString[30];
	RetCode code;
	WriteStr(&msComm, CMD_VERSION_STRING);

	code = ReadBuf(&msComm, versionString);

	// Verifies if the device is EmStat Pico by looking for "espico" in the version response
	if(code == CODE_VERSION_RESPONSE && strstr(versionString, "espico") != NULL)
	{
		while(strstr(versionString, "*") == NULL)
		{
			ReadBuf(&msComm, versionString); // Skip line containing *.
		}
		return true;
	}
	else
		return false;
}


//
// Send a script file line by line to the EmStat
//
// parameters:
//    filename    The filename/path of the file to send to the EmStat
//
// return:
//    The status of the operation - `CODE_OK` if successful
//
int SendScriptFile(const char *fileName)
{
	FILE *fp;
	char str[MS_MAX_LINECHARS + 1];	// Plus the string termination character (0)

	fp = fopen(fileName, "r");
	if (fp == NULL) {
		printf("Could not open file %s", fileName);
		return CODE_ERROR;
	}
	// Reads a single line from the script file and sends it to the device.
	while (fgets(str, MS_MAX_LINECHARS, fp) != NULL)
	{
		WriteStr(&msComm, str);
	}
	fclose(fp);
	return CODE_OK;
}


///
/// Print one MethodSCRIPToutpu  subpackage on the console.
///
void PrintSubpackage(const MscrSubPackage subpackage)
{
	// Format and print the subpackage value

	switch(subpackage.variable_type) {
		case MSCR_VT_POTENTIAL:
		case MSCR_VT_POTENTIAL_CE:
		case MSCR_VT_POTENTIAL_SE:
		case MSCR_VT_POTENTIAL_RE:
		case MSCR_VT_POTENTIAL_GENERIC1:
		case MSCR_VT_POTENTIAL_GENERIC2:
		case MSCR_VT_POTENTIAL_GENERIC3:
		case MSCR_VT_POTENTIAL_GENERIC4:
		case MSCR_VT_POTENTIAL_WE_VS_CE:
			printf("E[V]: %6.3f \t", subpackage.value);
			break;
		case MSCR_VT_CURRENT:
		case MSCR_VT_CURRENT_GENERIC1:
		case MSCR_VT_CURRENT_GENERIC2:
		case MSCR_VT_CURRENT_GENERIC3:
		case MSCR_VT_CURRENT_GENERIC4:
			printf("I[A]: %11.3E \t", subpackage.value);
			break;
		case MSCR_VT_ZREAL:
			printf("Zreal[Ohm]: %16.3f \t", subpackage.value);
			break;
		case MSCR_VT_ZIMAG:
			printf("Zimag[Ohm]: %16.3f \t", subpackage.value);
			break;
		case MSCR_VT_CELL_SET_POTENTIAL:
			printf("E set[V]: %6.3f \t", subpackage.value);
			break;
		case MSCR_VT_CELL_SET_CURRENT:
			printf("I set[A]: %11.3E \t", subpackage.value);
			break;
		case MSCR_VT_CELL_SET_FREQUENCY:
			printf("F set[Hz]: %6.3E \t", subpackage.value);
			break;
		case MSCR_VT_CELL_SET_AMPLITUDE:
			printf("A set[V]: %6.3f \t", subpackage.value);
			break;
		case MSCR_VT_UNKNOWN:
		default:
			printf("?%d?[?] %16.3f ", subpackage.variable_type, subpackage.value);
	}


	// Print subpackage metadata

	// Print status field if available
	if (subpackage.metadata.status >= 0)
	{
		const	char *status_str;
		if (subpackage.metadata.status == 0)
		{
			status_str = StatusToString(0);
		}
		else
		{
			// Find the first status flag that is set.
			for (int i = 0; i < 31; i++)
			{
				if ((subpackage.metadata.status & (1 << i)) != 0)
				{
					status_str = StatusToString(1 << i);
					break;
				}
			}
		}

		printf("status: %-16s \t", status_str);
	}

	// Print current range if available
	if (subpackage.metadata.current_range >= 0)
	{
		const char *current_range_str = current_range_to_string(subpackage.metadata.current_range);

		printf("CR: %-20s \t", current_range_str);
	}
}


//
// Print the processed MethodSCRIPT output on the console.
//
// parameters:
//    code        The status code from the received package
//    package     The processed MethodSCRIPT package
//    package_nr  The package number within the current measurement-loop (starts at 0)
//
void DisplayResults(const RetCode code, const MscrPackage package, const int package_nr)
{
	switch(code)
	{
	case CODE_RESPONSE_BEGIN:				// Measurement response begins
		printf("\nResponse begin\n");
		break;
	case CODE_MEASURING:
		printf("\nMeasuring... \n");
		break;
	case CODE_OK:							// Received valid package, print it.
			if(package_nr == 0)
	 			printf("\nReceiving measurement response:\n");
	 		// Print package index (starting at 1 on the console)
			printf(" %d \t", package_nr + 1);

			// Print all subpackages in
			for (int i = 0; i < package.nr_of_subpackages; i++)
				PrintSubpackage(package.subpackages[i]);

			printf("\n");
		 	fflush(stdout);
	 	break;
	case CODE_MEASUREMENT_DONE:			    // Measurement loop complete
		printf("\nMeasurement completed. ");
		break;
	case CODE_RESPONSE_END:				    // Measurement response end
		printf("%d data point(s) received.", package_nr);
		fflush(stdout);
		break;
	default:                    			// Failed to parse or identify package.
		printf("\nFailed to parse package: %d\n", code);
	}
}


//
// Open a new CSV file for writing. Overwrite if it already exists.
//
// parameters:
//    pFilename    The filename/path for the new CSV file
//    fp           The pointer to store the created file pointer in.
//
void OpenCSVFile(const char *pFilename, FILE **fp)
{
	*fp = fopen(pFilename, "w");		//Open file for writing (overwrite existing)
	if (*fp == NULL)
	{
		printf("Could not open CSV file %s (hint: make sure the directory exists)", pFilename);
	}

	// Add an extra line to tell Microsoft Excel that we use "," as separator
	if (SET_SEPARATOR_FOR_MS_EXCEL == 1)
	{
		fprintf(*fp, "\"sep=,\"\n");
	}
}


//
// Write a CSV header to the file. The fields in the header are determined by the `variable types`
// in the provided package.
//
// parameters:
//    fp               File pointer of the opened CSV file
//    first_package    The first package in the measurement loop, used to determine the header fields
//
//
void WriteHeaderToCSVFile(FILE *fp, MscrPackage first_package)
{
	if (fp == NULL)
	{
		printf("Unable to write header to CSV file\n");
		fflush(stdout);
		return;
	}

	// The first field is always the package index
	fprintf(fp, "\"Index\"");

	// Loop through package to find Variable types
	for (int i = 0; i < first_package.nr_of_subpackages; i++)
	{
		const char *variable_typename_str = VartypeToString(first_package.subpackages[i].variable_type);
		fprintf(fp, ",\"%s\"", variable_typename_str);

		if(first_package.subpackages[i].metadata.status >= 0)
			fprintf(fp, ",\"Status\"");
		if(first_package.subpackages[i].metadata.current_range >= 0)
			fprintf(fp, ",\"Current Range\"");

	}

	// Terminate the line
	fprintf(fp, "\n");
}


//
// Write one data package to the CSV file
//
// parameters:
//    fp          File pointer of the opened CSV file
//    package     The processed MethodSCRIPT package
//    package_nr  The package number within the current measurement-loop (starts at 0)
//
void WriteDataToCSVFile(FILE *fp, const MscrPackage package, int package_nr)
{
	if (fp == NULL)
	{
		printf("Unable to write to CSV file\n");
		fflush(stdout);
		return;
	}

	// Start the CSV line with a package index
	fprintf(fp, "%d", package_nr + 1);

	// Loop through package and add values to the CSV cells
	for (int i = 0; i < package.nr_of_subpackages; i++)
	{
		fprintf(fp, ",\"%f\"", package.subpackages[i].value);

		// Also print metadata if available

		if (package.subpackages[i].metadata.status >= 0)
		{
			const	char *status_str;
			if (package.subpackages[i].metadata.status == 0)
			{
				status_str = StatusToString(0);
			}
			else
			{
				// Find the first status flag that is set.
				for (int bit = 0; bit < 31; bit++)
				{
					if ((package.subpackages[i].metadata.status & (1 << bit)) != 0)
					{
						status_str = StatusToString(1 << bit);
						break;
					}
				}
			}
			fprintf(fp, ",\"%s\"", status_str, package.subpackages[i].metadata.status);
		}

		// Print current range if available
		if (package.subpackages[i].metadata.current_range >= 0)
		{
			const char *current_range_str = current_range_to_string(package.subpackages[i].metadata.current_range);

			fprintf(fp, ",\"%s\"", current_range_str);
		}

	}

	// Terminate the line
	fprintf(fp, "\n");
}


//
// Store the parsed MethodSCRIPT output in a CSV file.
// The first packet in a measurement loop will determine the values in the header field.
// If the format of consecutive packages differs within a measurement loop they will be printed as is
// and may in that case not match the header.
//
// parameters:
//    code        The status code from the received package
//    package     The processed MethodSCRIPT package
//    package_nr  The package number within the current measurement-loop (starts at 0)
//
void ResultsToCsv(const RetCode code, const MscrPackage package, const int package_nr)
{
	switch(code)
	{
	case CODE_RESPONSE_BEGIN:					// Measurement response begins
		OpenCSVFile(RESULT_FILEPATHNAME, &pFCsv);
		break;
	case CODE_MEASURING:
		break;
	case CODE_OK:								// Received valid package, print it.
		if(package_nr == 0)
		{
			WriteHeaderToCSVFile(pFCsv, package);
		}
		WriteDataToCSVFile(pFCsv, package, package_nr);
		break;
	case CODE_MEASUREMENT_DONE:         // Measurement loop complete
		fprintf(pFCsv, "\n");            // Add a empty line to create a new section
		break;
	case CODE_RESPONSE_END:             // Measurement response end
	    break;
	default:                            // Failed to parse or identify package.
		break;
	}
}


///
/// Receive and process MethodSCRIPT output from the EmStat.
/// The results are stored in a CSV file and displayed on the terminal.
/// This function will loop until the end-of-script is received or an error occurred.
///
void process_emstat_response()
{
	MscrPackage data;	// The processed package
	RetCode status_code; // Status of the current operation/measurement
	// The number of packages received inside the current measure-loop. Used in CSV and display.
	int loop_package_nr = 0;

	do
	{
		// Receives one package and stores the parsed values in the struct 'data'
		status_code = ReceivePackage(&msComm, &data);
		if(status_code < 0)
		{
			printf("Error while receiving packages from EmStat (code %d)\n", status_code);
			return;
		}

		// Start of a new loop received, begin counting from the start.
		if(status_code == CODE_MEASURING)
		{
			loop_package_nr = 0;
		}

		ResultsToCsv(status_code, data, loop_package_nr);		// Write result data-point to a CSV file
		DisplayResults(status_code, data, loop_package_nr);	// Displays the data-point on the console

		if (status_code == CODE_OK)
			loop_package_nr++;
	} while((status_code != CODE_RESPONSE_END) && (status_code >= 0));
}


//
// The main loop of the application.
// In this example the main loop is responsible for setting up the serial communication using `MSCommInit`and
// sending the MethodSCRIPT file to the device. At that point it will call the `process_emstat_response`
// function which processes the response from the EmStat, displays and stores the results.
// After the MethodSCRIPT execution completes on the device, the main function will clean up and exit.
//
int main(int argc, char *argv[])
{
	RetCode status_code = MSCommInit(&msComm, &WriteToDevice, &ReadFromDevice);

	if (status_code == CODE_OK)
	{
		int isOpen = OpenSerialPort(SERIAL_PORT_NAME, BAUD_RATE);

		if(isOpen)
		{
			printf("Connecting to EmStat Pico...\n");
			// Flush any previous communication (required for Bluetooth on older dev-boards)
			for (int i = 0 ; i < 3; i++)
			{
				WriteStr(&msComm, "\n");
				Sleep(100); // Wait 100ms
				while(ReadFromDevice() > 0);
			}

			// To make sure we have the right serial port open we will check if the device on the other end is
			// actually an EmStat.
			int fSuccess = VerifyEmStatPico();
			if(fSuccess)
			{
				printf("Serial port successfully connected to EmStat Pico.\n");
			} else {
				printf("Connected device is not EmStat Pico.\n");
				return -1;
			}
			// Everything is set up at this point, so time to send the script and process the response.
			if(SendScriptFile(METHODSCRIPT_FILEPATHNAME) == CODE_OK)
			{
				printf("\nMethodSCRIPT sent to EmStat Pico.\n");
				process_emstat_response();
			}

			// Close CSV file
			fclose(pFCsv);

			CloseSerialPort();
		} else {
			printf("ERROR: Could not open serial port [%s].\n", SERIAL_PORT_NAME);
		}
	} else {
		printf("ERROR: Failed to configure MSComm object.\n");
	}
}
