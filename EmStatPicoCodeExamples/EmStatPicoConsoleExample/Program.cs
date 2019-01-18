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
        static string ScriptFileName = "LSV_test_script.txt";                                        // Name of the script file
        static string AppLocation = Assembly.GetExecutingAssembly().Location;
        static string FilePath = System.IO.Path.GetDirectoryName(AppLocation) + "\\scripts";         // Location of the script file
        static string ScriptFilePath = Path.Combine(FilePath, ScriptFileName);

        const int PACKAGE_PARAM_VALUE_LENGTH = 9;                                                    // Length of the parameter value in a package
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
                                                                            { { "aa", "E (V)" },
                                                                              { "ba", "i (A)" },
                                                                              { "dc", "Frequency (Hz)" },
                                                                              { "cc", "Z' (Ohm)" },
                                                                              { "cd", "Z'' (Ohm)" } };

        static void Main(string[] args)
        {
            SerialPortEsP = OpenSerialPort();                   // Open and identify the port connected to ESPico
            if (SerialPortEsP != null && SerialPortEsP.IsOpen)
            {
                Console.WriteLine("\nConnected to EmStat Pico.\n");
                SendScriptFile();                               // Send the script file for LSV measurement
                ProcessReceivedPackets();                       // Parse the received response packets
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
                    serialPort.Open();                  //Open serial port 
                    if (serialPort.IsOpen)
                    {
                        serialPort.Write("t\n");
                        string response = serialPort.ReadLine();
                        if (response.Contains("esp"))   //Identify the port connected to EmStatPico
                        {
                            serialPort.ReadTimeout = 7000;
                            return serialPort;
                        }
                    }
                }
                catch (Exception exception)
                {
                    Console.WriteLine(exception);
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
        private static void ProcessReceivedPackets()
        {
            string readLine = "";
            Console.WriteLine("\nReceiving measurement response:");
            while (true)
            {
                readLine = ReadResponseLine();              // Read a line from the response
                RawData += readLine;                        // Add the response to raw data
                if (readLine == "\n")
                    break;
                NDataPointsReceived++;                      // Increment the number of data points if the read line is not empty
                ParsePackageLine(readLine);                 // Parse the line read 
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
        private static void ParsePackageLine(string responsePackageLine)
        {
            string paramIdentifier;
            string paramValue;
            int startingIndex = responsePackageLine.IndexOf('P');
            int currentIndex = startingIndex + 1;
            Console.Write($"\nindex = " + String.Format("{0,3} {1,5} ", NDataPointsReceived, " "));
            while (!(responsePackageLine.Substring(currentIndex) == "\n"))
            {
                paramIdentifier = responsePackageLine.Substring(currentIndex, 2);                           // The string that identifies the measurement parameter
                paramValue = responsePackageLine.Substring(currentIndex + 2, PACKAGE_PARAM_VALUE_LENGTH);   // The value of the measurement parameter
                double paramValueWithPrefix = ParseParamValues(paramValue);                                 // Append the SI prefix to the value
                Console.Write("{0,5} "=" {1,10} {2,4}", MeasurementParameters[paramIdentifier], string.Format("{0:0.000E+00}", paramValueWithPrefix).ToString(), " ");
                switch(paramIdentifier)
                {
                    case "aa":                                          // Potential reading
                        VoltageReadings.Add(paramValueWithPrefix);      // If potential reading add the value to the VoltageReadings array
                        break;
                    case "ba":                                          // Current reading
                        CurrentReadings.Add(paramValueWithPrefix);      // If current reading add the value to the CurrentReadings array
                        break;
                }
                currentIndex = currentIndex + 11;
            }
        }

        /// <summary>
        /// Parses the measurement parameter values and appends the respective prefixes
        /// </summary>
        /// <param name="paramValueString"></param>
        /// <returns>The parameter value after appending the unit prefix</returns>
        private static double ParseParamValues(string paramValueString)
        {
            char strUnitPrefix = paramValueString[7];                         //Identify the SI unit prefix from the package at position 8
            string strvalue = paramValueString.Remove(7);                     //Strip the value of the measured parameter from the package
            int value = Convert.ToInt32(strvalue, 16);                        // Convert the hex value to int
            double paramValue = value - OFFSET_VALUE;                         //Values offset to receive only positive values
            return (paramValue * SI_Prefix_Factor[strUnitPrefix.ToString()]); //Return the value of the parameter after appending the SI unit prefix
        }
    }
}
