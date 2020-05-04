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
using System.Numerics;
using System.Drawing;
using OxyPlot.WindowsForms;

namespace EmStatPicoEISPlotExample
{
    public partial class frmEISPlotExample : Form
    {
        static string ScriptFileName = "EIS_on_Randles_560Ohm_10kOhm_33nF.txt";                     //Name of the script file
        static string AppLocation = Assembly.GetExecutingAssembly().Location;
        static string FilePath = System.IO.Path.GetDirectoryName(AppLocation) + "\\scripts";        //Location of the script file
        static string ScriptFilePath = Path.Combine(FilePath, ScriptFileName);

        const string CMD_VERSION = "t\n";                                                           //Version command
        const int BAUD_RATE = 230400;                                                               //Baudrate for EmStat Pico
        const int READ_TIME_OUT = 7000;                                                             //Read time out for the device in ms
        const int PACKAGE_DATA_VALUE_LENGTH = 8;                                                    //Length of the data value in a package
        const int OFFSET_VALUE = 0x8000000;                                                         //Offset value to receive positive values

        private SerialPort SerialPortEsP;
        private List<double> FrequencyValues = new List<double>();                                  //Collection of Frequency values
        private List<double> RealImpedanceValues = new List<double>();                              //Collection of Real Impedance values
        private List<double> ImgImpedanceValues = new List<double>();                               //Collection of Imaginary Impedance values
        private List<Complex> ComplexImpedanceValues = new List<Complex>();                         //Collection of Complex Impedance values
        private List<double> ImpedanceMagnitudeValues = new List<double>();                         //Collection of Impedance mangnitude values
        private List<double> PhaseValues = new List<double>();                                      //Collection of Phase values

        private string RawData;
        private int NDataPointsReceived = 0;                                                        //The number of data points received from the measurement
        private PlotModel NyquistPlotModel;
        private PlotModel BodePlotModel;
        private LineSeries NyquistPlotData;
        private LineSeries BodePlotDataMagnitude;
        private LineSeries BodePlotDataPhase;

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

        public frmEISPlotExample()
        {
            InitializeComponent();
            InitPlot();
        }

        /// <summary>
        /// Initializes the plots with plot titles, plot data (Line Series) and sets the axes 
        /// </summary>
        private void InitPlot()
        {
            NyquistPlotModel = new PlotModel();
            SetPlot(NyquistPlotModel, "Z(Im) vs Z(Re)");              //Set up the Nyquist plot

            BodePlotModel = new PlotModel();
            SetPlot(BodePlotModel, "Log Z/Phase vs Log f");          //Set up the Bode plot

            InitializePlotData();                                    //Initializes the plot data for Nyquist and Bode plots

            nyquistPlotView.Model = NyquistPlotModel;
            bodePlotView.Model = BodePlotModel;
        }

        /// <summary>
        /// Sets the plot type, title and axes
        /// </summary>
        /// <param name="plotModel"></param>
        /// <param name="title"></param>
        private void SetPlot(PlotModel plotModel, string title)
        {
            plotModel.PlotType = plotModel.Equals(NyquistPlotModel) ? PlotType.Cartesian : PlotType.XY;
            plotModel.Title = title;
            plotModel.IsLegendVisible = true;
            plotModel.TitleFontSize = 14;
            SetAxes(plotModel);                                     //Sets the plot axes
        }

        /// <summary>
        /// Initializes the plot data for Nyquist and Bode plots
        /// </summary>
        private void InitializePlotData()
        {
            NyquistPlotData = GetLineSeries(OxyColors.Green, MarkerType.Circle, "Z'' vs Z");
            NyquistPlotModel.Series.Add(NyquistPlotData);           //Adds the data series to the plot model

            BodePlotDataMagnitude = GetLineSeries(OxyColors.Blue, MarkerType.Circle, " Z vs Frequency");
            BodePlotDataMagnitude.YAxisKey = "Z";                   //Sets the secondary Y axis to Impedance magnitude (Z)
            BodePlotModel.Series.Add(BodePlotDataMagnitude);        //Adds the data series to the plot model 

            BodePlotDataPhase = GetLineSeries(OxyColors.Red, MarkerType.Triangle, "Phase vs Frequency");
            BodePlotDataPhase.YAxisKey = "Phase";                   //Sets the secondary Y axis to Phase
            BodePlotModel.Series.Add(BodePlotDataPhase);            //Adds the data series to the plot model
        }

