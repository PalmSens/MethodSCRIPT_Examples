using OxyPlot;
using OxyPlot.Axes;
using OxyPlot.Series;
using System;
using System.Collections.Generic;
using System.IO;
using System.IO.Ports;
using System.Linq;
using System.Reflection;
using System.Windows.Forms;

namespace EmStatPicoPlotExample
{
    public partial class frmPlotExample : Form
    {
        static string ScriptFileName = "LSV_on_10KOhm.txt";                                            
        static string AppLocation = Assembly.GetExecutingAssembly().Location;
        static string FilePath = System.IO.Path.GetDirectoryName(AppLocation) + "\\scripts";         //Location of the script file
        static string ScriptFilePath = Path.Combine(FilePath, ScriptFileName);

        const string CMD_VERSION = "t\n";                                                            //Version command
        const int BAUD_RATE = 230400;                                                                //Baudrate for EmStat Pico
        const int READ_TIME_OUT = 7000;                                                              //Read time out for the device in ms
        const int PACKAGE_PARAM_VALUE_LENGTH = 8;                                                    //Length of the parameter value in a package
        const int OFFSET_VALUE = 0x8000000;                                                          //Offset value to receive positive values

        private SerialPort SerialPortEsP;
        private List<double> CurrentReadings = new List<double>();                                   //Collection of current readings
        private List<double> VoltageReadings = new List<double>();                                   //Collection of potential readings
        private string RawData;
        private int NDataPointsReceived = 0;                                                         //The number of data points received from the measurement
        private PlotModel plotModel = new PlotModel();
        private LineSeries plotData;

        readonly static Dictionary<string, double> SI_Prefix_Factor = new Dictionary<string, double> //The SI unit of the prefixes and their corresponding factors
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

