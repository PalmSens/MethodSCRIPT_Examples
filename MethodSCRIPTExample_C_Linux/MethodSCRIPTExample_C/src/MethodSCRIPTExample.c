/* ----------------------------------------------------------------------------
 *         PalmSens MethodSCRIPT SDK Example
 * ----------------------------------------------------------------------------
 * Copyright (c) 2020, PalmSens BV
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
 *  Implementation of MethodSCRIPT. See header file and the "Getting started-MethodSCRIPT-Example-C.pdf" for more information.
 */
#include "MethodSCRIPTExample.h"
#include "SerialPort.h"

#define MS_MAX_LINECHARS	128

MSComm msComm;				// MethodScript communication interface
FILE *pFCsv;				// CSV File pointer


// Select demo
// 0 = LSV (with 10k Ohm)
// 1 = EIS
// 2 = SWV (with 10k Ohm)
#define DEMO_SELECT	0

#if DEMO_SELECT == 0
	const char* METHODSCRIPT_FILEPATHNAME = "/ScriptFiles/MSExample_LSV_10k.mscr";	// LSV demo
	const char* RESULT_FILEPATHNAME = 		"Results/MSExample_LSV_10k.csv";
#elif DEMO_SELECT == 1
	const char* METHODSCRIPT_FILEPATHNAME = "/ScriptFiles/MSExample_EIS.mscr";		// EIS demo
	const char* RESULT_FILEPATHNAME = 		"./Results/MSExample_EIS.csv";
#elif DEMO_SELECT == 2
	const char* METHODSCRIPT_FILEPATHNAME = "/ScriptFiles/MSExample_SWV_10k.mscr";	// SWV demo
	const char* RESULT_FILEPATHNAME = 		"./Results/MSExample_SWV_10k.csv";
#endif


int main(int argc, char *argv[])
{
	int continueParsing;
	MeasureData data;
	int nDataPoints;
	RetCode status_code = MSCommInit(&msComm, &WriteToDevice, &ReadFromDevice);

	if (status_code == CODE_OK)
	{
		int isOpen = OpenSerialPort();
		int fSuccess = VerifyEmStatPico();					// Verifies if the connected device is EmStat Pico.
		if(fSuccess)
		{
			printf("Serial port successfully connected to EmStat Pico.\n");
		} else {
			printf("Connected device is not EmStat Pico.\n");
			return -1;
		}

		if(isOpen)
		{
			char buff[PATH_MAX];
			char *currentDirectory = getcwd(buff, PATH_MAX);		      // Fetches the current directory
			const char* filePath = METHODSCRIPT_FILEPATHNAME; 	  			// MethodScript Filename incl. path
			int combinedFilePathSize = PATH_MAX + 1 + strlen(filePath);	  	// Determines the max size of the combined file path
			char combinedFilePath[combinedFilePathSize];				  	// An array to hold the combined file path
			if(currentDirectory != NULL)
			{
				char *combinedPath = strcat(currentDirectory, filePath);  // Concatenates the current directory and file path to generate the combined file path
				strcpy(combinedFilePath, combinedPath);
				if(SendScriptFile(combinedFilePath))
				{
					printf("\nMethodSCRIPT sent to EmStat Pico.\n");
					nDataPoints = 0;
					do
					{
						status_code = ReceivePackage(&msComm, &data);			// Receives the response and stores the parsed values in the struct 'data'
						ResultsToCsv(status_code, data, nDataPoints);								// Write result data-point to a CSV file
						continueParsing = DisplayResults(status_code, data, &nDataPoints);			// Displays the results based on the returned code
					}while(continueParsing == 1);
				}
			}
			else {
				printf("Error in retrieving the current directory.");
			}
			CloseSerialPort();
		} else {
			printf("ERROR: Could not open serial port [%s].\n", PORT_NAME);
		}
	} else {
		printf("ERROR: Failed to configure MSComm object.\n");
	}
}


bool VerifyEmStatPico()
{
	char versionString[30];
	RetCode code;
	bool isConnected = false;
	WriteStr(&msComm, CMD_VERSION_STRING);
	do{
		code = ReadBuf(&msComm, versionString);
		if(code == CODE_VERSION_RESPONSE && strstr(versionString, "espico") != NULL) // Verifies if the device is EmStat Pico by looking for "espico" in the version response
			isConnected = 1;
		else if(strstr(versionString, "*\n") != NULL)								 // Reads until end of response and break
			break;
	}while(1);											//Note: this example expects a connected Emstat Pico and hangs otherwise!
	return isConnected;
}

