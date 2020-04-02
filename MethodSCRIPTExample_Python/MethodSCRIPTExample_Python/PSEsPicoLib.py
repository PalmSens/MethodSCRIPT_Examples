# -*- coding: ascii -*-
"""
/* ----------------------------------------------------------------------------
 *         PalmSens Method SCRIPT SDK V1.2
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
 
 Revision History
 1.0: 
 - Initial release
 
 1.1:
 - Added way of enabling verbose printing of data
 - Added parsing of multiple curves in one measurement
 - CheckFileExistAndRename now supports any suffix, not just .dat
 - Added support for parsing integer values
 - Added option to export to CSV
 - Added support for more value types
 - Added more VT support functions:
     GetVarType, GetVarTypeCols, GetValueMatrixWithVT, ParseResultFileWithVT
 - Removed hibernate as this is now deprecated. Hibernation is now done through MethodSCRIPT.

"""
import serial
import serial.tools.list_ports
import datetime
import os.path
import numpy as np

PSEsPicoLibVersion = "1.1"

print_verbose = False

#dictionary list for conversion of the SI prefixes
sip_factor = [{"si":"a", "factor": 1e-18},      #atto 
              {"si":"f", "factor": 1e-15},      #femto
              {"si":"p", "factor": 1e-12},      #pico
              {"si":"n", "factor": 1e-9 },      #nano
              {"si":"u", "factor": 1e-6 },      #micro
              {"si":"m", "factor": 1e-3 },      #mili
              {"si":" ", "factor": 1.0  },      # -
              {"si":"i", "factor": 1.0  },      #integer
              {"si":"k", "factor": 1e3  },      #kilo
              {"si":"M", "factor": 1e6  },      #Mega
              {"si":"G", "factor": 1e9  },      #Giga
              {"si":"T", "factor": 1e12 },      #Terra
              {"si":"P", "factor": 1e15 },      #Peta
              {"si":"E", "factor": 1e18 }]      #Exa

#dictionary containing all used var types for MethodSCRIPT
ms_var_types = [  {"vt":"aa", "type": "unknown"             , "unit" : " " },
                  {"vt":"ab", "type": "WE vs RE potential"  , "unit" : "V" },
                  {"vt":"ac", "type": "CE potential"        , "unit" : "V" },
                  #{"vt":"ad", "type": "WE potential"        , "unit" : "V" },
                  {"vt":"ae", "type": "RE potential"        , "unit" : "V" },
                  {"vt":"ag", "type": "WE vs CE potential"  , "unit" : "V" },
                  
                  {"vt":"as", "type": "AIN0 potential"      , "unit" : "V" },
                  {"vt":"at", "type": "AIN1 potential"      , "unit" : "V" },
                  {"vt":"au", "type": "AIN2 potential"      , "unit" : "V" },
                  
                  {"vt":"ba", "type": "WE current"          , "unit" : "A"},
                  
                  {"vt":"ca", "type": "Phase"               , "unit" : "Degrees"},
                  {"vt":"cb", "type": "Impedance"           , "unit" : "Ohm"},
                  {"vt":"cc", "type": "ZReal"               , "unit" : "Ohm"},
                  {"vt":"cd", "type": "ZImag"               , "unit" : "Ohm"},
                  
                  {"vt":"da", "type": "Applied potential"   , "unit" : "V"},
                  {"vt":"db", "type": "Applied current"     , "unit" : "A"},
                  {"vt":"dc", "type": "Applied frequency"   , "unit" : "Hz"},
                  {"vt":"dd", "type": "Applied AC amplitude", "unit" : "Vrms"},
                  
                  {"vt":"eb", "type": "Time"                , "unit" : "s"},
                  {"vt":"ec", "type": "Pin mask"            , "unit" : " "},
                  
                  {"vt":"ja", "type": "Misc. generic 1"     , "unit" : " " },
                  {"vt":"jb", "type": "Misc. generic 2"     , "unit" : " " },
                  {"vt":"jc", "type": "Misc. generic 3"     , "unit" : " " },
                  {"vt":"jd", "type": "Misc. generic 4"     , "unit" : " " }]


def GetLibVersion():
	return PSEsPicoLibVersion

#Set library printing verbosity, True = verbose, False = not verbose
def SetPrintVerbose(boolean):
    global print_verbose
    print_verbose = boolean