        readonly static Dictionary<string, string> MeasurementParameters = new Dictionary<string, string>  //Measurement parameter identifiers and their corresponding labels
                                                                            { { "da", "E (V)" },
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
            SetAxes();                                 //Sets the plot axes
            plotModel.Series.Add(plotData);            //Adds the data series to the plot model
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

        /// <summary>
        /// Opens the serial ports and identifies the port connected to EmStat Pico 
        /// </summary>
        /// <returns> The serial port connected to EmStat Pico</returns>
        private SerialPort OpenSerialPort()
        {
            SerialPort serialPort = null;
            string[] ports = SerialPort.GetPortNames();
            for (int i = 0; i < ports.Length; i++)
            {
                serialPort = GetSerialPort(ports[i]);    //Fetches a new instance of the serial port with the port name
                try
                {
                    serialPort.Open();                   //Opens serial port 
                    if (serialPort.IsOpen)
                    {
                        serialPort.Write(CMD_VERSION);                  //Writes the version command             
                        while (true)
                        {
                            string response = serialPort.ReadLine();    //Reads the response
                            response += "\n";
                            if (response.Contains("espico"))            //Verifies if the device connected is EmStat Pico
                                serialPort.ReadTimeout = READ_TIME_OUT; //Sets the read time out for the device
                            if (response.Contains("*\n"))
                                return serialPort;                      //Reads until "*\n" is found and breaks
                        }
                    }
                }
                catch (Exception exception)
                {
                    serialPort.Close();
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
            serialPort.ReadTimeout = 1000;                              //Initial time out set to 1000ms, upon connecting to EmStat Pico, time out reset to READ_TIME_OUT
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
        /// Sends the script file to EmStat Pico
        /// </summary>
        private void SendScriptFile()
        {
            string line = "";
            using (StreamReader stream = new StreamReader(ScriptFilePath))
            {
                while (!stream.EndOfStream)
                {
                    line = stream.ReadLine();               //Reads a line from the script file
                    line += "\n";                           //Appends a new line character to the line read
                    SerialPortEsP.Write(line);              //Sends the response line to EmStat Pico
                }
                lbConsole.Items.Add("Measurement started.");
            }
        }

        /// <summary>
        /// Processes the response packets from the EmStat Pico and store the response in RawData.
        /// Possibilities of time out exception in case the script file was empty 
        /// </summary>
        private void ProcessReceivedPackets()
        {
            string readLine = "";
            lbConsole.Items.Add("Receiving measurement response:");
            while (true)
            {
                readLine = ReadResponseLine();              //Reads a line from the response
                RawData += readLine;                        //Adds the response to raw data
                if (readLine == "\n")
                    break;
                else if (readLine[0] == 'P')
                {
                    NDataPointsReceived++;                 //Increments the number of data points if the read line contains the header char 'P
                    ParsePackageLine(readLine);            //Parses the line read 
                    UpdatePlot();                          //Updates the plot with the new data point added after parsing a line
                }
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
        /// Parses a single line of the package and adds the values of the measurement parameters to the corresponding arrays
        /// </summary>
        /// <param name="responsePackageLine">The line from response package to be parsed</param>
        private void ParsePackageLine(string packageLine)
        {
            string[] parameters;
            string paramIdentifier;
            string paramValue;
            int startingIndex = packageLine.IndexOf('P');

            string responsePackageLine = packageLine.Remove(startingIndex, 1);
            parameters = responsePackageLine.Split(';');                       //The parameters are separated by the delimiter ';'
            foreach (string parameter in parameters)
            {
                paramIdentifier = parameter.Substring(0, 2);                   //The string (2 characters) that identifies the measurement parameter
                paramValue = parameter.Substring(2, PACKAGE_PARAM_VALUE_LENGTH);
                double paramValueWithPrefix = ParseParamValues(paramValue);
                switch (paramIdentifier)
                {
                    case "da":                                                 //Potential reading
                        VoltageReadings.Add(paramValueWithPrefix);             //Adds the value to the VoltageReadings array
                        break;
                    case "ba":                                                 //Current reading
                        CurrentReadings.Add(paramValueWithPrefix * 1e6);       //Adds the value in uA to the CurrentReadings array
                        break;
                }
            }
        }

        /// <summary>
        /// Parses the measurement parameter values and appends the respective prefixes
        /// </summary>
        /// <param name="paramValueString"></param>
        /// <returns>The parameter value after appending the unit prefix</returns>
        private double ParseParamValues(string paramValueString)
        {
            char strUnitPrefix = paramValueString[7];                         //Identifies the SI unit prefix from the package at position 8
            string strvalue = paramValueString.Remove(7);                     //Retrieves the value of the measured parameter from the package
            int value = Convert.ToInt32(strvalue, 16);                        //Converts the hex value to int
            double paramValue = value - OFFSET_VALUE;                         //Values offset to receive only positive values
            return (paramValue * SI_Prefix_Factor[strUnitPrefix.ToString()]); //Returns the value of the parameter after appending the SI unit prefix
        }

        /// <summary>
        /// Update the plot with the measurement response values.
        /// </summary>
        private void UpdatePlot()
        {
            plotData.Points.Add(new DataPoint(VoltageReadings.Last(), CurrentReadings.Last())); //Adds the last added measurement values as new data points and update the plot
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

        #region Events

        /// <summary>
        /// Connects to the EmStatPico if available.
        /// </summary>
        /// <param name="sender"></param>
        /// <param name="e"></param>
        private void btnConnect_Click(object sender, EventArgs e)
        {
            SerialPortEsP = OpenSerialPort();                       //Opens and identifies the port connected to EmStat Pico
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
        /// Disconnects the serial port connected to EmStat Pico.
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
        /// Sends a script file, parses the response and updates the plot.
        /// </summary>
        /// <param name="sender"></param>
        /// <param name="e"></param>
        private void btnMeasure_Click(object sender, EventArgs e)
        {
            btnMeasure.Enabled = false;
            ClearPlot();                                    //Clears the plot to begin a new measurement
            NDataPointsReceived = 0;
            SendScriptFile();                               //Sends the script file for LSV measurement
            ProcessReceivedPackets();                       //Parses the received response packets
            btnMeasure.Enabled = true;
        }

        #endregion
    }
}
