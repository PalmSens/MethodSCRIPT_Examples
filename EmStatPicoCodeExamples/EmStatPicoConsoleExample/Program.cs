using System;
using System.Collections.Generic;
using System.Globalization;
using System.IO;
using System.IO.Ports;
using System.Reflection;
using System.Text;

namespace EmStatConsoleExample
{
    class Program
    {
        static string ScriptFileName = "meas_swv_test.txt";//"LSV_on_1KOhm.txt";                                           // Name of the script file
        static string AppLocation = Assembly.GetExecutingAssembly().Location;
        static string FilePath = System.IO.Path.GetDirectoryName(AppLocation) + "\\scripts";         // Location of the script file
        static string ScriptFilePath = Path.Combine(FilePath, ScriptFileName);

        const int PACKAGE_PARAM_VALUE_LENGTH = 8;                                                    // Length of the parameter value in a package
        const int OFFSET_VALUE = 0x8000000;                                                          // Offset value to receive positive values

        static SerialPort SerialPortEsP;
        static List<double> CurrentReadings = new List<double>();                                    // Collection of current readings
        static List<double> VoltageReadings = new List<double>();                                    // Collection of potential readings
        static string RawData;
        static int NDataPointsReceived = 0;                                                          // The number of data points received from the measurement

        readonly static Dictionary<string, double> SI_Prefix_Factor = new Dictionary<string, double> // The SI unit of the prefixes and their corresponding factors
                                                                   { { "a", 1e-18 },
                                                                     { "f", 1e-15 },
                                                                     { "p", 1e-12 },
                                                                     { "n", 1e-9 },
                                                                     { "u", 1e-6 },
                                                                     { "m", 1e-3 },
                                                                     { " ", 1 },
                                                                     { "K", 1e3 },
                                                                     { "M", 1e6 },
                                                                     { "G", 1e9 },
                                                                     { "T", 1e12 },
                                                                     { "P", 1e15 },
                                                                     { "E", 1e18 }};

        readonly static Dictionary<string, string> MeasurementParameters = new Dictionary<string, string>  // Measurement parameter identifiers and their corresponding labels
                                                                            { { "da", "E (V)" },
                                                                              { "ba", "i (A)" },
                                                                              { "dc", "Frequency (Hz)" },
                                                                              { "cc", "Z' (Ohm)" },
                                                                              { "cd", "Z'' (Ohm)" } };
        /// <summary>
        /// All possible current ranges, the current ranges
        /// that are supported by EmStat pico.
        /// </summary>
        public enum CurrentRanges
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
            hscr5mA = 137,
        }

        [Flags]
        public enum ReadingStatus
        {
            OK = 0x0,
            Overload = 0x2,
            Underload = 0x4,
            Overload_Warning = 0x8
        }