        /// <summary>
        /// Returns a new line series
        /// </summary>
        /// <param name="color"></param>
        /// <param name="markerType"></param>
        /// <param name="title"></param>
        /// <returns> A new line series</returns>
        private LineSeries GetLineSeries(OxyColor color, MarkerType markerType, string title)
        {
            LineSeries lineSeries = new LineSeries()
            {
                Color = color,
                MarkerType = markerType,
                MarkerSize = 5,
                MarkerStroke = OxyColors.White,
                MarkerFill = color,
                MarkerStrokeThickness = 1.5,
                Title = title
            };
            return lineSeries;
        }

        /// <summary>
        /// Sets the x-axis and y-axis for the plots with corresponding axis labels 
        /// </summary>
        private void SetAxes(PlotModel plotModel)
        {
            if (plotModel.Equals(NyquistPlotModel))
            {
                SetAxesNyquistPlot();
                ChangePanMouseButton(nyquistPlotView);
            }
            else if (plotModel.Equals(BodePlotModel))
            {
                SetAxesBodePlot();
                ChangePanMouseButton(bodePlotView);
            }
        }

        /// <summary>
        /// Sets the axes for Nyquist plot with grid lines and labels
        /// </summary>
        private void SetAxesNyquistPlot()
        {
            var xAxisNyquistPlot = GetLinearAxis("ZReal", "Z' (Ohm)", AxisPosition.Bottom, OxyColors.Black);
            var yAxisNyquistPlot = GetLinearAxis("ZImag", "-Z'' (Ohm)", AxisPosition.Left, OxyColors.Black);

            //Add the axes to the Nyquist plot model
            NyquistPlotModel.Axes.Add(xAxisNyquistPlot);
            NyquistPlotModel.Axes.Add(yAxisNyquistPlot);
        }

        /// <summary>
        /// Gets a new instance of linear axis at the specified position with given title
        /// </summary>
        /// <param name="title">The axis label</param>
        /// <param name="position">The axis position</param>
        /// <returns></returns>
        private LinearAxis GetLinearAxis(string key, string title, AxisPosition position, OxyColor color)
        {
            var linearAxis = new LinearAxis()
            {
                MajorGridlineStyle = LineStyle.Dash,
                Key = key,
                Position = position,
                IsZoomEnabled = true,
                IsPanEnabled = true,
                Title = title,
                TitleColor = color
            };
            return linearAxis;
        }

        /// <summary>
        /// Sets the axes for Bode plot with grid lines and labels
        /// </summary>
        private void SetAxesBodePlot()
        {
            var xAxisBodePlot = GetLogAxis("Frequency", "Frequency (Hz)", AxisPosition.Bottom, OxyColors.Black);
            var yAxisBodePlot = GetLogAxis("Z", "Z (Ohm)", AxisPosition.Left, OxyColors.Blue);
            var yAxisSecondaryBodePlot = GetLinearAxis("Phase", "-Phase (deg)", AxisPosition.Right, OxyColors.Red);

            //Add the axes to the Bode plot model
            BodePlotModel.Axes.Add(xAxisBodePlot);
            BodePlotModel.Axes.Add(yAxisBodePlot);
            BodePlotModel.Axes.Add(yAxisSecondaryBodePlot);
        }

        /// <summary>
        /// Changes the bind mouse button for panning from default right to left.
        /// </summary>
        /// <param name="plotView"></param>
        private void ChangePanMouseButton(PlotView plotView)
        {
            plotView.Controller = new PlotController();
            plotView.Controller.UnbindMouseDown(OxyMouseButton.Right);
            plotView.Controller.BindMouseDown(OxyMouseButton.Left, PlotCommands.PanAt);
        }

