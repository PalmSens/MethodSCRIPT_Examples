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
        static string ScriptFileName = "LSV_on_10kOhm.txt";                                            
        static string AppLocation = Assembly.GetExecutingAssembly().Location;
        static string FilePath = System.IO.Path.GetDirectoryName(AppLocation) + "\\scripts";         //Location of the script file
        static string ScriptFilePath = Path.Combine(FilePath, ScriptFileName);

        const string CMD_VERSION = "t\n";                                                            //Version command
        const int BAUD_RATE = 230400;                                                                //Baudrate for EmStat Pico
        const int READ_TIME_OUT = 7000;                                                              //Read time out for the device in ms
        const int PACKAGE_DATA_VALUE_LENGTH = 8;                                                    //Length of the data value in a package
        const int OFFSET_VALUE = 0x8000000;                                                          //Offset value to receive positive values

        private SerialPort SerialPortEsP;
        private List<double> CurrentReadings = new List<double>();                                   //Collection of current readings
        private List<double> VoltageReadings = new List<double>();                                   //Collection of potential readings
        private string RawData;
        private int NDataPointsReceived = 0;                                                         //The number of data points received from the measurement
        private PlotModel plotModel = new PlotModel();
        private LineSeries plotData;

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
            hscr5mA = 137,
        }

        [Flags]
        private enum ReadingStatus
        {
            OK = 0x0,
            Overload = 0x2,
            Underload = 0x4,
            Overload_Warning = 0x8
        }

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
        private SerialPort GetSerialPort(string port)
        {
            SerialPort serialPort = new SerialPort(port);
            serialPort.DataBits = 8;
            serialPort.Parity = Parity.None;
            serialPort.StopBits = StopBits.One;
            serialPort.BaudRate = BAUD_RATE;
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
                    SerialPortEsP.Write(line);              //Sends the read line to EmStat Pico
                }
                lbConsole.Items.Add("Measurement started.");
            }
        }

        /// <summary>
        /// Processes the response packages from the EmStat Pico and stores the response in RawData.
        /// Possibilities of time out exception in case the script file was empty 
        /// </summary>
        private void ProcessReceivedPackets()
        {
            string readLine = "";
            lbConsole.Items.Add("Receiving measurement response:");
            while (true)
            {
                readLine = ReadResponseLine();              //Reads a line from the measurement response
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
        /// Parses a measurement data package and adds the parsed data values to their corresponding arrays
        /// </summary>
        /// <param name="responsePackageLine">The measurement data package to be parsed</param>
        private void ParsePackageLine(string packageLine)
        {
            string[] variables;
            string variableIdentifier;
            string dataValue;

            int startingIndex = packageLine.IndexOf('P');                       //Identifies the beginning of the package
            string responsePackageLine = packageLine.Remove(startingIndex, 1);  //Removes the beginning character 'P'

            variables = responsePackageLine.Split(';');                         //The data values are separated by the delimiter ';'
            foreach (string parameter in variables)
            {
                variableIdentifier = parameter.Substring(0, 2);                 //The string (2 characters) that identifies the variable type
                dataValue = parameter.Substring(2, PACKAGE_DATA_VALUE_LENGTH);
                double dataValueWithPrefix = ParseParamValues(dataValue);       //Parses the data value package and returns the actual values with their corresponding SI unit prefixes 
                switch (variableIdentifier)
                {
                    case "da":                                                 //Potential reading
                        VoltageReadings.Add(dataValueWithPrefix);              //Adds the value to the VoltageReadings array
                        break;
                    case "ba":                                                 //Current reading
                        CurrentReadings.Add(dataValueWithPrefix * 1e6);       //Adds the value in uA to the CurrentReadings array
                        break;
                }
            }
        }

        /// <summary>
        /// Parses the data value package and appends the respective prefixes
        /// </summary>
        /// <param name="paramValueString">The data value package to be parsed</param>
        /// <returns>The actual data value after appending the unit prefix</returns>
        private double ParseParamValues(string paramValueString)
        {
            if (paramValueString == "     nan")
                return double.NaN;

            char strUnitPrefix = paramValueString[7];                         //Identifies the SI unit prefix from the package at position 8
            string strvalue = paramValueString.Remove(7);                     //Retrieves the value of the variable the package
            int value = Convert.ToInt32(strvalue, 16);                        //Converts the hex value to int
            double paramValue = value - OFFSET_VALUE;                         //Values offset to receive only positive values
            return (paramValue * SI_Prefix_Factor[strUnitPrefix.ToString()]); //Returns the actual data value after appending the SI unit prefix
        }

        /// <summary>
        /// Update the plot with the measurement response values.
        /// </summary>
        private void UpdatePlot()
        {
            plotData.Points.Add(new DataPoint(VoltageReadings.Last(), CurrentReadings.Last())); //Adds the last added measurement values as new data points and updates the plot
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
            ProcessReceivedPackets();                       //Parses the received measurement packages
            btnMeasure.Enabled = true;
        }

        #endregion
    }
}
