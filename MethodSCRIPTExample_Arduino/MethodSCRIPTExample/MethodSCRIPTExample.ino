/* ----------------------------------------------------------------------------
 *         PalmSens MethodSCRIPT SDK Example
 * ----------------------------------------------------------------------------
 * Copyright (c) 2016-2020, PalmSens BV
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
 * This is an example of how to use the MethodSCRIPT to connect your Arduino board to an EmStat Pico.
 * The example allows the user to start measurements on the EmStat Pico from a windows PC connected to the Arduino through USB.
 *
 * Environment setup:
 * To run this example, you must include the MethodSCRIPT C libraries and the MathHelperLibrary.
 * To do this, follow the menu "Sketch -> Include Library -> Add .ZIP Library..." and select the MethodSCRIPTComm folder.
 * Follow the same process and include the MathHelperLibrary.
 * You should now be able to compile the example.
 *
 * Hardware setup:
 * To run this example, connect your Arduino MKRZERO "Serial1" port Rx, Tx and GND to the EmStat Pico Serial Tx, Rx and GND respectively.
 * Note:  Make sure the UART switch block SW4 on the EmStat dev board has the switches for MKR 3 and 4 turned on.
 * The Arduino board should be connected normally to a PC. If not powering the EmStat Pico by other means, it should
 * be connected to the PC through USB for power.
 *
 * How to use:
 * Compile and upload this sketch through the Arduino IDE.
 * Next, open a serial monitor to the Arduino (you can do this from the Arduino IDE).
 * You should see messages being printed containing measured data from the EmStat Pico.
 */

//Because MSComm is a C library and Arduino uses a C++ compiler, we must use the "extern "C"" wrapper.
extern "C" {
	#include <MSComm.h>
	#include <MathHelpers.h>
};
#include <Arduino.h>


int _nDataPoints = 0;
char _versionString[30];


// Select demo
// 0 = LSV (with 10k Ohm)
// 1 = EIS
// 2 = SWV (with 10k Ohm)
#define DEMO_SELECT	0

static bool s_printSent = false;
static bool s_printReceived = false;
const char* CMD_VERSION_STRING = "t\n";

//LSV MethodSCRIPT
const char* LSV_ON_10KOHM = "e\n"
                            "var c\n"
                            "var p\n"
                            "set_pgstat_mode 3\n"
                            "set_max_bandwidth 200\n"
                            "set_cr 500u\n"
                            "set_e -500m\n"
                            "cell_on\n"
                            "wait 1\n"
                            "meas_loop_lsv p c -500m 500m 50m 100m\n"
                            "pck_start\n"
                            "pck_add p\n"
                            "pck_add c\n"
                            "pck_end\n"
                            "endloop\n"
                            "cell_off\n\n";

//SWV MethodSCRIPT
const char* SWV_ON_10KOHM = "e\n"
                            "var p\n"
                            "var c\n"
                            "var f\n"
                            "var r\n"
                            "set_pgstat_mode 3\n"
                            "set_max_bandwidth 100000\n"
                            "set_cr 500n\n"
                            "set_e -500m\n"
                            "cell_on\n"
                            "wait 1\n"
                            "meas_loop_swv p c f r -500m 500m 5m 25m 10\n"
                            "pck_start\n"
                            "pck_add p\n"
                            "pck_add c\n"
                            "pck_end\n"
                            "endloop\n"
                            "cell_off\n\n";

//EIS MethodSCRIPT
const char* EIS_ON_WE_C =   "e\n"
                            "#declare variables for freq, real,imaginary parts of complex result\n"
                            "var f\n"
                            "var r\n"
                            "var j\n"
                            "#set to channel 0 (Lemo)\n"
                            "set_pgstat_chan 0\n"
                            "#set mode to High speed\n"
                            "set_pgstat_mode 3\n"
                            "#Set to 1mA cr\n"
                            "set_cr 50u\n"
                            "set_autoranging 50u 500u\n"
                            "cell_on\n"
                            "#call the EIS loop with 15mV amplitude,fstart=200kHz,fend=100Hz,nrOfPoints=51, 0mV DC\n"
                            "meas_loop_eis f r j 15m 200k 100 51 0m\n"
                            "#add the returned variables to the data package\n"
                            "pck_start\n"
                            "pck_add f\n"
                            "pck_add r\n"
                            "pck_add j\n"
                            "pck_end\n"
                            "endloop\n"
                            "on_finished:\n"
                            "cell_off\n\n";




//The MethodSCRIPT communication object.
MSComm _msComm;

//We have to give MSComm some functions to communicate with the EmStat Pico (in MSCommInit).
//However, because the C compiler doesn't understand C++ classes,
//we must wrap the write/read functions from the Serial class in a normal function, first.
//We are using Serial and Serial1 here, but you can use any serial port.
int write_wrapper(char c)
{
	if(s_printSent == true)
	{
		Serial.write(c);               //Sends all data to PC, if required for debugging purposes (s_printReceived to be set to true)
	}
	return Serial1.write(c);        //Writes a character to the device
}

