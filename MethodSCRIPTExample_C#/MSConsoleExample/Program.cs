/*
Copyright (c) 2019-2021 PalmSens BV
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

   - Redistributions of source code must retain the above copyright notice,
     this list of conditions and the following disclaimer.
   - Neither the name of PalmSens BV nor the names of its contributors
     may be used to endorse or promote products derived from this software
     without specific prior written permission.
   - This license does not release you from any requirement to obtain separate
     licenses from 3rd party patent holders to use this software.
   - Use of the software either in source or binary form must be connected to,
     run on or loaded to an PalmSens BV component.

DISCLAIMER: THIS SOFTWARE IS PROVIDED BY PALMSENS "AS IS" AND ANY EXPRESS OR
IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO
EVENT SHALL THE REGENTS AND CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA,
OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

using System;
using System.Collections.Generic;
using System.Globalization;
using System.IO;
using System.IO.Ports;
using System.Reflection;

namespace EmStatConsoleExample
{
    class Program
    {
        static string ScriptFileName = "LSV_on_10kOhm.txt";//"SWV_on_10kOhm.txt";                    //Name of the script file
        static string AppLocation = Assembly.GetExecutingAssembly().Location;
        static string FilePath = System.IO.Path.GetDirectoryName(AppLocation) + "\\scripts";         //Location of the script file
        static string ScriptFilePath = Path.Combine(FilePath, ScriptFileName);

        const string CMD_VERSION = "t\n";                                                            //Version command
        const int BAUD_RATE = 230400;                                                                //Baudrate for EmStat Pico
        // const int BAUD_RATE = 921600;                                                             //Baudrate for EmStat4 or Nexus
        const int DEFAULT_READ_TIME_OUT = 1000;                                                      //Default read time out when not connected to a device, in ms
        const int READ_TIME_OUT = 7000;                                                              //Read time out when connected, in ms
        const int PACKAGE_DATA_VALUE_LENGTH = 8;                                                     //Length of the data value in a package
        const int OFFSET_VALUE = 0x8000000;                                                          //Offset value to receive positive values

        static SerialPort SerialPortEsP;
        static DeviceType deviceType = DeviceType.UNKNOWN;
        static List<double> CurrentReadings = new List<double>();                                    //Collection of current readings
        static List<double> VoltageReadings = new List<double>();                                    //Collection of potential readings
        static string RawData;
        static int NDataPointsReceived = 0;                                                          //The number of data points received from the measurement

        /// <summary>
        /// The SI unit of the prefixes and their corresponding factors
        /// </summary>
        readonly static Dictionary<string, double> SI_Prefix_Factor = new Dictionary<string, double>
                                                          { { "a", 1e-18 },
                                                            { "f", 1e-15 },
                                                            { "p", 1e-12 },
                                                            { "n", 1e-9 },
                                                            { "u", 1e-6 },
                                                            { "m", 1e-3 },
                                                            { " ", 1.0 },
                                                            { "i", 1.0 },
                                                            { "k", 1e3 },
                                                            { "M", 1e6 },
                                                            { "G", 1e9 },
                                                            { "T", 1e12 },
                                                            { "P", 1e15 },
                                                            { "E", 1e18 }};

        /// <summary>
        /// Variable types and their corresponding labels
        /// </summary>
        readonly static Dictionary<string, string> MeasurementVariables = new Dictionary<string, string>
        {
            {"aa", " " },
            {"ab", "E (V)" }, //WE vs RE potential
            {"ac", "E CE (V)" }, //Versus GND
            {"ad", "E WE/SE (V)" }, //Versus GND
            {"ae", "E RE (V)" }, //Versus GND
            {"ag", "E WE/SE vs CE (V)" },

            {"as", "E AIN0 (V)" },
            {"at", "E AIN1 (V)" },
            {"au", "E AIN2 (V)" },


            {"ba", "i (A)" }, //WE current

            {"ca", "Phase (Degrees)" },
            {"cb", "Z (Ohm)" },
            {"cc", "Z' (Ohm)" },
            {"cd", "Z'' (Ohm)" },


            {"da", "E (V)" }, //Applied WE vs RE setpoint
            {"db", "i (A)" }, //Applied WE current setpoint
            {"dc", "Frequency (Hz)" }, //Applied frequency
            {"dd", "E AC (Vrms)" }, //Applied ac RMS amplitude


            {"eb", "Time (s)"},
            {"ec", "Pin mask"},

            {"ja", "Misc. generic 1" },
            {"jb", "Misc. generic 2" },
            {"jc", "Misc. generic 3" },

            {"jd", " " }
        };