        static void Main(string[] args)
        {
            SerialPortEsP = OpenSerialPort();                   // Open and identify the port connected to ESPico
            if (SerialPortEsP != null && SerialPortEsP.IsOpen)
            {
                Console.WriteLine("\nConnected to EmStat Pico.\n");
                SendScriptFile();                               // Send the script file for LSV measurement
                ProcessReceivedPackages();                       // Parse the received response packages
                SerialPortEsP.Close();                          // Close the serial port
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
        /// Opens the serial ports and identifies the port connected to ESPico 
        /// </summary>
        /// <returns> The serial port connected to ESPico</returns>
        private static SerialPort OpenSerialPort()
        {
            SerialPort serialPort = null;
            string[] ports = SerialPort.GetPortNames();
            for (int i = 0; i < ports.Length; i++)
            {
                serialPort = GetSerialPort(ports[i]);
                try
                {
                    serialPort.Open();                  // Open serial port 
                    if (serialPort.IsOpen)
                    {
                        serialPort.Write("t\n");
                        string response = serialPort.ReadLine();
                        if (response.Contains("esp"))   // Identify the port connected to EmStatPico
                        {
                            serialPort.ReadTimeout = 7000;
                            return serialPort;
                        }
                    }
                }
                catch (Exception exception)
                {
                }
            }
            return serialPort;
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
            serialPort.BaudRate = 230400;
            serialPort.ReadTimeout = 1000;
            serialPort.WriteTimeout = 2;
            return serialPort;
        }

        /// <summary>
        /// Sends the script file to ESPico
        /// </summary>
        private static void SendScriptFile()
        {
            string line = "";
            using (StreamReader stream = new StreamReader(ScriptFilePath))
            {
                while (!stream.EndOfStream)
                {
                    line = stream.ReadLine();               // Read a line from the script file
                    line += "\n";                           // Append a new line character to the line read
                    SerialPortEsP.Write(line);              // Send the read line to ESPico
                }
                Console.WriteLine("Measurement started.");
            }
        }

        /// <summary>
        /// Processes the response packets from the ESPico and store the response in RawData.
        /// </summary>
        private static void ProcessReceivedPackages()
        {
            string readLine = "";
            Console.WriteLine("\nReceiving measurement response:");
            while (true)
            {
                readLine = ReadResponseLine();               // Read a line from the response
                RawData += readLine;                         // Add the response to raw data
                if (readLine == "\n")
                    break;
                else if (readLine.Contains("P"))
                {
                    NDataPointsReceived++;                  // Increment the number of data points if the read line contains the header char 'P
                    ParsePackageLine(readLine);             // Parse the line read 
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
                readChar = SerialPortEsP.ReadChar();        // Read a character from the serial port input buffer
                if (readChar > 0)                           // Possibility of time out exception if the operation doesn't complete within the read time out
                {
                    readLine += (char)readChar;             // Append the read character to readLine to form a response line
                    if ((char)readChar == '\n')
                    {
                        return readLine;                    // Return the readLine when a new line character is encountered
                    }
                }
            }
        }

        /// <summary>
        /// Parses a single line of the package and adds the values of the measurement parameters to the corresponding arrays
        /// </summary>
        /// <param name="responsePackageLine">The line from response package to be parsed</param>
        private static void ParsePackageLine(string packageLine)
        {
            string[] parameters;
            string paramIdentifier;
            string paramValue;
            int startingIndex = packageLine.IndexOf('P');
            string responsePackageLine = packageLine.Remove(startingIndex, 1);
            startingIndex = 0;
            Console.Write($"\nindex = " + String.Format("{0,3} {1,2} ", NDataPointsReceived, " "));
            parameters = responsePackageLine.Split(';');
            foreach (string parameter in parameters)
            {
                paramIdentifier = parameter.Substring(startingIndex, 2);   // The string that identifies the measurement parameter
                paramValue = responsePackageLine.Substring(startingIndex + 2, PACKAGE_PARAM_VALUE_LENGTH);
                double paramValueWithPrefix = ParseParamValues(paramValue);
                switch (paramIdentifier)
                {
                    case "da":                                            // Potential reading
                        VoltageReadings.Add(paramValueWithPrefix);        // If potential reading add the value to the VoltageReadings array
                        break;
                    case "ba":                                            // Current reading
                        CurrentReadings.Add(paramValueWithPrefix);        // If current reading add the value to the CurrentReadings array
                        break;
                }
                Console.Write("{0,4} = {1,10} {2,2}", MeasurementParameters[paramIdentifier], string.Format("{0:0.000E+00}", paramValueWithPrefix).ToString(), " ");
                ParseMetaDataValues(parameter.Substring(10));
            }
        }

        /// <summary>
        /// Parses the meta data values of the variables, if any.
        /// The first character in each meta data value specifies the type of data.
        /*  1 - 1 char hex mask holding the status (0 = OK, 2 = overload, 4 = underload, 8 = overload warning (80% of max))
         *  2 - 2 chars hex holding the current range index. First bit high (0x80) indicates a high speed mode cr.
         *  4 - 1 char hex holding the noise value */
        /// </summary>
        /// <param name="packageMetaData"></param>
        private static void ParseMetaDataValues(string packageMetaData)
        {
            string[] metaDataValues;
            metaDataValues = packageMetaData.Split(new string[] { "," }, StringSplitOptions.RemoveEmptyEntries);
            foreach (string metaData in metaDataValues)
            {
                switch (metaData[0])
                {
                    case '1':
                        GetReadingStatusFromPackage(metaData);
                        break;
                    case '2':
                        GetCurrentRangeFromPackage(metaData);
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
            long statusBits = (Convert.ToInt32(metaDatastatus[1].ToString(), 16));
            if ((statusBits & 0x0) == (long) ReadingStatus.OK)
                status = "OK";
            if ((statusBits & 0x2) == (long) ReadingStatus.Overload)
                status = "Overload";
            if ((statusBits & 0x4) == (long) ReadingStatus.Underload)
                status = "Underload";
            if ((statusBits & 0x8) == (long) ReadingStatus.Overload_Warning)
                status = "Overload warning";
            Console.Write(String.Format("Status : {0,-10} {1,2}", status, " "));
        }

        /// <summary>
        /// Parses the bytes corresponding to current range from the package and prints the current range value.
        /// </summary>
        /// <param name="metaDataCR"></param>
        private static void GetCurrentRangeFromPackage(string metaDataCR)
        {
            string currentRangeStr = "";
            byte crByte;
            if(byte.TryParse(metaDataCR.Substring(1,2), NumberStyles.AllowHexSpecifier, CultureInfo.InvariantCulture, out crByte))
            {
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
        }

        /// <summary>
        /// Parses the noise from the package.
        /// </summary>
        /// <param name="metaDataNoise"></param>
        private static void GetNoiseFromPackage(string metaDataNoise)
        {

        }

        /// <summary>
        /// Parses the measurement parameter values and appends the respective prefixes
        /// </summary>
        /// <param name="paramValueString"></param>
        /// <returns>The parameter value after appending the unit prefix</returns>
        private static double ParseParamValues(string paramValueString)
        {
            char strUnitPrefix = paramValueString[7];                         // Identify the SI unit prefix from the package at position 8
            string strvalue = paramValueString.Remove(7);                     // Strip the value of the measured parameter from the package
            int value = Convert.ToInt32(strvalue, 16);                        // Convert the hex value to int
            double paramValue = value - OFFSET_VALUE;                         // Values offset to receive only positive values
            return (paramValue * SI_Prefix_Factor[strUnitPrefix.ToString()]); // Return the value of the parameter after appending the SI unit prefix
        }
    }
}
