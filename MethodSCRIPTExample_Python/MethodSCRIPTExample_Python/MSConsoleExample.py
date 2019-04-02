# -*- coding: utf-8 -*-
"""
/* ----------------------------------------------------------------------------
 *         PalmSens Method SCRIPT SDK
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
"""

import serial      
import os.path  
import PSEsPicoLib 


#script specific settings
MSfilepath = ".\\MethodSCRIPT files"
MScriptFile = "MSExampleCV.mscr"

#combine the path and filename 
MScriptFile = os.path.join(MSfilepath, MScriptFile)


#initialization and open the port
ser = serial.Serial()   #Create an instance of the serial object

myport = "COM55"                            #set the comport
if PSEsPicoLib.OpenComport(ser,myport,1):   #open myport with 1 sec timeout
    print("Succesfuly opened: " + ser.port  )
    try:
       if PSEsPicoLib.IsConnected(ser):             #Check if EmstatPico is connected
           print("Connected!")                  
           print(PSEsPicoLib.GetVersion(ser))       #Print the version 
           with open(MScriptFile) as f:             #open the script file
                content = f.readlines()             #read all the contents
                for scriptline in content:          #read the content line by line
                    print(scriptline.strip())       #print the line to send without the linefeed (stripped)
                    ser.write(bytes(scriptline,  'ascii'))  #send the scriptline to the device
           #Fetch the data comming back from the device         
           while True:
                response = ser.readline()                #read until linefeed is read or timeout is expired
                res_line = response.decode("ascii")      #decode the returned bytes to an ascii string
                print("read data: " + res_line.strip())  #print the data read without the linefeed 
                if res_line.startswith('P'):             #data point start with P
                    pck = res_line[1:len(res_line)]      #ignore last and first character
                    for v in pck.split(';'):             #value fields are seperated by a semicolon
                        str_vt = v[0:2]                  #get the value-type 
                        str_var = v[2:2+8]               #strip out value type, we ignore it for now
                        value = PSEsPicoLib.ParseVarString(str_var)   #Parse the value
                        var_type = PSEsPicoLib.GetVarType(str_vt)   #Get the variable type  
                        var_unit = PSEsPicoLib.GetValueUnit(str_vt)   #Get the unit      
                        print(var_type + "=" +str(value) + " " + str(var_unit) )
                if (res_line == '\n'):                   #Check on termination of data from the device
                    break                                #exit while loop

       else:
           print("Unable to connected!")                  
    except Exception as e1:                         #catch exception 
        print("error communicating...: " + str(e1)) #print the exception
    finally:
       ser.close()                                  #close the comport
else:
    print("cannot open serial port ")
    