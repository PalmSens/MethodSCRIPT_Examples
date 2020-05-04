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
        const int READ_TIME_OUT = 7000;                                                              //Read time out for the device in ms
        const int PACKAGE_DATA_VALUE_LENGTH = 8;                                                     //Length of the data value in a package
        const int OFFSET_VALUE = 0x8000000;                                                          //Offset value to receive positive values

        static SerialPort SerialPortEsP;
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
        /// All possible current ranges, the current ranges
        /// that are supported by EmStat pico.
        /// </summary>
        private enum CurrentRanges
        {
            cr100nA = 0,
            cr2uA = 1,
            cr4uA = 2,
            cr8uA = 3,
            cr16uA = 4,
            cr32uA = 5,
            cr63uA = 6,
            cr125uA = 7,
            cr250uA = 8,
            cr500uA = 9,
            cr1mA = 10,
            cr5mA = 11,
            hscr100nA = 128,
            hscr1uA = 129,
            hscr6uA = 130,
            hscr13uA = 131,
            hscr25uA = 132,
            hscr50uA = 133,
            hscr100uA = 134,
            hscr200uA = 135,
            hscr1mA = 136,
            hscr5mA = 137
        }

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
            SerialPortEsP = OpenSerialPort();                   //Opens and identifies the port connected to EmStat Pico
            if (SerialPortEsP != null && SerialPortEsP.IsOpen)
            {
                Console.WriteLine("\nConnected to EmStat Pico.\n");
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
        /// Opens the serial ports and identifies the port connected to EmStat Pico 
        /// </summary>
        /// <returns> The serial port connected to EmStat Pico</returns>
        private static SerialPort OpenSerialPort()
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

                        if (response.Contains("espico"))
                        {
                            serialPort.ReadTimeout = READ_TIME_OUT; //Sets the read time out for the device
                            return serialPort;
                        }

                        //Not a valid device
                        serialPort.Close();
                    }
                }
                catch (Exception exception)
                {
                    //Probably timeout or could not open serial port
                    if (serialPort.IsOpen)
                        serialPort.Close();
                }
            }

            //Not found
            return null;
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
            serialPort.ReadTimeout = 1000;                  //Initial time out set to 1000ms, upon connecting to EmStat Pico, time out reset to READ_TIME_OUT
            serialPort.WriteTimeout = 2;
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
                readLine = ReadResponseLine();               //Reads a line from the measurement response
                RawData += readLine;                         //Adds the response to raw data
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
                    readLine += (char)readChar;             //Appends the read character to readLine to form a response line
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
            string variableIdentifier;
            string dataValue;  

            int startingIndex = packageLine.IndexOf('P');                        //Identifies the beginning of the package
            string responsePackageLine = packageLine.Remove(startingIndex, 1);   //Removes the beginning character 'P'

            Console.Write($"\nindex = " + String.Format("{0,3} {1,2} ", NDataPointsReceived, " "));
            variables = responsePackageLine.Split(';');                         //The data values are separated by the delimiter ';'

            foreach (string variable in variables)
            {
                variableIdentifier = variable.Substring(0, 2);                 //The string (2 characters) that identifies the variable type
                dataValue = variable.Substring(2, PACKAGE_DATA_VALUE_LENGTH);
                double dataValueWithPrefix = ParseParamValues(dataValue);      //Parses the data value package and returns the actual values with their corresponding SI unit prefixes 
                switch (variableIdentifier)
                {
                    case "da":                                                  //Potential reading
                        VoltageReadings.Add(dataValueWithPrefix);              //Adds the value to the VoltageReadings array
                        break;
                    case "ba":                                                  //Current reading
                        CurrentReadings.Add(dataValueWithPrefix);              //Adds the value to the CurrentReadings array
                        break;
                }
                Console.Write("{0,4} = {1,10} {2,2}", MeasurementVariables[variableIdentifier], string.Format("{0:0.000E+00}", dataValueWithPrefix).ToString(), " ");
                if(variable.Substring(10).StartsWith(","))                 
                    ParseMetaDataValues(variable.Substring(10));               //Parses the metadata values in the variable, if any
            }
        }

        /// <summary>
        /// Parses the metadata values of the variable, if any.
        /// The first character in each meta data value specifies the type of data.
        /*  1 - 1 char hex mask holding the status (0 = OK, 2 = overload, 4 = underload, 8 = overload warning (80% of max))
         *  2 - 2 chars hex holding the current range index. First bit high (0x80) indicates a high speed mode cr.
         *  4 - 1 char hex holding the noise value */
        /// </summary>
        /// <param name="packageMetaData"></param>
        private static void ParseMetaDataValues(string packageMetaData)
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
                        if(crByte != 0) DisplayCR(crByte);
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
        /// <param name="crByte">The crByte value whose string is to be obtained</param>
        private static void DisplayCR(byte crByte)
        {
            string currentRangeStr = "";
            switch (crByte)
            {
                case (byte)CurrentRanges.cr100nA:
                    currentRangeStr = "100nA";
                    break;
                case (byte)CurrentRanges.cr2uA:
                    currentRangeStr = "2uA";
                    break;
                case (byte)CurrentRanges.cr4uA:
                    currentRangeStr = "4uA";
                    break;
                case (byte)CurrentRanges.cr8uA:
                    currentRangeStr = "8uA";
                    break;
                case (byte)CurrentRanges.cr16uA:
                    currentRangeStr = "16uA";
                    break;
                case (byte)CurrentRanges.cr32uA:
                    currentRangeStr = "32uA";
                    break;
                case (byte)CurrentRanges.cr63uA:
                    currentRangeStr = "63uA";
                    break;
                case (byte)CurrentRanges.cr125uA:
                    currentRangeStr = "125uA";
                    break;
                case (byte)CurrentRanges.cr250uA:
                    currentRangeStr = "250uA";
                    break;
                case (byte)CurrentRanges.cr500uA:
                    currentRangeStr = "500uA";
                    break;
                case (byte)CurrentRanges.cr1mA:
                    currentRangeStr = "1mA";
                    break;
                case (byte)CurrentRanges.cr5mA:
                    currentRangeStr = "15mA";
                    break;
                case (byte)CurrentRanges.hscr100nA:
                    currentRangeStr = "100nA (High speed)";
                    break;
                case (byte)CurrentRanges.hscr1uA:
                    currentRangeStr = "1uA (High speed)";
                    break;
                case (byte)CurrentRanges.hscr6uA:
                    currentRangeStr = "6uA (High speed)";
                    break;
                case (byte)CurrentRanges.hscr13uA:
                    currentRangeStr = "13uA (High speed)";
                    break;
                case (byte)CurrentRanges.hscr25uA:
                    currentRangeStr = "25uA (High speed)";
                    break;
                case (byte)CurrentRanges.hscr50uA:
                    currentRangeStr = "50uA (High speed)";
                    break;
                case (byte)CurrentRanges.hscr100uA:
                    currentRangeStr = "100uA (High speed)";
                    break;
                case (byte)CurrentRanges.hscr200uA:
                    currentRangeStr = "200uA (High speed)";
                    break;
                case (byte)CurrentRanges.hscr1mA:
                    currentRangeStr = "1mA (High speed)";
                    break;
                case (byte)CurrentRanges.hscr5mA:
                    currentRangeStr = "5mA (High speed)";
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
            char strUnitPrefix = paramValueString[7];                         //Identifies the SI unit prefix from the package at position 8
            string strvalue = paramValueString.Remove(7);                     //Retrieves the value of the variable the package
            int value = Convert.ToInt32(strvalue, 16);                        //Converts the hex value to int
            double paramValue = value - OFFSET_VALUE;                         //Values offset to receive only positive values
            return (paramValue * SI_Prefix_Factor[strUnitPrefix.ToString()]); //Returns the actual data value after appending the SI unit prefix
        }
    }
}
