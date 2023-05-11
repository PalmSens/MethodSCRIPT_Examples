/**
 * \file
 * PalmSens MethodSCRIPT SDK Example for Arduino
 *
 * This is an example of how to use the MethodSCRIPT to connect your Arduino
 * board to a MethodSCRIPT device. The example allows the user to start 
 * measurements on the MethodSCRIPT device from a PC connected to the Arduino 
 * through USB.
 *
 * Environment setup
 * -----------------
 * To run this example, you must include the MethodSCRIPT C libraries and the
 * MathHelperLibrary. To do this, follow the menu "Sketch -> Include Library
 * -> Add .ZIP Library..." and select the MethodSCRIPTComm folder. Follow the
 * same process and include the MathHelperLibrary. You should now be able to
 * compile the example.
 *
 * Hardware setup
 * --------------
 * To run this example, connect your Arduino MKRZERO "Serial" port Rx, Tx and
 * GND to the MethodSCRIPT device Serial Tx, Rx and GND respectively.
 * 
 * Note (EmStat Pico Dev Board):  Make sure the UART switch block SW4 on the
 * EmStat Pico dev board has the switches for MKR 3 and 4 turned on. The Arduino 
 * board should be connected normally to a PC. If not powering the EmStat Pico 
 * by other means, it should be connected to the PC through USB for power.
 * 
 * Note (EmStat4 Dev Board): Make sure the switches (SW1, SW10, SW4-7) are set 
 * according to the manual when using the EmStat4 dev board.
 *
 * How to use
 * ----------
 * Compile and upload this sketch through the Arduino IDE. Next, open a serial
 * monitor to the Arduino (you can do this from the Arduino IDE). You should
 * see messages being printed containing measured data from the MethodSCRIPT device.
 *
 * ----------------------------------------------------------------------------
 *
 *	\copyright (c) 2019-2023 PalmSens BV
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

#include <Arduino.h>
#include "MathHelpers.h"
#include "MSComm.h"

// Select demo
// 0 = LSV (connect to WE B of PalmSens Dummy Cell = 10k ohm resistor)
// 1 = EIS (connect to WE C of PalmSens Dummy Cell = Randless Circuit)
// 2 = SWV (connect to WE B of PalmSens Dummy Cell = 10k ohm resistor)
#define DEMO_SELECT  0

// Please uncomment one of the lines below to select your device type
//#define DEVICE_TYPE DT_ESPICO
//#define DEVICE_TYPE DT_ES4

#ifndef DEVICE_TYPE
  #error "No device type selected"
#endif

// Please uncomment one of the lines below to select the correct baudrate for your MethodSCRIPT device
// EmStat Pico: 230400
// EmStat4:     921600
//#define MSCRIPT_DEV_BAUDRATE 921600
//#define MSCRIPT_DEV_BAUDRATE 230400

#ifndef MSCRIPT_DEV_BAUDRATE
  #error "No baudrate selected"
#endif

int _nDataPoints = 0;
char _versionString[30];

static bool s_printSent = false;
static bool s_printReceived = false;
char const * CMD_VERSION_STRING = "t\n";

// LSV MethodSCRIPT
char const * LSV_ON_10KOHM = "e\n"
                             "var c\n"
                             "var p\n"
                             "set_pgstat_mode 3\n"
                             "set_max_bandwidth 200\n"
                             "set_range ba 500u\n"
                             "set_e -500m\n"
                             "cell_on\n"
                             "wait 1\n"
                             "meas_loop_lsv p c -500m 500m 50m 100m\n"
                             "pck_start\n"
                             "pck_add p\n"
                             "pck_add c\n"
                             "pck_end\n"
                             "endloop\n"
                             "cell_off\n"
                             "\n";

// SWV MethodSCRIPT
char const * SWV_ON_10KOHM = "e\n"
                             "var p\n"
                             "var c\n"
                             "var f\n"
                             "var r\n"
                             "set_pgstat_mode 3\n"
                             "set_max_bandwidth 100000\n"
                             "set_range ba 100u\n"
                             "set_e -500m\n"
                             "cell_on\n"
                             "wait 1\n"
                             "meas_loop_swv p c f r -500m 500m 5m 25m 10\n"
                             "pck_start\n"
                             "pck_add p\n"
                             "pck_add c\n"
                             "pck_end\n"
                             "endloop\n"
                             "cell_off\n"
                             "\n";

// EIS MethodSCRIPT
char const * EIS_ON_WE_C =   "e\n"
                             // declare variables for frequency, real and imaginary parts of complex result
                             "var f\n"
                             "var r\n"
                             "var j\n"
                             // set to channel 0 (Lemo)
                             "set_pgstat_chan 0\n"
                             // set mode to High Speed
                             "set_pgstat_mode 3\n"
                             // set to 50 uA current range
                             "set_range ba 50u\n"
                             "set_autoranging ba 50u 500u\n"
                             "cell_on\n"
                             // call the EIS loop with 15 mV amplitude, f_start = 200 kHz, f_end = 100 Hz, nrOfPoints = 51, 0 mV DC
                             "meas_loop_eis f r j 15m 200k 100 51 0m\n"
                             // add the returned variables to the data package
                             "pck_start\n"
                             "pck_add f\n"
                             "pck_add r\n"
                             "pck_add j\n"
                             "pck_end\n"
                             "endloop\n"
                             "on_finished:\n"
                             "cell_off\n"
                             "\n";

// The MethodSCRIPT communication object.
MSComm _msComm;

// We have to give MSComm some functions to communicate with the MethodSCRIPT device (in MSCommInit).
// However, because the C compiler doesn't understand C++ classes,
// we must wrap the write/read functions from the Serial class in a normal function first.
// We are using Serial and SerialUSB here, but you can use any serial port.
int write_wrapper(char c)
{
  if (s_printSent) {
    // Send all data to PC if required for debugging purposes (s_printReceived to be set to true)
    SerialUSB.write(c);
  }
  // Write a character to the device
  return Serial.write(c);
}

int read_wrapper()
{
  // Read a character from the device
  int c = Serial.read();

  if (s_printReceived && (c != -1)) { // -1 means no data
    // Send all received data to PC if required for debugging purposes (s_printReceived to be set to true)
    SerialUSB.write(c);
  }
  return c;
}

// Verify if connected to known MethodSCRIPT device
int VerifyMSDevice()
{
  int i = 0;
  int isConnected = 0;
  RetCode code;

  SendScriptToDevice(CMD_VERSION_STRING);
  while (!Serial.available()) {
    // Wait until data is available on Serial
  }
  while (Serial.available()) {
    code = ReadBuf(&_msComm, _versionString);
    if (code == CODE_VERSION_RESPONSE) {
      if (strstr(_versionString, "espbl") != NULL) {
        SerialUSB.println("EmStat Pico is connected in boot loader mode.");
        isConnected = 0;

      // Verify if the device is EmStat Pico by looking for "espico" in the version response
      } else if (strstr(_versionString, "espico") != NULL) {
        SerialUSB.println("Connected to EmStat Pico");
        isConnected = 1;
      }
      else if (strstr(_versionString, "es4_lr") != NULL) {
        SerialUSB.println("Connected to EmStat4 LR");
        isConnected = 1;
      }
      else if (strstr(_versionString, "es4_hr") != NULL) {
        SerialUSB.println("Connected to EmStat4 HR");
        isConnected = 1;
      }
      else if (strstr(_versionString, "es4_bl") != NULL) {
        SerialUSB.println("Connected to EmStat4 Bootloader");
        isConnected = 0;
      }

    //Read until end of response and break
    } else if(strstr(_versionString, "*\n") != NULL) {
      break;

    } else {
      SerialUSB.println("Connected device is not MethodSCRIPT device");
      isConnected = 0;
    }
    delay(20);
  }
  return isConnected;
}

// The MethodSCRIPT is sent to the device through the Serial port
void SendScriptToDevice(char const * scriptText)
{
  WriteStr(&_msComm, scriptText);
}

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
    SerialUSB.print("\tE[V]: ");
    SerialUSB.print(sci(subpackage.value, 3));
    break;

  case MSCR_VT_CURRENT:
  case MSCR_VT_CURRENT_GENERIC1:
  case MSCR_VT_CURRENT_GENERIC2:
  case MSCR_VT_CURRENT_GENERIC3:
  case MSCR_VT_CURRENT_GENERIC4:
    SerialUSB.print("\tI[A]: ");
    SerialUSB.print(sci(subpackage.value, 3));
    break;

  case MSCR_VT_ZREAL:
    SerialUSB.print("\tZreal[ohm]: ");
    SerialUSB.print(sci(subpackage.value, 3));
    break;

  case MSCR_VT_ZIMAG:
    SerialUSB.print("\tZimag[ohm]: ");
    SerialUSB.print(sci(subpackage.value, 3));
    break;

  case MSCR_VT_CELL_SET_POTENTIAL:
    SerialUSB.print("\tE set[V]: ");
    SerialUSB.print(sci(subpackage.value, 3));
    break;

  case MSCR_VT_CELL_SET_CURRENT:
    SerialUSB.print("\tI set[A]: ");
    SerialUSB.print(sci(subpackage.value, 3));
    break;

  case MSCR_VT_CELL_SET_FREQUENCY:
    SerialUSB.print("\tF set[Hz]: ");
    SerialUSB.print(sci(subpackage.value, 3));
    break;

  case MSCR_VT_CELL_SET_AMPLITUDE:
    SerialUSB.print("\tA set[V]: ");
    SerialUSB.print(sci(subpackage.value, 2));
    break;

  case MSCR_VT_UNKNOWN:
  default:
    char formatted_srt[64];
    snprintf(formatted_srt, 64, "\t?%d?[?] %16.3f ", subpackage.variable_type, subpackage.value);
    SerialUSB.print(formatted_srt);
  }

  // Print metadata
  // Note a value of <0 indicates it was provided in the MethodSCRIPT output

  // `Status` field metadata
  if (subpackage.metadata.status >= 0) {
    char const * status_str;
    if (subpackage.metadata.status == 0) {
      status_str = StatusToString((Status)0);
    } else {
      // The `status` field is a bitmask, so we have to check every bit separately.
      // Only print the first flag that was set to keep the output readable.
      for (int i = 0; i < 31; i++) {
        if ((subpackage.metadata.status & (1 << i)) != 0) {
          status_str = StatusToString((Status)(1 << i));
          break;
        }
      }
    }

    char formatted_srt[64];
    snprintf(formatted_srt, 64, "\tstatus: %-16s \t", status_str);
    SerialUSB.print(formatted_srt);
  }

  // `range` metadata
  if (subpackage.metadata.range >= 0) {
    char const * range_str = range_to_string(subpackage.metadata.range, (VarType)subpackage.variable_type);
    char formatted_srt[64];
    snprintf(formatted_srt, 64, "Range: %-20s \t", range_str);
    SerialUSB.print(formatted_srt);
  }
}


// The entry point of the Arduino code
void setup()
{
  // Init serial ports
  // SerialUSB is the Arduino serial port communicating with the PC
  SerialUSB.begin(230400);
  // Serial is the port communicating with the MethodSCRIPT device
  Serial.begin(MSCRIPT_DEV_BAUDRATE);
  
  //Wait for USB port to be active
  while (!SerialUSB) {
    /* do nothing */
  }

  //Wait for MethodSCRIPT port to be active
  while (!Serial) {
    /* do nothing */
  }
  
  if (s_printReceived == true) {
    SerialUSB.println("s_printReceived");
  }

  //Init MSComm struct (one for every MethodSCRIPT device).
  RetCode code = MSCommInit(&_msComm, DEVICE_TYPE, &write_wrapper, &read_wrapper);
  if (code == CODE_OK) {
    if (VerifyMSDevice()) {
#if DEMO_SELECT == 0
      SendScriptToDevice(LSV_ON_10KOHM); //Send the "LSV_ON_10KOHM" MethodSCRIPT to the device
#elif DEMO_SELECT == 1
      SendScriptToDevice(EIS_ON_WE_C);   //Send the "EIS_ON_WE_C" MethodSCRIPT to the device
#else // DEMO_SELECT == 2
      SendScriptToDevice(SWV_ON_10KOHM); //Send the "SWV_ON_10KOHM" MethodSCRIPT to the device
#endif
    }
  }
}

