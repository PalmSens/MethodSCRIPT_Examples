using OxyPlot;
using OxyPlot.Axes;
using OxyPlot.Series;
using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.ComponentModel;
using System.Data;
using System.Drawing;
using System.IO;
using System.IO.Ports;
using System.Linq;
using System.Reflection;
using System.Text;
using System.Threading.Tasks;
using System.Windows.Forms;

namespace EmStatPicoPlotExample
{
    public partial class frmPlotExample : Form
    {
        static string ScriptFileName = "LSV_on_1KOhm.txt";               
        static string AppLocation = Assembly.GetExecutingAssembly().Location;
        static string FilePath = System.IO.Path.GetDirectoryName(AppLocation) + "\\scripts";        // Location of the script file
        static string ScriptFilePath = Path.Combine(FilePath, ScriptFileName);

        const int PACKAGE_PARAM_VALUE_LENGTH = 9;                                                   // Length of the parameter value in a package
        const int OFFSET_VALUE = 0x8000000;                                                         // Offset value to receive positive values

        private SerialPort SerialPortEsP;
        private List<double> CurrentReadings = new List<double>();                                  // Collection of current readings
        private List<double> VoltageReadings = new List<double>();                                  // Collection of potential readings
        private string RawData;                                                                     
        private int NDataPointsReceived = 0;                                                        // The number of data points received from the measurement
        private PlotModel plotModel = new PlotModel();
        private LineSeries plotData;

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
                                                                              { "ba", "I (A)" },
                                                                              { "dc", "Frequency" },
                                                                              { "cc", "Z'" },
                                                                              { "cd", "Z''" } };

        public frmPlotExample()
        {
            InitializeComponent();
            InitPlot();
        }

        /// <summary>
        /// Initializes the plot with plot title, plot data (Line Series) and sets the axes 
        /// </summary>
        private void InitPlot()
        {
            samplePlotView.Model = plotModel;
            plotModel.Title = "LSV: I vs E";
            plotModel.TitleFontSize = 14;
            plotData = new LineSeries()
            {
                Color = OxyColors.Green,
                MarkerType = MarkerType.Circle,
                MarkerSize = 6,
                MarkerStroke = OxyColors.White,
                MarkerFill = OxyColors.Green,
                MarkerStrokeThickness = 1.5,
            };
            SetAxes();                                 // Set the plot axes
            plotModel.Series.Add(plotData);            // Add the data series to the plot model
        }

        /// <summary>
        /// Sets the x-axis and y-axis for the plot with corresponding titles 
        /// </summary>
        private void SetAxes()
        {
            var xAxis = new LinearAxis()
            {
                Position = OxyPlot.Axes.AxisPosition.Bottom,
                MajorGridlineStyle = LineStyle.Dash,
                Title = "Potential (V)"
            };
            var yAxis = new OxyPlot.Axes.LinearAxis()
            {
                Position = OxyPlot.Axes.AxisPosition.Left,
                MajorGridlineStyle = LineStyle.Dash,
                Title = "Current (uA)"
            };
            //Set the x-axis and y-axis for the plot model
            plotModel.Axes.Add(xAxis);                 
            plotModel.Axes.Add(yAxis);
        }

        #region Events

        /// <summary>
        /// Connects to the EmStatPico if available.
        /// </summary>
        /// <param name="sender"></param>
        /// <param name="e"></param>
        private void btnConnect_Click(object sender, EventArgs e)
        {
            SerialPortEsP = OpenSerialPort();        // Open and identify the port connected to ESPico
            if (SerialPortEsP != null && SerialPortEsP.IsOpen)
            {
                lbConsole.Items.Add($"Connected to EmStat Pico");
                EnableButtons(true);
            }
            else
            {
                lbConsole.Items.Add($"Could not connect. \n");
            }
        }

        /// <summary>
        /// Disconnects the serial port connected to EsPico.
        /// </summary>
        /// <param name="sender"></param>
        /// <param name="e"></param>
        private void btnDisconnect_Click(object sender, EventArgs e)
        {
            SerialPortEsP.Close();
            lbConsole.Items.Add($"Disconnected from EmStat Pico");
            EnableButtons(false);
        }