#Convert a measurement matrix obtained through GetValueMatrixWithVT, 
#GetValueMatrix, ParseResultFileWithVT or ParseResultFile to a CSV string
def MatrixToCSV(matrix, vts = []):
    try:
        csv = ''
        for icurve in range(len(matrix)):
            if(len(vts) > 0):
                #create column headers
                ncols = GetVarTypeCols(vts, icurve);
                for icol in range(ncols):
                    vt = GetVarType(vts, icol)
                    csv += GetVarTypeName(vt) + ' (' + GetVarTypeUnit(vt) + ');'
                csv = csv[:-1] + '\n'
            #Write column data
            for row in matrix[icurve]:
                for col in row:
                    csv += str(col) + ';'
                csv = csv[:-1]
                csv += '\n'
    except Exception as e1:
        print("error saving csv...: " + str(e1)) #print the exception
    
    return csv

#Get var type of the specified column and curve
#"vts" is a var type matrix obtained from ParseResultFileWithVT or GetValueMatrixWithVT
def GetVarType(vts, col, icurve = 0):
    return vts[icurve][col]

#Get a list of var types of the specified curve
#"vts" is a var type matrix obtained from ParseResultFileWithVT or GetValueMatrixWithVT
def GetVarTypeCols(vts, icurve = 0):
    return len(vts[icurve])

#Get the full name of a var type
def GetVarTypeName(vt):
    for i in range(len(ms_var_types)):
        if ms_var_types[i]["vt"] == vt:
            return ms_var_types[i]["type"]
    return "unknown"

#Get the unit of a var type
def GetVarTypeUnit(vt):
    for i in range(len(ms_var_types)):
        if ms_var_types[i]["vt"] == vt:
            return ms_var_types[i]["unit"]
    return "?"  

#Convert the integer value to floatng point using the SI Prefix factor
def ValConverter(value,sip):
    for i in range(len(sip_factor)):
        if sip_factor[i]["si"] == sip:
            return value * sip_factor[i]["factor"]
    return "NAN"

#Convert raw data string into a value matrix containing the measurement data
#Organization is: val_array[curves][rows][columns] where:
#curves: Sets of measurement packages separated by a end-of-curve terminator such as *, + or -
#rows: Measurement package lines, containing multiple data values
#columns: The separate data values within a measurement package
def GetValueMatrix(content):
    val_array=[[]]
    i=0
    j=0
    for resultLine in content:
        #check for possible end-of-curve characters
        if resultLine.startswith('*') or resultLine.startswith('+') or resultLine.startswith('-'):
            j = len(val_array) #step to next section of values
            i = 0
        else:
            #parse line into value array and value type array
            vals,vts = ParseResultsFromLine(resultLine)
            #Ignore lines that hold no actual data package
            if len(vals) > 0:
                #If we're here we've found data for this curve, so check that space in allocated for this curve
                #This way we don't allocate space for empty curves (for example when a loop has no data packages)
                if j >= len(val_array):
                    val_array.append([])
                #Add values to value array
                val_array[j].append(vals)
                if(print_verbose == True):
                    print(val_array[j][i])
                i = i + 1
    return val_array

#Convert raw data string into a value matrix containing the measurement data
#Organization is: val_array[curves][rows][columns] where:
#curves: Sets of measurement packages separated by a end-of-curve terminator such as *, + or -
#rows: Measurement package lines, containing multiple data values
#columns: The separate data values within a measurement package
#
#This function also returns the var type matrix found in each curve.
#The organization of this matrix is: vt_array[curves][column]
def GetValueMatrixWithVT(content):
    val_array=[]
    vt_array=[]
    i=0
    j=0
    for resultLine in content:
        #check for possible end-of-curve characters
        if resultLine.startswith('*') or resultLine.startswith('+') or resultLine.startswith('-'):
            j = len(val_array) #step to next section of values
            i = 0
        else:
            #parse line into value array and value type array
            vals,vts = ParseResultsFromLine(resultLine)
            #Ignore lines that hold no actual data package
            if len(vals) > 0:
                #If we're here we've found data for this curve, so check that space in allocated for this curve
                #This way we don't allocate space for empty curves (for example when a loop has no data packages)
                if j >= len(val_array):
                    val_array.append([])
                    vt_array.append([])
                    #Save first list of col VT's for each curve
                    vt_array[j] = vts
                
                #Add values to value array
                val_array[j].append(vals)
                if(print_verbose == True):
                    print(val_array[j][i])
                i = i + 1
    return val_array, vt_array