int package_nr = 0;

void loop()
{
  // put your main code here, to run repeatedly:
  MscrPackage package;
  char current[20];
  char readingStatus[16];
  // If we have any buffered messages waiting for us
  while (Serial.available()) {
    // Read from the device and parses the response
    RetCode code = ReceivePackage(&_msComm, &package);
    switch (code) {
    case CODE_RESPONSE_BEGIN: // Measurement response begins
      SerialUSB.println();
      SerialUSB.print("Response begin");
      SerialUSB.println();
      break;
    case CODE_MEASURING:
      SerialUSB.println();
      SerialUSB.print("Measuring...");
      SerialUSB.println();
      package_nr = 0;
      break;
    case CODE_OK: // Received valid package, print it.
      if (package_nr++ == 0) {
        SerialUSB.println();
        SerialUSB.println("Receiving measurement response:");
      }
      // Print package index (starting at 1 on the console)
      SerialUSB.print(package_nr);
      // Print all subpackages in
      for (int i = 0; i < package.nr_of_subpackages; i++) {
        PrintSubpackage(package.subpackages[i]);
      }
      SerialUSB.println();
      break;
    case CODE_MEASUREMENT_DONE: // Measurement loop complete
      SerialUSB.println();
      SerialUSB.println("Measurement completed.");
      break;
    case CODE_RESPONSE_END: // Measurement response end
      SerialUSB.print(package_nr);
      SerialUSB.println(" data point(s) received.");
      break;
    default: // Failed to parse or identify package
      SerialUSB.println();
      SerialUSB.print("Failed to parse package: ");
      SerialUSB.println(code);
    }
  }
}
