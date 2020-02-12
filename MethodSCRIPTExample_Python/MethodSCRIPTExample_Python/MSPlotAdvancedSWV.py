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
# This example showcases some of the more advanced features of the PSEsPicoLib.
# The example runs a script that contains two SWV measurements. It can also be
# easily converted to parse any MethodSCRIPT output by changing the plotted 
# columns in the configuration section.
# The following features are showcased in this example:
# - Running and plotting a Square Wave Voltammetry (SWV) measurement
# - Plotting multiple curves or measurements in one plot
# - Plotting multiple measured values against on x axis
# - Extracting the type of measured values from the received data
# - Exporting measurement data to CSV

###############################################################################
# Imports
###############################################################################

import serial      
import os.path  
import PSEsPicoLib 
import matplotlib.pyplot as plt
import sys

###############################################################################
# Configuration
###############################################################################

#Folder where scripts are stored
MSfilepath = ".\\MethodSCRIPT files"
#Name of script file to run
MScriptFile = "MSExampleAdvancedSWV.mscr"

#column index to put on the x axis
xaxis_icol = 0
#column indices to put on the y axis, can be multiple but must be same type
yaxis_icols = [1, 2, 3]
#names for each column
yaxis_col_names = ['Potential', 'Current', 'Forward Current', 'Reverse Current']

#COM port of the EmStat Pico
myport = "COM9"

#Set to False to disable printing of raw and parsed data
verbose_printing = True

###############################################################################
# Code
###############################################################################

#Set printing verbosity
PSEsPicoLib.SetPrintVerbose(verbose_printing)

#used to only parse data once we have succesfully executed the script
measurement_succes = False

#combine the path and filename 
MScriptPathandFile = os.path.join(MSfilepath, MScriptFile)

#initialization and open the port
ser = serial.Serial()   #Create an instance of the serial object

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
           
           measurement_succes = True
       else:
           print("Unable to connected!")                  
    except Exception as e1:                         #catch exception 
        print("error communicating...: " + str(e1)) #print the exception
    finally:
       ser.close()                                  #close the comport
else:
    print("cannot open serial port ")
    
if(not measurement_succes):
   sys.exit()
   
#Parse result file to Value matrix and value type matrix
value_matrix, vts = PSEsPicoLib.ParseResultFileWithVT(ResultFile)

#Convert data to CSV string
csv = PSEsPicoLib.MatrixToCSV(value_matrix, vts)

#Get filepath to save CSV to
(prefix, sep, suffix) = MScriptFile.rpartition('.')     #split the file-extension and the filename
CSVFile = prefix + '.csv'                               #change the extension to .csv
CSVFile = os.path.join(MSfilepath + "\\data", CSVFile)  #join path to data folder and filename
CSVFile = PSEsPicoLib.CheckFileExistAndRename(CSVFile)  #check that we can save here

#Write CSV to file
f = open(CSVFile,"w+")      #Open file for writing
f.write(csv)                #write data to file
f.close()                   #close file
           
#Comment out close to prevent closing previous plot before each run
plt.close()
plt.figure()
plt.title(os.path.basename(MScriptFile))
#Put specified column of the first curve on x axis
xvar_type = PSEsPicoLib.GetVarType(vts, xaxis_icol);
plt.xlabel(PSEsPicoLib.GetVarTypeName(xvar_type) + ' (' + PSEsPicoLib.GetVarTypeUnit(xvar_type) + ')')
#Put specified column of the first curve on y axis
yvar_type = PSEsPicoLib.GetVarType(vts, yaxis_icols[0]);
plt.ylabel(PSEsPicoLib.GetVarTypeName(yvar_type) + ' (' + PSEsPicoLib.GetVarTypeUnit(yvar_type) + ')')
plt.grid(b=True, which='major')
plt.grid(b=True, which='minor', color='b', linestyle='-', alpha=0.2)
plt.minorticks_on()

#Loop through all curves and plot them
ncurves = PSEsPicoLib.GetCurveCount(value_matrix) #Query amount of curves found
for icurve in range(ncurves):
    #Get xaxis for this curve
    xaxis = PSEsPicoLib.GetColumnFromMatrix(value_matrix, xaxis_icol, icurve)
    #loop through all yaxis columns and plot one curve per column
    ncols = PSEsPicoLib.GetVarTypeCols(vts, icurve);
    for icol in yaxis_icols:
        #ignore invalid columns
        if(icol < ncols and PSEsPicoLib.GetVarType(vts, icol, icurve) == yvar_type): 
            #Get the yaxis for this column
            yaxis=PSEsPicoLib.GetColumnFromMatrix(value_matrix, icol, icurve)
            #Plot curve y axis against global x axis
            #If there are multiple curves, add curve index to label
            if(ncurves > 1):
                plt.plot(xaxis, yaxis, label= yaxis_col_names[icol] + ' vs ' + yaxis_col_names[xaxis_icol] + ' ' + str(icurve))
            else:
                plt.plot(xaxis, yaxis, label= yaxis_col_names[icol] + ' vs ' + yaxis_col_names[xaxis_icol])

#Generate legend and show plot
plt.legend()
plt.show()
