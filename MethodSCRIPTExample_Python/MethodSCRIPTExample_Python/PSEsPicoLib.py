# -*- coding: ascii -*-
"""
/* ----------------------------------------------------------------------------
 *         PalmSens Method SCRIPT SDK V1.2
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
"""
import serial
import serial.tools.list_ports
import datetime
import os.path
import numpy as np
import time

PSEsPicoLibVersion = "1.2"

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

ms_value_types = [{"vt":"aa", "type": "unknown"             , "unit" : " " },
                  {"vt":"ab", "type": "RE potential"        , "unit" : "V" },
                  {"vt":"ac", "type": "CE potential"        , "unit" : "V" },
                  {"vt":"ad", "type": "WE potential"        , "unit" : "V" },
                  {"vt":"ba", "type": "WE current"          , "unit" : "A"},
                  {"vt":"da", "type": "Applied potential"   , "unit" : "V"},
                  {"vt":"dc", "type": "Applied frequency"   , "unit" : "Hz"},
                  {"vt":"jd", "type": "Misc. generic 4"     , "unit" : " " }]


def GetLibVersion():
	return PSEsPicoLibVersion

def GetVarType(vt):
    for i in range(len(ms_value_types)):
        if ms_value_types[i]["vt"] == vt:
            return ms_value_types[i]["type"]
    return "unknown"

def GetValueUnit(vt):
    for i in range(len(ms_value_types)):
        if ms_value_types[i]["vt"] == vt:
            return ms_value_types[i]["unit"]
    return "?"
    

#Convert the integer value to floatng point using the SI Prefix factor
def ValConverter(value,sip):
    for i in range(len(sip_factor)):
        if sip_factor[i]["si"] == sip:
            return value * sip_factor[i]["factor"]
    return "NAN"



def GetValueMatrix(content):
    val_array=[] 
    vt_array=[] 
    i=0
    for resultLine in content:
        vals,vts = ParseResultsFromLine(resultLine)
        if len(vals) > 0:
            val_array.append(vals)
            vt_array.append(vts)
            print(val_array[i])
            #print(vt_array[i])
            i = i +1
    return val_array

def ParseResultFile(resultfile):
    with open(resultfile) as f:
        content = f.readlines()
        #content = [x.strip() for x in content]
    values = GetValueMatrix(content)
    return values    


def GetColumnFromMatrix(matrix, column):
    value_list = [row[column] for row in matrix]
    return np.asarray(value_list)

def GetRowFromMatrix(matrix, row):
    value_list = matrix[row]
    return np.asarray(value_list)

def FindComport(exclude_port):
    ports = serial.tools.list_ports.comports(include_links=False)   #Get the available ports
    myport = None
    for port in ports :
        print(port.device)
        if (port.device != exclude_port):               #Exclude port from the available ports
            myport = port.device                        #Set the port to the highest availabe port
    return myport   

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

def GetVersion(ser):
    ser.write(bytes("t\n",  'ascii'))
    version = ser.read_until(bytes("*\n",  'ascii'))
    version = str(version,  'ascii')
    version = version.strip()
    version = version.replace('\n',' ')                 #version command can be multiple lines
    version = version[1:]                               #remove first character (echoed 't')
    #print("version=" + version)
    return version


def LoadMscriptFromFlash(ser):
    ser.write(bytes("Lmscr\n",  'ascii'))

def RunMscriptFromFlash(ser):
    ser.write(bytes("Lmscr\n",  'ascii'))        #load first
    ser.write(bytes("r\n",  'ascii'))           #run script

def GetMscriptVersion(ser):
    ser.write(bytes("v\n",  'ascii'))
    mscript_version = str(ser.readline(),  'ascii')
    return mscript_version

    
def GetRegister(ser,reg):
    sCmd = "G" + "%02d" % (reg)   
    #print(sCmd)
    ser.write(bytes(sCmd+ "\n" ,  'ascii'))
    response = ser.readline()
    return str(response,  'ascii')

def GetSerial(ser):
    ser.write(bytes("i\n",  'ascii'))
    strResponse = str(ser.readline(),  'ascii')
    strResponse = strResponse[1:]                       #remove first character (echoed 'i')
    return strResponse

def SetHybernatemode(ser):
    ser.write(bytes("s\n",  'ascii'))

    
def GetResults(ser):
    datafile = ""
    while True:
        response = ser.readline()
        str_line = response.decode("ascii")
        print("read data: " + str(response))
        datafile = datafile + str_line
        if (str_line == '\n'):                          #empty line means end of script
            break
    return datafile

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

 
def ReadLinesFromFile(filename):
    with open(filename) as f:
        content = f.readlines()
    return content


def SendScriptLine(ser,scriptline):
    print(scriptline.strip())
    ser.write(bytes(scriptline,  'ascii'))
    


def SendScriptFile(ser,scriptfile):
    with open(scriptfile) as f:
        content = f.readlines()
    print(len(content))
    for scriptline in content:
        SendScriptLine(ser,scriptline)
        
        
def CheckFileExistAndRename(filepathname):
    ResultFile = filepathname
    file_Exist=os.path.isfile(filepathname) 
    now = datetime.datetime.now()
    if file_Exist :
        (prefix, sep, suffix) = ResultFile.rpartition('.')
        prefix = prefix + "_"  + str(now.year) + "_" + str(now.month) + "_" + str(now.day) + "_" + str(now.hour) + "_" + str(now.minute) + "_" + str(now.second)
        ResultFile = prefix + '.dat'
    return ResultFile