        /// <summary>
        /// Opens the serial ports and identifies the port connected to ESPico 
        /// </summary>
        /// <returns> The serial port connected to ESPico</returns>
        /// 
        private SerialPort OpenSerialPort()
        {
            SerialPort serialPort = null;
            string[] ports = SerialPort.GetPortNames();
            for (int i = 0; i < ports.Length; i++)
            {
                serialPort = GetSerialPort(ports[i]);    // Fetch a new instance of the serial port with the port name
                try
                {
                    serialPort.Open();                   // Open serial port 
                    if (serialPort.IsOpen)
                    {
                        serialPort.Write("t\n");
                        string response = serialPort.ReadLine();
                        if (response.Contains("esp"))   // Identify the port connected to EmStatPico using "esp" in the version string
                        {
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
        private SerialPort GetSerialPort(string port)
        {
            SerialPort serialPort = new SerialPort(port);
            serialPort.DataBits = 8;
            serialPort.Parity = Parity.None;
            serialPort.StopBits = StopBits.One;
            serialPort.BaudRate = 230400;
            serialPort.ReadTimeout = 7000;
            serialPort.WriteTimeout = 2;
            return serialPort;
        }

        /// <summary>
        /// Enable/Disable buttons upon connecting and disconnecting.
        /// </summary>
        /// <param name="isConnected"></param>
        private void EnableButtons(bool isConnected)
        {
            btnConnect.Enabled = !isConnected;
            btnDisconnect.Enabled = isConnected;
            btnMeasure.Enabled = isConnected;
        }

        /// <summary>
        /// Sends a script file, parses the response and updates the plot.
        /// </summary>
        /// <param name="sender"></param>
        /// <param name="e"></param>
        private void btnMeasure_Click(object sender, EventArgs e)
        {
            btnMeasure.Enabled = false;
            ClearPlot();                                    // Clear the plot to begin a new measurement
            NDataPointsReceived = 0;
            SendScriptFile();                               // Send the script file for LSV measurement
            ProcessReceivedPackets();                       // Parse the received response packets
            btnMeasure.Enabled = true;
        }

        /// <summary>
        /// Sends the script file to ESPico
        /// </summary>
        private void SendScriptFile()
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
                lbConsole.Items.Add("Measurement started.");
            }
        }

        /// <summary>
        /// Processes the response packets from the ESPico and store the response in RawData.
        /// Possibilities of time out exception in case the script file was empty 
        /// </summary>
        private void ProcessReceivedPackets()
        {
            string readLine = "";
            lbConsole.Items.Add("Receiving measurement response:");
            while (true)
            {
                readLine = ReadResponseLine();              // Read a line from the response
                RawData += readLine;                        // Add the response to raw data
                if (readLine == "\n")                       
                    break;
                NDataPointsReceived++;                      // Increment the number of data points if the read line is not empty
                ParsePackageLine(readLine);                 // Parse the line read 
                UpdatePlot();                               // Update the plot with the new data point added after parsing a line
            }
            lbConsole.Items.Add($"Measurement completed.");
            lbConsole.Items.Add($"{NDataPointsReceived} data points received.");
        }

        /// <summary>
        /// Reads characters and forms a line from the data received
        /// </summary>
        /// <returns>A line of response</returns>
        private string ReadResponseLine()
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
        private void ParsePackageLine(string responsePackageLine)
        {
            string paramIdentifier;
            string paramValue;
            int startingIndex = responsePackageLine.IndexOf('P');
            int currentIndex = startingIndex + 1;
            while (!(responsePackageLine.Substring(currentIndex) == "\n"))
            {
                paramIdentifier = responsePackageLine.Substring(currentIndex, 2);                           // The string that identifies the measurement parameter
                paramValue = responsePackageLine.Substring(currentIndex + 2, PACKAGE_PARAM_VALUE_LENGTH);   // The value of the measurement parameter
                double paramValueWithPrefix = ParseParamValues(paramValue);                                 // Append the SI prefix to the value
                switch (paramIdentifier)
                {
                    case "aa":                                                 // Potential reading
                        VoltageReadings.Add(paramValueWithPrefix);             // If potential reading add the value to the VoltageReadings array
                        break;
                    case "ba":                                                 // Current reading
                        CurrentReadings.Add(paramValueWithPrefix * 1e6);       // If current reading add the value in uA to the CurrentReadings array
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
        private double ParseParamValues(string paramValueString)
        {
            char strUnitPrefix = paramValueString[7];                         // Identify the SI unit prefix from the package at position 8
            string strvalue = paramValueString.Remove(7);                     // Strip the value of the measured parameter from the package
            int value = Convert.ToInt32(strvalue, 16);                        // Convert the hex value to int
            double paramValue = value - OFFSET_VALUE;                         // Values offset to receive only positive values
            return (paramValue * SI_Prefix_Factor[strUnitPrefix.ToString()]); // Return the value of the parameter after appending the SI unit prefix
        }

        /// <summary>
        /// Update the plot with the measurement response values.
        /// </summary>
        private void UpdatePlot()
        {
            plotData.Points.Add(new DataPoint(VoltageReadings.Last(), CurrentReadings.Last())); // Add the last added measurement values as new data points and update the plot
            plotModel.InvalidatePlot(true);
        }

        /// <summary>
        /// Clears the plot before every measurement
        /// </summary>
        private void ClearPlot()
        {
            plotData.Points.Clear();
            plotModel.InvalidatePlot(true);
        }

        #endregion
    }
}