        /// <summary>
        /// Types of metadata range
        /// </summary>
        private enum MetadataRangeType
        {
            UNKNOWN,
            CURRENT,
            POTENTIAL
        }

        /// <summary>
        /// Mapping of variable IDs to their metadata range type.
        /// </summary>
        readonly static Dictionary<string, MetadataRangeType> metaDataTypes = new Dictionary<string, MetadataRangeType>
        {
            { "aa", MetadataRangeType.UNKNOWN},
            { "ab", MetadataRangeType.POTENTIAL },
            { "ac", MetadataRangeType.POTENTIAL },
            { "ad", MetadataRangeType.POTENTIAL },
            { "ae", MetadataRangeType.POTENTIAL },
            { "af", MetadataRangeType.POTENTIAL },
            { "ag", MetadataRangeType.POTENTIAL },
            { "as", MetadataRangeType.POTENTIAL },
            { "at", MetadataRangeType.POTENTIAL },
            { "au", MetadataRangeType.POTENTIAL },
            { "av", MetadataRangeType.POTENTIAL },
            { "aw", MetadataRangeType.POTENTIAL },
            { "ax", MetadataRangeType.POTENTIAL },
            { "ay", MetadataRangeType.POTENTIAL },
            { "az", MetadataRangeType.POTENTIAL },
            { "ba", MetadataRangeType.CURRENT },
            { "ca", MetadataRangeType.UNKNOWN },
            { "cb", MetadataRangeType.UNKNOWN },
            { "cc", MetadataRangeType.CURRENT },
            { "cd", MetadataRangeType.POTENTIAL },
            { "ce", MetadataRangeType.POTENTIAL },
            { "cf", MetadataRangeType.CURRENT },
            { "cg", MetadataRangeType.UNKNOWN },
            { "ch", MetadataRangeType.UNKNOWN },
            { "ci", MetadataRangeType.POTENTIAL },
            { "cj", MetadataRangeType.UNKNOWN },
            { "ck", MetadataRangeType.CURRENT },
            { "da", MetadataRangeType.POTENTIAL },
            { "db", MetadataRangeType.CURRENT },
            { "dc", MetadataRangeType.UNKNOWN },
            { "dd", MetadataRangeType.UNKNOWN },
            { "ea", MetadataRangeType.UNKNOWN },
            { "eb", MetadataRangeType.UNKNOWN },
            { "ec", MetadataRangeType.UNKNOWN },
            { "ed", MetadataRangeType.UNKNOWN },
            { "ee", MetadataRangeType.UNKNOWN },
            { "ha", MetadataRangeType.CURRENT },
            { "hb", MetadataRangeType.CURRENT },
            { "hc", MetadataRangeType.CURRENT },
            { "hd", MetadataRangeType.CURRENT },
            { "ia", MetadataRangeType.POTENTIAL },
            { "ib", MetadataRangeType.POTENTIAL },
            { "ic", MetadataRangeType.POTENTIAL },
            { "id", MetadataRangeType.POTENTIAL },
            { "ja", MetadataRangeType.UNKNOWN },
            { "jb", MetadataRangeType.UNKNOWN },
            { "jc", MetadataRangeType.UNKNOWN },
            { "jd", MetadataRangeType.UNKNOWN },
        };

        /// <summary>
        /// Known devices
        /// </summary>
        private enum DeviceType
        {
            UNKNOWN,
            EMSTAT_PICO,
            EMSTAT4,
            NEXUS
        }

        /// <summary>
        /// Mapping of device types to human-readable names
        /// </summary>
        readonly static Dictionary<DeviceType, string> deviceNames = new Dictionary<DeviceType, string>
        {
            {DeviceType.UNKNOWN, "Unknown device"},
            {DeviceType.EMSTAT_PICO, "Emstat Pico"},
            {DeviceType.EMSTAT4, "Emstat4"},
            {DeviceType.NEXUS, "Nexus"}
        };