#Open a file and parse the results, see GetValueMatrix
def ParseResultFile(resultfile):
    with open(resultfile) as f:
        content = f.readlines()
    values = GetValueMatrix(content)
    return values

#Open a file and parse the results, see GetValueMatrixWithVT
def ParseResultFileWithVT(resultfile):
    with open(resultfile) as f:
        content = f.readlines()
    values, vts = GetValueMatrixWithVT(content)
    return values, vts

#Get the amount of curves in a matrix obtained by GetValueMatrixWithVT, 
#GetValueMatrix, ParseResultFileWithVT or ParseResultFile
def GetCurveCount(matrix):
    return len(matrix)

#Get the specified curve from a matrix obtained by GetValueMatrixWithVT, 
#GetValueMatrix, ParseResultFileWithVT or ParseResultFile
def GetCurve(matrix, icurve):
    return matrix[icurve]

#Gets all column data for the specified column and curve from a value matrix obtained by 
#GetValueMatrixWithVT, GetValueMatrix, ParseResultFileWithVT or ParseResultFile
#If no curve is specified all column data from all curves is combined
def GetColumnFromMatrix(matrix, column, icurve = -1):
    if icurve == -1:
        value_list = []
        for icurve in range(len(matrix)):
            value_list += [row[column] for row in matrix[icurve]]
    else:
        value_list = [row[column] for row in matrix[icurve]]
    return np.asarray(value_list)

#Gets all row data for the specified row and curve from a value matrix obtained by 
#GetValueMatrixWithVT, GetValueMatrix, ParseResultFileWithVT or ParseResultFile
#If no curve is specified the row is returned from the first curve
def GetRowFromMatrix(matrix, row, icurve = 0):
    value_list = matrix[icurve][row]
    return np.asarray(value_list)

#Returns the first COM port found, ports can be excluded by specifying "exclude_port"
def FindComport(exclude_port):
    ports = serial.tools.list_ports.comports(include_links=False)   #Get the available ports
    myport = None
    for port in ports :
        if(print_verbose == True):
            print(port.device)
        if (port.device != exclude_port):               #Exclude port from the available ports
            myport = port.device                        #Set the port to the highest availabe port
    return myport   

#Opens a serial COM port
def OpenComport(ser,comport,timeout):
    ser.port = comport                                  #set the port 
    ser.baudrate = 230400                               #Baudrate is 230400 for EmstatPico
    ser.bytesize = serial.EIGHTBITS                     #number of bits per bytes
    ser.parity = serial.PARITY_NONE                     #set parity check: no parity
    ser.stopbits = serial.STOPBITS_ONE                  #number of stop bits
    #ser.timeout = None                                 #block read
    ser.timeout = timeout                               #timeout block read
    ser.xonxoff = False                                 #disable software flow control
    ser.rtscts = False                                  #disable hardware (RTS/CTS) flow control
    ser.dsrdtr = False                                  #disable hardware (DSR/DTR) flow control
    ser.writeTimeout = 2                                #timeout for write is 2 seconds
    try: 
        ser.open()                                      #open the port
    except serial.SerialException as e:                 #catch exception
        print("error open serial port: " + str(e))      #print the exception
        return False
    return True

#Verify that an EmStat Pico responds on the current serial port
def IsConnected(ser):
    prev_timeout = ser.timeout                          #Get the current timeout to restore it later
    ser.timeout = 4                                     #Set the timeout to 2 seconds
    ser.write(bytes("t\n",  'ascii'))                   #write the command 
    response =  ser.read_until(bytes("*\n", 'ascii'))   #read until *\n
    response = str(response, 'ascii')                   #convert bytes to ascii string        
    start=response.find('esp')                          #check the presents of "esp" in the repsonse
    ser.timeout = prev_timeout                          #restore timeout
    if start == -1:                                     #return if string is found
        return False
    return True

#Flush the Pico parse buffer 
def Flush(ser):
    prev_timeout = ser.timeout                          #Get the current timeout to restore it later
    ser.timeout = 4                                     #Set the timeout to 2 seconds
    ser.write(bytes("\n",  'ascii'))                   	#write a linefeed to flush
    response =  ser.read_until(bytes("\n", 'ascii'))   	#read until \n to catch the response
    ser.timeout = prev_timeout                          #restore timeout

