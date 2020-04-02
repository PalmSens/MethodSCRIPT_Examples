# -*- coding: utf-8 -*-
"""
/* ----------------------------------------------------------------------------
 *         PalmSens Method SCRIPT SDK
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
"""
###############################################################################
# Description
###############################################################################
# This example showcases how to perform and plot a 
# Electrochemical Impedance Spectroscopy (EIS) measurement.

###############################################################################
# Imports
###############################################################################

import serial      
import os.path  
import PSEsPicoLib 
import matplotlib.pyplot as plt
import numpy as np
import sys

###############################################################################
# Configuration
###############################################################################

#Folder where scripts are stored
MSfilepath = ".\\MethodSCRIPT files"
#Name of script file to run
MScriptFile = "MSExampleEIS.mscr"

#COM port of the EmStat Pico
myport = "COM101"

#Set to False to disable printing of raw and parsed data
verbose_printing = True

###############################################################################
# Code
###############################################################################

#Set printing verbosity
PSEsPicoLib.SetPrintVerbose(verbose_printing)

#combine the path and filename 
MScriptPathandFile = os.path.join(MSfilepath, MScriptFile)

#used to only parse data once we have succesfully executed the script
measurement_succes = False

#initialization and open the port
ser = serial.Serial()   #Create an instance of the serial object

if PSEsPicoLib.OpenComport(ser,myport,1):   #open myport with 1 sec timeout
    print("Succesfuly opened: " + ser.port  )
    try:
       PSEsPicoLib.Flush(ser)                       #Flush the EmstatPico parse buffer
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
           
           measurement_succes = True
       else:
           print("Unable to connected!")                  
       ser.close()                                  #close the comport
    except Exception as e1:                         #catch exception 
        print("error communicating...: " + str(e1)) #print the exception
    finally:
        ser.close()
else:
    print("cannot open serial port ")

if(not measurement_succes):
   sys.exit()
    
value_matrix = PSEsPicoLib.ParseResultFile(ResultFile)  #Parse result file to Value matrix
    
applied_frequency=PSEsPicoLib.GetColumnFromMatrix(value_matrix,0)   #Get the applied frequencies
measured_zreal=PSEsPicoLib.GetColumnFromMatrix(value_matrix,1)      #Get the measured real part of the complex impedance
measured_zimag=PSEsPicoLib.GetColumnFromMatrix(value_matrix,2)      #Get the measured imaginary part of the complex impedance

#Calculate Z and Phase  
measured_zimag = -measured_zimag                          #invert the imaginary part for the electrochemist convention
Zcomplex= measured_zreal + 1j*measured_zimag              #compose the complex impedance
Zphase=np.angle(Zcomplex, deg=True)     #Get the phase from the complex impedance in degrees
Z=np.abs(Zcomplex)                      #Get the impedance value 


#show the Nyquist plot as figure 1
plt.figure(1)                           
plt.plot(measured_zreal,measured_zimag)
plt.title('Nyquist plot')
plt.axis('equal')
plt.grid()
plt.xlabel("Z\'")
plt.ylabel("-Z\'\'")


#show the Bode plot as dual y-axis
fig, ax1 = plt.subplots()

color = 'tab:red'                           #plot the impedance in Red
ax1.set_xlabel('Frequency (Hz)')            #X-axes is Frequency
ax1.set_ylabel('Z', color=color)            #axes-1 is Z (impedance)
ax1.semilogx(applied_frequency, Z, color=color)         #X-axis is logarithmic
ax1.tick_params(axis='y', labelcolor=color) #show ticks

# Turn on the minor TICKS, which are required for the minor GRID
ax1.minorticks_on()

# Customize the major grid
ax1.grid(which='major', linestyle='-', linewidth='0.1', color='black')

ax2 = ax1.twinx()  # instantiate a second axes that shares the same x-axis

color = 'tab:blue'
ax2.set_ylabel("-Phase (degrees)", color=color)  # we already handled the x-label with ax1
ax2.semilogx(applied_frequency, Zphase, color=color)
ax2.tick_params(axis='y', labelcolor=color)

fig.tight_layout()              # otherwise the right y-label is slightly clipped
plt.grid(True,which="both")
plt.title('Bode plot')
plt.show()
    