        /// <summary>
        /// All current ranges that are supported by EmStat pico.
        /// </summary>
        private static Dictionary<byte, string> CurrentRangesPico = new Dictionary<byte, string>
        {
            { 0, "100nA"},
            { 1, "2uA"},
            { 2, "4uA"},
            { 3, "8uA"},
            { 4, "16uA"},
            { 5, "32uA"},
            { 6, "63uA"},
            { 7, "125uA"},
            { 8, "250uA"},
            { 9, "500uA"},
            { 10, "1mA"},
            { 11, "15mA"},
            { 128, "100nA (High speed)"},
            { 129, "1uA (High speed)"},
            { 130, "6uA (High speed)"},
            { 131, "13uA (High speed)"},
            { 132, "25uA (High speed)"},
            { 133, "50uA (High speed)"},
            { 134, "100uA (High speed)"},
            { 135, "200uA (High speed)"},
            { 136, "1mA (High speed)"},
            { 137, "5mA (High speed)"},
        };

        /// <summary>
        /// All current ranges that are supported by EmStat4
        /// </summary>
        private static Dictionary<byte, string> CurrentRangesES4 = new Dictionary<byte, string>
        {
            // EmStat4 LR only:
            { 3, "1 nA" },
            { 6, "10 nA" },
            // EmStat4 LR/HR:
            { 9, "100 nA" },
            { 12, "1 uA" },
            { 15, "10 uA" },
            { 18, "100 uA" },
            { 21, "1 mA" },
            { 24, "10 mA" },
            // EmStat4 HR only:
            { 27, "100 mA" }
        };

        /// <summary>
        /// All potential ranges that are supported by EmStat4
        /// </summary>
        private static Dictionary<byte, string> PotentialRangesES4 = new Dictionary<byte, string>
        {
            { 2, "50 mV" },
            { 3, "100 mV" },
            { 4, "200 mV" },
            { 5, "500 mV" },
            { 6, "1 V" }
        };

        /// <summary>
        /// All current ranges that are supported by Nexus
        /// </summary>
        private static Dictionary<byte, string> CurrentRangesNexus = new Dictionary<byte, string>
        {
            // Potentiostat ranges
            { 16, "100 pA" },
            { 0 , "1 nA" },
            { 1 , "10 nA" },
            { 2 , "100 nA" },
            { 3 , "1 uA" },
            { 4 , "10 uA" },
            { 5 , "100 uA" },
            { 6 , "1 mA (tia)" },
            { 7 , "10 mA (tia)" },
            { 8 , "1 mA" },
            { 9 , "10 mA" },
            { 10, "100 mA" },
            { 11, "1 A" },
            // Galvanostat ranges
            { 32, "1 nA" },
            { 33, "10 nA" },
            { 34, "100 nA" },
            { 35, "1 uA" },
            { 36, "10 uA" },
            { 37, "100 uA" },
            { 38, "1 mA (tia)" },
            { 39, "10 mA (tia)" },
            { 40, "1 mA" },
            { 41, "10 mA" },
            { 42, "100 mA" },
            { 43, "1 A" }
        };

        /// <summary>
        /// All potential ranges that are supported by Nexus
        /// </summary>
        private static Dictionary<byte, string> PotentialRangesNexus = new Dictionary<byte, string>
        {
            { 0, "1 V" },
            { 1, "100 mV" },
            { 2, "10 mV" },
            { 3, "1 mV" },
        };

        [Flags]
        private enum ReadingStatus
        {
            OK = 0x0,
            Overload = 0x2,
            Underload = 0x4,
            Overload_Warning = 0x8
        }

        static void Main(string[] args)
        {
            (SerialPortEsP, deviceType) = OpenSerialPort();     //Opens and identifies the port, and identifies the device type
            if (SerialPortEsP != null && SerialPortEsP.IsOpen)
            {
                Console.WriteLine($"\nConnected to {deviceNames[deviceType]}\n");
                SendScriptFile();                               //Sends the script file for LSV measurement
                ProcessReceivedPackages();                      //Parses the received measurement response packages
                SerialPortEsP.Close();                          //Closes the serial port
            }
            else
            {
                Console.WriteLine($"Could not connect. \n");
            }
            Console.WriteLine("");
            Console.WriteLine("Press any key to exit.");
            Console.ReadKey();
        }

