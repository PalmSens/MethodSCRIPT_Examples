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
 *
 *  This example exists of the following files
 *  MethodSCRIPTExample.c           - Gives a high level overview on how to interact with the Pico
 *  MethodSCRIPTExample.h           - Some settings and definitions used in this example
 *  SerialPort.c/h                  - Example on how to implement the serial communication required for MSComm
 *  METHODSCRIPT_output_processor.c - Example on how to process the MethodSCRIPT output into a human readable format
 */


#include "MethodSCRIPTExample.h"
#include "SerialPort.h"

//
// Function prototypes from output_formatter.c
//
void PrintSubpackage(const MscrSubPackage subpackage);
void DisplayResults(const RetCode code, const MscrPackage package, const int package_nr);
void OpenCSVFile(const char *pFilename, FILE **fp);
void WriteHeaderToCSVFile(FILE *fp, MscrPackage first_package);
void WriteDataToCSVFile(FILE *fp, const MscrPackage package, int package_nr);
void ResultsToCsv(const RetCode code, const MscrPackage package, const int package_nr);
void close_csv_file();


// MethodScript communication interface
MSComm msComm;


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
	const char* METHODSCRIPT_FILEPATHNAME = "/../ScriptFiles/INSERT_FILENAME_HERE.mscr";
	const char* RESULT_FILEPATHNAME       = "../Results/INSERT_FILENAME_HERE.csv";
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

			close_csv_file();

			CloseSerialPort();
		} else {
			printf("ERROR: Could not open serial port [%s].\n", SERIAL_PORT_NAME);
		}
	} else {
		printf("ERROR: Failed to configure MSComm object.\n");
	}
}