int SendScriptFile(char* fileName)
{
	FILE *fp;
	char str[MS_MAX_LINECHARS+1];	//Plus the string termination character (0)

	fp = fopen(fileName, "r");
	if (fp == NULL) {
		printf("Could not open file %s", fileName);
		return FAILURE;
	}
	// Reads a single line from the script file and sends it to the device.
	while (fgets(str, MS_MAX_LINECHARS, fp) != NULL)
	{
		WriteStr(&msComm, str);
	}
	fclose(fp);
	return SUCCESS;
}

int DisplayResults(RetCode code, MeasureData result, int *nDataPoints)
{
	int continueParsing = 1;
	switch(code)
	{
	case CODE_RESPONSE_BEGIN:				// Measurement response begins
		printf("\nResponse begin\n");
		break;
	case CODE_MEASURING:
		printf("\nMeasuring... \n");
		break;
	case CODE_OK:							// Received valid package, print it.
		if(*nDataPoints == 0)
			printf("\nReceiving measurement response:\n");
		printf("\n %d \t", ++*nDataPoints);
		if (result.zreal != HUGE_VALF)		//Check if it is EIS data
		{
			printf("frequency(Hz): %8.1f \t", result.frequency);
			printf("Zreal(Ohm): %16.3f \t", result.zreal);
			printf("Zimag(Ohm): %16.3f \t", result.zimag);
			fflush(stdout);
		}
		else
		{
			printf("E(V): %6.3f \t", result.potential);
			printf("i(A) : %11.3E \t", result.current);
			fflush(stdout);
		}
		printf("Status: %-15s  ", result.status);
		printf("CR: %s ", result.cr);
		fflush(stdout);
		break;
	case CODE_MEASUREMENT_DONE:			    // Measurement loop complete
		printf("\nMeasurement completed. ");
		break;
	case CODE_RESPONSE_END:				    // Measurement response end
		printf("%d data point(s) received.", *nDataPoints);
	    fflush(stdout);
	    continueParsing = 0;
	    break;
	default:                    			// Failed to parse or identify package.
		printf("\nFailed to parse package: %d\n", code);
		continueParsing = 0;
	}
	return continueParsing;
}

void ResultsToCsv(const RetCode code, const MeasureData result, const int nDataPoints)
{
	switch(code)
	{
	case CODE_RESPONSE_BEGIN:					// Measurement response begins
		OpenCSVFile(RESULT_FILEPATHNAME, &pFCsv);
		break;
	case CODE_MEASURING:
		break;
	case CODE_OK:								// Received valid package, print it.
		if(nDataPoints == 0)
		{
			WriteHeaderToCSVFile(pFCsv);
		}
		WriteDataToCSVFile(pFCsv, result, nDataPoints);
		break;
	case CODE_MEASUREMENT_DONE:			    	// Measurement loop complete
		fclose(pFCsv);
		break;
	case CODE_RESPONSE_END:				    	// Measurement response end
	    break;
	default:                    				// Failed to parse or identify package.
		break;
	}
}

void OpenCSVFile(const char *pFilename, FILE **fp)
{
	*fp = fopen(pFilename, "w");		//Open file for writing (overwrite existing)
	if (*fp == NULL)
	{
		printf("Could not open CSV file %s (hint: make sure the directory exists)", pFilename);
	}
}

void WriteHeaderToCSVFile(FILE *fp)
{
	if (fp == NULL)
	{
		printf("Unable to write header to CSV file");
		fflush(stdout);
		return;
	}
	// Tell MS Excel that we use "," as separator
	fprintf(fp,"\"sep=,\"\n");
	// Write table header
	fprintf(fp,"Index,Potential,Current,Frequency,Zreal,Zimag,CR,Status\n");
}


void WriteDataToCSVFile(FILE *fp, MeasureData resultdata, int nDataPoints)
{
	if (fp == NULL)
	{
		printf("Unable to write to CSV file\n");
		fflush(stdout);
		return;
	}
	fprintf(fp,"%d,%f,%f,%f,%f,%f,%s,%s\n", nDataPoints+1, resultdata.potential, resultdata.current, resultdata.frequency, resultdata.zreal, resultdata.zimag, resultdata.cr, resultdata.status);
}