        /// <summary>
        /// Opens the serial ports and identifies the port connected a MethodSCRIPT-capable device.
        /// Also identifies the device type of the connected device.
        /// </summary>
        /// <returns> The serial port connected to a device, and the type of that device.
        /// If no MethodSCRIPT-capable device is found, or if it does not respond as expected,
        /// the return values are null and DeviceType.UNKNOWN.</returns>
        private static (SerialPort serialPort, DeviceType deviceType) OpenSerialPort()
        {
            string[] ports = SerialPort.GetPortNames();
            for (int i = 0; i < ports.Length; i++)
            {
                SerialPort serialPort = GetSerialPort(ports[i]);                   //Fetches a new instance of the serial port with the port name
                try
                {
                    serialPort.Open();                                  //Opens serial port 
                    if (serialPort.IsOpen)
                    {
                        serialPort.Write("\n"); //If any unfinished command was sent to the pico, this will clear it.
                        serialPort.Write(CMD_VERSION);                  //Writes the version command  

                        //Reads until "*\n" is found
                        string response = "";
                        while (!response.Contains("*\n"))
                        {
                            response += serialPort.ReadLine();    //Reads the response
                            response += "\n"; //ReadLine removes \n
                        }

                        serialPort.ReadTimeout = READ_TIME_OUT;
                        if (response.Contains("espico"))
                        {
                            return (serialPort, DeviceType.EMSTAT_PICO);
                        }
                        else if (response.Contains("es4_"))
                        {
                            return (serialPort, DeviceType.EMSTAT4);
                        }
                        else if (response.Contains("nexus1"))
                        {
                            return (serialPort, DeviceType.NEXUS);
                        }

                        //Not a valid device
                        serialPort.ReadTimeout = DEFAULT_READ_TIME_OUT; //Reset back to default
                        serialPort.Close();
                    }
                }
                catch (Exception)
                {
                    //Probably timeout or could not open serial port
                    if (serialPort.IsOpen)
                        serialPort.Close();
                }
            }

            //Not found
            return (null, DeviceType.UNKNOWN);
        }

        /// <summary>
        /// Fetches a new instance of the serial port with the port name passed to it
        /// </summary>
        /// <param name="port"></param>
        /// <returns></returns>
        private static SerialPort GetSerialPort(string port)
        {
            SerialPort serialPort = new SerialPort(port);
            serialPort.DataBits = 8;
            serialPort.Parity = Parity.None;
            serialPort.StopBits = StopBits.One;
            serialPort.BaudRate = BAUD_RATE;
            serialPort.ReadTimeout = DEFAULT_READ_TIME_OUT; //Initial time out. Upon connecting to device, time out set to READ_TIME_OUT
            serialPort.WriteTimeout = 200;
            return serialPort;
        }

        /// <summary>
        /// Sends the script file to EmStat Pico
        /// </summary>
        private static void SendScriptFile()
        {
            string line = "";
            using (StreamReader stream = new StreamReader(ScriptFilePath))
            {
                while (!stream.EndOfStream)
                {
                    line = stream.ReadLine();               //Reads a line from the script file
                    line += "\n";                           //Adds a new line character to the line read
                    SerialPortEsP.Write(line);              //Sends the read line to EmStat Pico
                }
                Console.WriteLine("Measurement started.");
            }
        }

        /// <summary>
        /// Processes the response packages from the EmStat Pico and stores the response in RawData.
        /// </summary>
        private static void ProcessReceivedPackages()
        {
            string readLine = "";
            Console.WriteLine("\nReceiving measurement response:");
            while (true)
            {
                readLine = ReadResponseLine();              //Reads a line from the measurement response
                RawData += readLine;                        //Adds the response to raw data
                if (readLine == "\n")
                    break;
                else if (readLine[0] == 'P')
                {
                    NDataPointsReceived++;                  //Increments the number of data points if the read line contains the header char 'P
                    ParsePackageLine(readLine);             //Parses the line read 
                }
            }
            Console.WriteLine("");
            Console.WriteLine($"\nMeasurement completed, {NDataPointsReceived} data points received.");
        }

