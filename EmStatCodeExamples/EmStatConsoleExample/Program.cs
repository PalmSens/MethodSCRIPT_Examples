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
        const int PACKAGE_PARAM_VALUE_LENGTH = 9;
        static SerialPort SerialPortEsP;
        static string ScriptFileName = "LSV_test_script.txt";
        static string AppLocation = Assembly.GetExecutingAssembly().Location;
        //static string ScriptFileName = "200k_500_15mV_cr5m.mscr";
        static string FilePath = System.IO.Path.GetDirectoryName(AppLocation) + "\\scripts";
        static string ScriptFilePath = Path.Combine(FilePath, ScriptFileName);
        const int OFFSET_VALUE = 0x8000000; // Offset value to receive positive values

        const string POTENTIAL_READING = "aa";
        const string CURRENT_READING = "ba";
        static List<double> CurrentReadings = new List<double>();
        static List<double> VoltageReadings = new List<double>();
        static string RawData;

        readonly static Dictionary<string, double> Prefix_Factor = new Dictionary<string, double> { { "a", 1e-18 }, { "f", 1e-15 }, { "p", 1e-12 }, { "n", 1e-9 }, { "u", 1e-6 }, { "m", 1e-3 },
                                                                                                      { " ", 1 }, { "K", 1e3 }, { "M", 1e6 }, { "G", 1e9 }, { "T", 1e12 }, { "P", 1e15 }, { "E", 1e18 }};

        readonly static Dictionary<string, string> MeasurementParameters = new Dictionary<string, string> { { "aa", "E" }, { "ba", "I" }, { "dc", "Frequency" }, { "cc", "Z'" }, { "cd", "Z''" } };

        static void Main(string[] args)
        {
            SerialPortEsP = OpenSerialPort();        // Open and identify the port connected to EsPico
            if (SerialPortEsP != null && SerialPortEsP.IsOpen)
            {
                Console.WriteLine("Connected");
                SendScriptFile();                    //Send the script file to EmStatPico
                Console.WriteLine("Measurement Parameters Received:\n");
                ProcessReceivedPackets();            //Parse the received the response packets
                DisplayRawData();
                SerialPortEsP.Close();               //Close the serial port
            }
            else
            {
                Console.WriteLine("Not connected");
            }
        }

        /// <summary>
        /// Opens the serial ports and identifies the port connected to EsPico 
        /// </summary>
        /// <returns> The serial port connected to EsPico</returns>
        /// 
        private static SerialPort OpenSerialPort()
        {
            SerialPort serialPort = null;
            string[] ports = SerialPort.GetPortNames();
            for (int i = 0; i < ports.Length; i++)
            {
                serialPort = new SerialPort(ports[i]);
                serialPort.DataBits = 8;
                serialPort.Parity = Parity.None;
                serialPort.StopBits = StopBits.One;
                serialPort.BaudRate = 230400;
                serialPort.ReadTimeout = 7000;
                serialPort.WriteTimeout = 2;
                try
                {
                    serialPort.Open();               //Open serial port 
                    if (serialPort.IsOpen)
                    {
                        serialPort.Write("t\n");
                        string response = serialPort.ReadLine();
                        if (response.Contains("esp")) //Identify the port connected to EmStatPico
                        {
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
        /// Sends the script file to EsPico
        /// </summary>
        private static void SendScriptFile()
        {
            using (StreamReader stream = new StreamReader(ScriptFilePath))
            {
                string line;
                while (!stream.EndOfStream)
                {
                    line = stream.ReadLine();
                    if (!(line == null))
                    {
                        Console.WriteLine(line);
                        line += "\n";
                        SerialPortEsP.Write(line);
                    }
                }
            }
        }

        /// <summary>
        /// Processes the response packets from the EsPico and store the response in RawData.
        /// </summary>
        private static void ProcessReceivedPackets()
        {
            string readLine = "";
            while (true)
            {
                readLine = ReadResponseLine();
                RawData += readLine;
                if (readLine == "\n")
                    break;
                ParsePackageLine(readLine);
            }
        }

        /// <summary>
        /// Reads a line from the data received
        /// </summary>
        /// <returns></returns>
        private static string ReadResponseLine()
        {
            string readLine = "";
            int readChar;
            while (true)
            {
                readChar = SerialPortEsP.ReadChar();
                if (readChar > 0)
                {
                    readLine += (char)readChar;
                    if ((char)readChar == '\n')
                    {
                        return readLine;
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
            while (!(responsePackageLine.Substring(currentIndex) == "\n"))
            {
                paramIdentifier = responsePackageLine.Substring(currentIndex, 2);
                paramValue = responsePackageLine.Substring(currentIndex + 2, PACKAGE_PARAM_VALUE_LENGTH);
                double paramValueWithPrefix = ParseParamValues(paramValue);
                Console.Write(MeasurementParameters[paramIdentifier] + " : " + paramValueWithPrefix.ToString() + " ");
                switch(paramIdentifier)
                {
                    case POTENTIAL_READING:
                        VoltageReadings.Add(paramValueWithPrefix);
                        break;
                    case CURRENT_READING:
                        CurrentReadings.Add(paramValueWithPrefix);
                        break;
                }
                currentIndex = currentIndex + 11;
            }
            Console.Write("\n");
        }

        /// <summary>
        /// Parses the measurement parameter values and adds the respective prefixes
        /// </summary>
        /// <param name="paramValueString"></param>
        /// <returns>The parameter value after appending the unit prefix</returns>
        private static double ParseParamValues(string paramValueString)
        {
            char strUnitPrefix = paramValueString[7];
            string strvalue = paramValueString.Remove(7);
            int value = Convert.ToInt32(strvalue, 16);         // Convert hex value to int
            double paramValue = value - OFFSET_VALUE;          //Values offset to receive only positive values
            return (paramValue * Prefix_Factor[strUnitPrefix.ToString()]);
        }

        /// <summary>
        /// Display the raw data on the console
        /// </summary>
        private static void DisplayRawData()
        {
            Console.Write("Raw Data: \n");
            using (StringReader reader = new StringReader(RawData))
            {
                string responseLine;
                while ((responseLine = reader.ReadLine()) != null)
                {
                    Console.Write(responseLine + "\n");
                }
            }
        }
    }
}