        /// <summary>
        /// Gets a new instance of logarathmic axis at the given position with the given key and title
        /// </summary>
        /// <param name="key">The key to identify the axis</param>
        /// <param name="title">The axis title/label </param>
        /// <param name="position">The position of the axis</param>
        /// <returns></returns>
        private LogarithmicAxis GetLogAxis(string key, string title, AxisPosition position, OxyColor color)
        {
            var logAxis = new LogarithmicAxis()
            {
                MajorGridlineStyle = LineStyle.Dash,
                Key = key,
                Position = position,
                IsZoomEnabled = true,
                IsPanEnabled = true,
                Title = title,
                TitleColor = color
            };
            return logAxis;
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
                    CalculateComplexImpedance();
                    UpdatePlots();                         //Updates the plot with the new data point added after parsing a line
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
        /// <param name="responsePackageLine">The line from response package to be parsed</param>
        private void ParsePackageLine(string packageLine)
        {
            string[] variables;
            string variableIdentifier;
            string dataValue;

            int startingIndex = packageLine.IndexOf('P');                       //Identifies the beginning of the package
            string responsePackageLine = packageLine.Remove(startingIndex, 1);  //Removes the beginning character 'P'

            variables = responsePackageLine.Split(';');                         //The data values are separated by the delimiter ';'
            foreach (string variable in variables)
            {
                variableIdentifier = variable.Substring(0, 2);                 //The string (2 characters) that identifies the variable type
                dataValue = variable.Substring(2, PACKAGE_DATA_VALUE_LENGTH);
                double dataValueWithPrefix = ParseParamValues(dataValue);      //Parses the data value package and returns the actual values with their corresponding SI unit prefixes 
                switch (variableIdentifier)
                {
                    case "dc":                                                  //Frequency reading
                        FrequencyValues.Add(dataValueWithPrefix);              //Adds the value to the FrequencyReadings list
                        break;
                    case "cc":                                                  //Real Impedance reading
                        RealImpedanceValues.Add(dataValueWithPrefix);          //Adds the value to RealImpedanceReadings list
                        break;
                    case "cd":                                                  //Imaginary Impedance reading
                        ImgImpedanceValues.Add(dataValueWithPrefix);           //Adds the value to ImgImpedanceReadings list
                        break;
                }
            }
        }

        /// <summary>
        /// Parses the data value package and appends the respective SI unit prefixes
        /// </summary>
        /// <param name="paramValueString">The data value package to be parsed</param>
        /// <returns>The actual data after appending the unit prefix</returns>
        private double ParseParamValues(string paramValueString)
        {
            char strUnitPrefix = paramValueString[7];                         //Identifies the SI unit prefix from the package at position 8
            string strvalue = paramValueString.Remove(7);                     //Retrieves the value of the variable the package
            int value = Convert.ToInt32(strvalue, 16);                        //Converts the hex value to int
            double paramValue = value - OFFSET_VALUE;                         //Values offset to receive only positive values
            return (paramValue * SI_Prefix_Factor[strUnitPrefix.ToString()]); //Returns the actual data value after appending the SI unit prefix
        }

        /// <summary>
        /// Calculates the complex impedance, magnitude and phase of the impedance values 
        /// </summary>
        private void CalculateComplexImpedance()
        {
            Complex ZComplex = new Complex(RealImpedanceValues.Last(), ImgImpedanceValues.Last());
            ComplexImpedanceValues.Add(ZComplex);
            ImpedanceMagnitudeValues.Add(ComplexImpedanceValues.Last().Magnitude);
            PhaseValues.Add(ComplexImpedanceValues.Last().Phase * 180 / Math.PI);
        }

        /// <summary>
        /// Update the plots with the measurement values.
        /// </summary>
        private void UpdatePlots()
        {
            NyquistPlotData.Points.Add(new DataPoint(RealImpedanceValues.Last(), -ImgImpedanceValues.Last()));           //Adds the last added measurement values as new data points
            NyquistPlotModel.InvalidatePlot(true);                                                                       //Updates the plot
            NyquistPlotModel.ResetAllAxes();

            BodePlotDataMagnitude.Points.Add(new DataPoint(FrequencyValues.Last(), ImpedanceMagnitudeValues.Last()));    //Adds new  data points
            BodePlotDataPhase.Points.Add(new DataPoint(FrequencyValues.Last(), -PhaseValues.Last()));                    //Adds new  data points
            BodePlotModel.InvalidatePlot(true);                                                                          //Updates the plot
            BodePlotModel.ResetAllAxes();
        }

        ///// <summary>
        ///// Clears the plots before every measurement
        ///// </summary>
        private void ClearPlots()
        {
            NyquistPlotData.Points.Clear();
            NyquistPlotModel.InvalidatePlot(true);
            BodePlotDataMagnitude.Points.Clear();
            BodePlotDataPhase.Points.Clear();
            BodePlotModel.InvalidatePlot(true);
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
                lbConsole.Items.Add($"Connected to EmStat Pico.");
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
            SerialPortEsP.Close();                                  //Closes the serial port
            lbConsole.Items.Add($"Disconnected from EmStat Pico.");
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
            ClearPlots();                                    //Clears the plot to begin a new measurement
            NDataPointsReceived = 0;
            SendScriptFile();                               //Sends the script file for EIS measurement
            ProcessReceivedPackets();                       //Parses the received measurement packages
            btnMeasure.Enabled = true;
        }

        #endregion
    }
}