        /// <summary>
        /// Reads characters and forms a line from the data received
        /// </summary>
        /// <returns>A line of response</returns>
        private static string ReadResponseLine()
        {
            string readLine = "";
            int readChar;
            while (true)
            {
                readChar = SerialPortEsP.ReadChar();        //Reads a character from the serial port input buffer
                if (readChar > 0)                           //Possibility of time out exception if the operation doesn't complete within the read time out; increment READ_TIME_OUT for measurements with long response times
                {
                    readLine += (char)readChar;             //Adds the read character to readLine to form a response line
                    if ((char)readChar == '\n')
                    {
                        return readLine;                    //Returns the readLine when a new line character is encountered
                    }
                }
            }
        }

        /// <summary>
        /// Parses a measurement data package and adds the parsed data values to their corresponding arrays
        /// </summary>
        /// <param name="responsePackageLine">The line from response package to be parsed</param>
        private static void ParsePackageLine(string packageLine)
        {
            string[] variables;
            string variableType;
            string dataValue;

            int startingIndex = packageLine.IndexOf('P');                        //Identifies the beginning of the package
            string responsePackageLine = packageLine.Remove(startingIndex, 1);   //Removes the beginning character 'P'

            Console.Write($"\nindex = " + String.Format("{0,3} {1,2} ", NDataPointsReceived, " "));
            variables = responsePackageLine.Split(';');                         //The data values are separated by the delimiter ';'

            foreach (string variable in variables)
            {
                variableType = variable.Substring(0, 2);                        //The string (2 characters) that identifies the variable type
                dataValue = variable.Substring(2, PACKAGE_DATA_VALUE_LENGTH);
                double dataValueWithPrefix = ParseParamValues(dataValue);       //Parses the data value package and returns the actual values with their corresponding SI unit prefixes 
                switch (variableType)
                {
                    case "da":                                                  //Potential reading
                        VoltageReadings.Add(dataValueWithPrefix);               //Adds the value to the VoltageReadings array
                        break;
                    case "ba":                                                  //Current reading
                        CurrentReadings.Add(dataValueWithPrefix);               //Adds the value to the CurrentReadings array
                        break;
                }
                Console.Write("{0,4} = {1,10} {2,2}", MeasurementVariables[variableType], string.Format("{0:0.000E+00}", dataValueWithPrefix).ToString(), " ");
                if (variable.Substring(10).StartsWith(","))
                    ParseMetaDataValues(variableType, variable.Substring(10));               //Parses the metadata values in the variable, if any
            }
        }

        /// <summary>
        /// Parses the metadata values of the variable, if any.
        /// The first character in each meta data value specifies the type of data.
        /// 1 - 1 char hex mask holding the status (0 = OK, 2 = overload, 4 = underload, 8 = overload warning (80% of max))
        /// 2 - 2 chars hex holding the current range index. First bit high (0x80) indicates a high speed mode cr.
        /// 4 - 1 char hex holding the noise value
        /// </summary>
        /// <param name="variableType">The type of variable ("da", "ba", etc.) this metadata is attached to.</param>
        /// <param name="packageMetaData">The metadata value itself, as described above.</param>
        private static void ParseMetaDataValues(string variableType, string packageMetaData)
        {
            string[] metaDataValues;
            metaDataValues = packageMetaData.Split(new string[] { "," }, StringSplitOptions.RemoveEmptyEntries);          //The metadata values are separated by the delimiter ','
            byte crByte;
            foreach (string metaData in metaDataValues)
            {
                switch (metaData[0])
                {
                    case '1':
                        GetReadingStatusFromPackage(metaData);
                        break;
                    case '2':
                        crByte = GetCurrentRangeFromPackage(metaData);
                        if (crByte != 0)
                        {
                            DisplayCR(variableType, crByte);
                        }
                        break;
                    case '4':
                        GetNoiseFromPackage(metaData);
                        break;
                }
            }
        }

