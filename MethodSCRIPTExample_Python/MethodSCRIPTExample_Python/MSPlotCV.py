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
import matplotlib.pyplot as plt



#script specific settings
MSfilepath = ".\\MethodSCRIPT files"
MScriptFile = "MSExampleCV.mscr"

#combine the path and filename 
MScriptPathandFile = os.path.join(MSfilepath, MScriptFile)


#initialization and open the port
ser = serial.Serial()   #Create an instance of the serial object

myport = "COM55"                            #set the comport
if PSEsPicoLib.OpenComport(ser,myport,1):   #open myport with 1 sec timeout
    print("Succesfuly opened: " + ser.port  )
    try:
       if PSEsPicoLib.IsConnected(ser):             #Check if EmstatPico is connected
           print("Connected!")                  
           
           # Send the MethodSCRIPT file
           PSEsPicoLib.SendScriptFile(ser,MScriptPathandFile)  
           #Get the results and store it in datafile
           datafile=PSEsPicoLib.GetResults(ser)                             # fetch the results

           #Create "data" subfolder 
           (prefix, sep, suffix) = MScriptFile.rpartition('.')   #split the file-extension and the filename
           ResultFile = prefix + '.dat'                          #change the extension to .dat
           ResultPath = MSfilepath+"\\data"                      #use subfolder for the data
           try:  
               os.mkdir(ResultPath)
           except OSError:  
               print ("Creation of the directory %s failed" % ResultPath)
           else:  
               print ("Successfully created the directory %s " % ResultPath)
               
           ResultFile = os.path.join(ResultPath, ResultFile)                #combine the path and the filename
           ResultFile = PSEsPicoLib.CheckFileExistAndRename(ResultFile)     #Rename the file if it exists to a unique name by add the date+time
           #print(ResultFile)       
           f = open(ResultFile,"w+")    #Open file for writing
           f.write(datafile)            #write data to file
           f.close()                    #close file
       else:
           print("Unable to connected!")                  
    except Exception as e1:                         #catch exception 
        print("error communicating...: " + str(e1)) #print the exception
    finally:
       ser.close()                                  #close the comport
else:
    print("cannot open serial port ")


value_matrix = PSEsPicoLib.ParseResultFile(ResultFile)  #Parse result file to Value matrix

applied_potential=PSEsPicoLib.GetColumnFromMatrix(value_matrix,0)  #Get the applied potentials
measured_current=PSEsPicoLib.GetColumnFromMatrix(value_matrix,1)   #Get the measured current

plt.figure(1)
plt.plot(applied_potential,measured_current)
plt.title("Voltammogram")
plt.xlabel("Applied Potential (V)")
plt.ylabel("Measured Current (A)")
plt.show()
plt.grid(b=True, which='major')
plt.grid(b=True, which='minor', color='b', linestyle='-', alpha=0.2)
plt.minorticks_on()

    