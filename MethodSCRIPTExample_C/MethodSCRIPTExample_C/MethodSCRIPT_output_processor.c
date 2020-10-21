
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
 /*
 * This file contains functions that are responsible for outputting MethodSCRIPT data to a CSV file
 * and the console.
 */


#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "MethodSCRIPTExample.h"

// CSV File pointer
FILE *pFCsv;

// Path to the CSF file to create
extern const char* RESULT_FILEPATHNAME;


///
/// Print one MethodSCRIPT output subpackage on the console.
///
/// parameters:
///   subpackage The subpackage to process
///
void PrintSubpackage(const MscrSubPackage subpackage)
{
	// Format and print the subpackage value
	// This is a bit bulky, but does nothing more than call printf with a format that is
	// sensible for the `variable type` of the subpackage.

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


	// Print metadata
	// Note a value of <0 indicates it was provided in the MethodSCRIPT output

	// `Status` field metadata
	if (subpackage.metadata.status >= 0)
	{
		const	char *status_str;
		if (subpackage.metadata.status == 0)
		{
			status_str = StatusToString(0);
		}
		else
		{
			// The `status` field is a bitmask, so we have to check every bit separately.
			// Only print the first flag that was set to keep the output readable.
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

	// `current range` metadata
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
		fprintf(fp, ",\"%.15f\"", package.subpackages[i].value);

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
			fprintf(fp, ",\"%s\"", status_str);
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


//
// Close the CSV file on the operating system.
//
void close_csv_file()
{
	if (pFCsv != NULL)
		fclose(pFCsv);
}
