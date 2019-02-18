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
 * This is an example of how to use the EmStat pico SDK to connect your Arduino board to an EmStat pico.
 * The example allows the user to start measurements on the EmStat pico from the PC connected to the Arduino through USB.
 * 
 * Environment setup:
 * To run this example, you must include the EmStat pico SDK as a library first.
 * To do this, follow the menu "Sketch -> Include Library -> Add .ZIP Library..." and select the PalmSensComm folder.
 * You should now be able to compile the example.
 * 
 * Hardware setup:
 * To run this example, connect your Arduino "Serial1" port Rx, Tx and GND to the EmStat pico Serial Tx, Rx and GND respectively.
 * Note:  This example assumes that "Serial" is your PC connection (probably USB) and the EmStat pico is connected on "Serial1".
 * The Arduino board should be connected normally to a PC. If not powering the EmStat by other means, the EmStat pico should 
 * be connected to the PC through USB for power.
 * 
 * How to use:
 * Compile and upload this sketch through the Arduino IDE. 
 * Next, open a serial monitor to the Arduino (you can do this from the Arduino IDE). 
 * You should see messages being printed containing measured data from the EmStat pico. 
 */

//Because PSComm is a C library and Arduino uses a C++ compiler, we must use the "extern "C"" wrapper.
extern "C" {
  #include <PSComm.h>
  #include <MathHelpers.C>
};
#include <Arduino.h>
#include "wiring_private.h"

//Create a new UART instance assigning it to TX (14) and RX (13) pins on the Arduino
Uart Serial1(&sercom5, 14, 13, SERCOM_RX_PAD_3, UART_TX_PAD_2); 

int nDataPoints = 0;
char versionString[29];


bool PrintSent = false;
bool PrintReceived = false;
const char* Cmd_VersionString = "t\n";
const char* LSV_MethodScript = "e\n" 
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
                               
const char* SWV_MethodScript = "e\n" 
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


//The EmStat pico communication object.
PSComm espComm;

//We have to give PSComm some functions to communicate with the EmStat pico (in PSCommInit).
//However, because the C compiler doesn't understand C++ classes,
//we must wrap the write/read functions from the Serial class in a normal function, first.
//We are using Serial and Serial1 here, but you can use any serial port.
int write_wrapper(char c)
{
  if(PrintSent == true)
  {
    //Send all data to PC as well for debugging purposes
    Serial.write(c);
  }
  return Serial1.write(c);
  return 0;
}

int read_wrapper()
{
  int c = Serial1.read();
  
  if(PrintReceived == true && c != -1) //-1 means no data
  {
    //Send all received data to PC for debugging purposes
    Serial.write(c);
  }
  return c;
}

// Attach the interrupt handler to the SERCOM
void SERCOM5_Handler()
{
 Serial1.IrqHandler();
}

//Verify if connected to EmStat pico by checking the version string contains the "esp"
int VerifyESPico()
{
  int i = 0;
  SendScriptToDevice(Cmd_VersionString);
  while (!Serial1.available()){;}
  while (Serial1.available())
  {
    char incomingByte_pico = Serial1.read(); 
    versionString[i++] = incomingByte_pico;
    if(incomingByte_pico == '\n')
    {
      versionString[i] = '\0';
      break;
    }
    delay(20);
  }
  if(strstr(versionString, "esp"))
  {
    Serial.println("Connected to Emstat pico.");
  }
  return 1;
}

//The method script is written on the device through the Serial1 port 
void SendScriptToDevice(const char* scriptText)
{
  for(int i = 0; i < strlen(scriptText); i++)
  {
    Serial1.write(scriptText[i]);
  }
}

void setup()
{
  // put your setup code here, to run once:
  
  //Init serial ports
  Serial.begin(230400);     
  Serial1.begin(230400);      
  while(!Serial){;}

  pinPeripheral(13, PIO_SERCOM_ALT);			//Assign SDA function to pin 13
  pinPeripheral(14, PIO_SERCOM_ALT); 			//Assign SCL function to pin 14

  //Init PSComm struct (one for every EmStat pico).
  PSCommInit(&espComm,  2000, &write_wrapper, &read_wrapper, &millis);
  VerifyESPico();
  SendScriptToDevice(LSV_MethodScript);
  //SendScriptToDevice(SWV_MethodScript);
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
    //Read from the device and try to identify and parse a package
    RetCode code = ReceivePackage(&espComm, &data);
    if(code == CODE_MEASURING)
    {
      Serial.println("\nMeasuring... ");
    }
    else if(code == CODE_OK)
    {
      if(nDataPoints == 0)
          Serial.println("\nReceiving measurement response:");
      //Received valid package, print it.
      Serial.print("\n");
      Serial.print(++nDataPoints);
      Serial.print("\tE (V): ");
      Serial.print(data.potential, 3);
      Serial.print("\t\ti (A): ");
      //Serial.print(data.current, 12);
      Serial.print(sci(data.current, 3));
      Serial.print("\tStatus: ");
      sprintf(readingStatus, "%-15s", data.status);
      Serial.print(readingStatus);
      Serial.print("\tCR: ");
      Serial.print(data.cr);
    }
    else if(code == CODE_MEASUREMENT_DONE)
    {
      Serial.println("\n");
      Serial.print("Measurement completed. ");
    }
    else if(code == CODE_RESPONSE_END)
    {
       Serial.print(nDataPoints);
       Serial.print(" data point(s) received.");
    }
    else
    {
      //Failed to parse or identify package.
      Serial.print("\nFailed to parse package: ");
      Serial.println(code);
    }
   }    
}