        /// <summary>
        /// Parses the reading status from the package. 1 char hex mask holding the status (0 = OK, 2 = overload, 4 = underload, 8 = overload warning (80% of max)) 
        /// </summary>
        /// <param name="metaDatastatus"></param>
        private static void GetReadingStatusFromPackage(string metaDatastatus)
        {
            string status = "";
            int statusBits = (Convert.ToInt32(metaDatastatus[1].ToString(), 16));          //One char of the metadata value corresponding to status is retrieved
            if ((statusBits) == (long)ReadingStatus.OK)
                status = nameof(ReadingStatus.OK);
            if ((statusBits & 0x2) == (long)ReadingStatus.Overload)
                status = nameof(ReadingStatus.Overload);
            if ((statusBits & 0x4) == (long)ReadingStatus.Underload)
                status = nameof(ReadingStatus.Underload);
            if ((statusBits & 0x8) == (long)ReadingStatus.Overload_Warning)
                status = nameof(ReadingStatus.Overload_Warning);
            Console.Write(String.Format("Status : {0,-10} {1,2}", status, " "));
        }

        /// <summary>
        /// Parses the bytes corresponding to current range from the package.
        /// </summary>
        /// <param name="metaDataCR"></param>
        /// <returns>The cr byte after parsing</returns>
        private static byte GetCurrentRangeFromPackage(string metaDataCR)
        {
            byte crByte;
            if (byte.TryParse(metaDataCR.Substring(1, 2), NumberStyles.AllowHexSpecifier, CultureInfo.InvariantCulture, out crByte)) //Two characters of the metadata value corresponding to current range are retrieved as byte
            {
                return crByte;
            }
            return 0;
        }

        /// <summary>
        /// Displays the string corresponding to the input cr byte
        /// </summary>
        /// <param name="variableType">The type of variable ("da", "ba", etc.) this metadata was attached to. Used to determine the correct unit.</param>
        /// <param name="crByte">The crByte value whose string is to be obtained</param>
        private static void DisplayCR(string variableType, byte crByte)
        {
            string currentRangeStr = "";
            switch (deviceType)
            {
                case DeviceType.EMSTAT_PICO:
                    currentRangeStr = CurrentRangesPico[crByte];
                    break;
                case DeviceType.EMSTAT4:
                    switch (metaDataTypes[variableType])
                    {
                        case MetadataRangeType.UNKNOWN:
                            currentRangeStr = "UNKNOWN";
                            break;
                        case MetadataRangeType.CURRENT:
                            currentRangeStr = CurrentRangesES4[crByte];
                            break;
                        case MetadataRangeType.POTENTIAL:
                            currentRangeStr = PotentialRangesES4[crByte];
                            break;
                    }
                    break;
                case DeviceType.NEXUS:
                    switch (metaDataTypes[variableType])
                    {
                        case MetadataRangeType.UNKNOWN:
                            currentRangeStr = "UNKNOWN";
                            break;
                        case MetadataRangeType.CURRENT:
                            currentRangeStr = CurrentRangesNexus[crByte];
                            break;
                        case MetadataRangeType.POTENTIAL:
                            currentRangeStr = PotentialRangesNexus[crByte];
                            break;
                    }
                    break;
                case DeviceType.UNKNOWN:
                    currentRangeStr = "UNKNOWN";
                    break;
            }
            Console.Write(String.Format("CR : {0,-5} {1,2}", currentRangeStr, " "));
        }


        /// <summary>
        /// Parses the noise from the package.
        /// </summary>
        /// <param name="metaDataNoise"></param>
        private static void GetNoiseFromPackage(string metaDataNoise)
        {

        }

        /// <summary>
        /// Parses the data value package and appends the respective SI unit prefixes
        /// </summary>
        /// <param name="paramValueString">The data value package to be parsed</param>
        /// <returns>The actual data value after appending the unit prefix</returns>
        private static double ParseParamValues(string paramValueString)
        {
            if (paramValueString == "     nan")
                return double.NaN;

            char strUnitPrefix = paramValueString[7];                         //Identifies the SI unit prefix from the package at position 8
            string strvalue = paramValueString.Remove(7);                     //Retrieves the value of the variable the package
            int value = Convert.ToInt32(strvalue, 16);                        //Converts the hex value to int
            double paramValue = value - OFFSET_VALUE;                         //Values offset to receive only positive values
            return (paramValue * SI_Prefix_Factor[strUnitPrefix.ToString()]); //Returns the actual data value after appending the SI unit prefix
        }
    }
}