int read_wrapper()
{
	int c = Serial1.read();         //Reads a character from the device

	if((s_printReceived == true) && (c != -1)) //-1 means no data
	{
		Serial.write(c);            //Sends all received data to PC, if required for debugging purposes (s_printReceived to be set to true)
	}
	return c;
}

//Verify if connected to EmStat Pico by checking the version string contains the "espico"
int VerifyESPico()
{
	int i = 0;
	int isConnected = 0;
	RetCode code;

	SendScriptToDevice(CMD_VERSION_STRING);
	while (!Serial1.available());                                   //Waits until Serial1 is available
	while (Serial1.available())
	{
	   code = ReadBuf(&_msComm, _versionString);
	   if(code == CODE_VERSION_RESPONSE)
	   {
	      if(strstr(_versionString, "espbl") != NULL)
	      {
	      	Serial.println("EmStat Pico is connected in boot loader mode.");
	      	isConnected = 0;
	      }
	      else if(strstr(_versionString, "espico") != NULL)           //Verifies if the device is EmStat Pico by looking for "espico" in the version response
	      {
	      	Serial.println("Connected to EmStat Pico");
	      	isConnected = 1;
	      }
	   }
	   else if(strstr(_versionString, "*\n") != NULL)                //Reads until end of response and break
	   {
	      break;
	   }
	   else
	   {
	      Serial.println("Connected device is not EmStat Pico");
	      isConnected = 0;
	   }
	   delay(20);
	}
	return isConnected;
}

//The MethodSCRIPT is sent to the device through the Serial1 port
void SendScriptToDevice(const char* scriptText)
{
	WriteStr(&_msComm, scriptText);
}

//The entry point of the Arduino code
void setup()
{
	// put your setup code here, to run once:
	//Init serial ports
	//Serial is the Arduino serial port communicating with the PC
	Serial.begin(230400);
	//Serial1 is the port on EmStat Pico dev board communicating with the Arduino
	Serial1.begin(230400);
	//Waits until the Serial port is active
	while(!Serial);

	if(s_printReceived == true)
	{
		Serial.println("s_printReceived");
	}

	//Init MSComm struct (one for every EmStat Pico).
	RetCode code = MSCommInit(&_msComm, &write_wrapper, &read_wrapper);
	if( code == CODE_OK)
	{
		if(VerifyESPico())
		{
#if DEMO_SELECT == 0
			SendScriptToDevice(LSV_ON_10KOHM);    //Sends the "LSV_ON_10KOHM" MethodSCRIPT to the device
#elif DEMO_SELECT == 1
			SendScriptToDevice(EIS_ON_WE_C);    //Sends the "EIS_ON_WE_C" MethodSCRIPT to the device
#else // DEMO_SELECT == 2
			SendScriptToDevice(SWV_ON_10KOHM);	//Sends the "SWV_ON_10KOHM" MethodSCRIPT to the device
#endif
		}
	}
}

void loop()
{
  // put your main code here, to run repeatedly:
  MeasureData data;
  char current[20];
  char readingStatus[16];
  //If we have any buffered messages waiting for us
  while (Serial1.available())
  {
		RetCode code = ReceivePackage(&_msComm, &data);     //Reads from the device and parses the response
		switch(code)
		{
		case CODE_RESPONSE_BEGIN:                         //Measurement response begins
			// do nothing
			break;
		case CODE_MEASURING:
			Serial.println("\nMeasuring... ");
			break;
		case CODE_OK:                                     //Received valid package, prints it.
			if(_nDataPoints == 0)
				Serial.println("\nReceiving measurement response:");
			Serial.print("\n");
			Serial.print(++_nDataPoints);
			if (data.zreal != HUGE_VALF)
			{
				Serial.print("\tFrequency(Hz): ");
				Serial.print(sci(data.frequency, 3));
				Serial.print("\tZreal(Ohm): ");
				Serial.print(sci(data.zreal, 3));
				Serial.print("\tZimag(Ohm): ");
				Serial.print(sci(data.zimag, 3));
			}
			else
			{
				Serial.print("\tE (V): ");
				Serial.print(data.potential, 3);
				Serial.print("\t\ti (A): ");
				Serial.print(sci(data.current, 3));
			}
			Serial.print("\tStatus: ");
			sprintf(readingStatus, "%-15s", data.status);
			Serial.print(readingStatus);
			Serial.print("\tCR: ");
			Serial.print(data.cr);
			break;
		case CODE_MEASUREMENT_DONE:                      //Measurement loop complete
			Serial.println("\n");
			Serial.print("Measurement completed. ");
			break;
		case CODE_RESPONSE_END:                          //Measurement response end
			Serial.print(_nDataPoints);
			Serial.print(" data point(s) received.");
			break;
		default:                                         //Failed to parse or identify package.
			Serial.print("\nFailed to parse package: ");
			break;
    }
  }
}