#Query the EmStat Pico firmware version
def GetVersion(ser):
    ser.write(bytes("t\n",  'ascii'))
    version = ser.read_until(bytes("*\n",  'ascii'))
    version = str(version,  'ascii')
    version = version.strip()
    version = version.replace('\n',' ')                 #version command can be multiple lines
    version = version[1:]                               #remove first character (echoed 't')
    #print("version=" + version)
    return version

#Loads the MethodSCRIPT stored in flash into RAM so that it can be executed later
def LoadMscriptFromFlash(ser):
    ser.write(bytes("Lmscr\n",  'ascii'))

#Loads the MethodSCRIPT stored in flash into RAM and execute it
def RunMscriptFromFlash(ser):
    ser.write(bytes("Lmscr\n",  'ascii'))        #load first
    ser.write(bytes("r\n",  'ascii'))           #run script

#Get the MethodSCRIPT version this EmStat Pico uses
def GetMscriptVersion(ser):
    ser.write(bytes("v\n",  'ascii'))
    mscript_version = str(ser.readline(),  'ascii')
    return mscript_version

#Get a register
def GetRegister(ser,reg):
    sCmd = "G" + "%02d" % (reg)   
    #print(sCmd)
    ser.write(bytes(sCmd+ "\n" ,  'ascii'))
    response = ser.readline()
    return str(response,  'ascii')

#Get the EmStat Pico serial nr
def GetSerial(ser):
    ser.write(bytes("i\n",  'ascii'))
    strResponse = str(ser.readline(),  'ascii')
    strResponse = strResponse[1:]                       #remove first character (echoed 'i')
    return strResponse

#Receive MethodSCRIPT output data
def GetResults(ser):
    datafile = ""
    while True:
        response = ser.readline()
        str_line = response.decode("ascii")
        if(print_verbose == True):
            print("read data: " + str(response))
        datafile = datafile + str_line
        if (str_line == '\n' or  '!' in str_line):                          #empty line means end of script
            break
    return datafile

#Parse variable part of MethodSCRIPT data package
def ParseVarString(varstr):
    SIP = varstr[7]                 #get SI Prefix 
    varstr = varstr[:7]             #strip SI prefix from value
    val = int(varstr,16)            #convert the hexdecimal number to an integer
    val = val - 2**27               #substract the offset binary part to make it a signed value
    return ValConverter(val,SIP)    #return the converted floating point value
        

#Parses the results from one result-line and returns a list of values
def ParseResultsFromLine(res_line):
    lval= list()                            #Create a list for values
    lvt= list()                             #Create a list for values
    if res_line.startswith('P'):            #data point start with P
        pck = res_line[1:len(res_line)]     #ignore last and first character
        for v in pck.split(';'):            #value fields are seperated by a semicolon
            str_vt = v[0:2]                 #get the value-type 
            str_var = v[2:2+8]              #strip out value type
            val = ParseVarString(str_var)   #Parse the value
            lval.append(val)                #append value to the list
            lvt.append(str_vt)              
    return lval,lvt                         #return the list of values and list of value types


#Read specified file into a string
def ReadLinesFromFile(filename):
    with open(filename) as f:
        content = f.readlines()
    return content

#Send one line of a script
def SendScriptLine(ser,scriptline):
    if(print_verbose == True):
        print(scriptline.strip())
    ser.write(bytes(scriptline,  'ascii'))
    
#Send MethodSCRIPT to the EmStat Pico
def SendScriptFile(ser,scriptfile):
    with open(scriptfile) as f:
        content = f.readlines()
    if(print_verbose == True):
        print(len(content))
    for scriptline in content:
        SendScriptLine(ser,scriptline)
        
#Check that specified filename does not exist already, adds current date if file does exist
def CheckFileExistAndRename(filepathname):
    ResultFile = filepathname
    file_Exist=os.path.isfile(filepathname) 
    now = datetime.datetime.now()
    if file_Exist :
        (prefix, sep, suffix) = ResultFile.rpartition('.')
        prefix = prefix + "_"  + str(now.year) + "_" + str(now.month) + "_" + str(now.day) + "_" + str(now.hour) + "_" + str(now.minute) + "_" + str(now.second)
        ResultFile = prefix + sep + suffix
    return ResultFile
