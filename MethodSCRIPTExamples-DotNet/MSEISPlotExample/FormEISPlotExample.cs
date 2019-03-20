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
        const int PACKAGE_PARAM_VALUE_LENGTH = 8;                                                   //Length of the parameter value in a package
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
                                                                            { { "dc", "Frequency (Hz)" },
                                                                              { "cc", "Z' (Ohm)" },
                                                                              { "cd", "Z'' (Ohm)" } };

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
            NyquistPlotModel.Series.Add(NyquistPlotData);           //Add the data series to the plot model

            BodePlotDataMagnitude = GetLineSeries(OxyColors.Blue, MarkerType.Circle, " Z vs Frequency");
            BodePlotDataMagnitude.YAxisKey = "Z";                   //Set the secondary Y axis to Impedance magnitude (Z)
            BodePlotModel.Series.Add(BodePlotDataMagnitude);        //Add the data series to the plot model 

            BodePlotDataPhase = GetLineSeries(OxyColors.Red, MarkerType.Triangle, "Phase vs Frequency");
            BodePlotDataPhase.YAxisKey = "Phase";                   //Set the secondary Y axis to Phase
            BodePlotModel.Series.Add(BodePlotDataPhase);            //Add the data series to the plot model
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
                Smooth = true,
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
            var xAxisNyquistPlot = GetLinearAxis("Z (Ohm)", AxisPosition.Bottom);
            var yAxisNyquistPlot = GetLinearAxis("-Z'' (Ohm)", AxisPosition.Left);

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
        private LinearAxis GetLinearAxis(string title, AxisPosition position)
        {
            var linearAxis = new LinearAxis()
            {
                Position = position,
                MajorGridlineStyle = LineStyle.Dash,
                IsZoomEnabled = true,
                IsPanEnabled = true,
                Title = title
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
            var yAxisSecondaryBodePlot = GetLogAxis("Phase", "-Phase (deg)", AxisPosition.Right, OxyColors.Red);

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
        /// 
        private SerialPort OpenSerialPort()
        {
            SerialPort serialPort = null;
            string[] ports = SerialPort.GetPortNames();
            for (int i = 0; i < ports.Length; i++)
            {
                serialPort = GetSerialPort(ports[i]);                   //Fetches a new instance of the serial port with the port name
                try
                {
                    serialPort.Open();                                  //Opens serial port 
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
            parameters = responsePackageLine.Split(';');                     //The parameters are separated by the delimiter ';'
            foreach (string parameter in parameters)
            {
                paramIdentifier = parameter.Substring(0, 2);                 //The string (2 characters) that identifies the measurement parameter
                paramValue = parameter.Substring(2, PACKAGE_PARAM_VALUE_LENGTH);
                double paramValueWithPrefix = ParseParamValues(paramValue);
                switch (paramIdentifier)
                {
                    case "dc":                                               //Frequency reading
                        FrequencyValues.Add(paramValueWithPrefix);           //Adds the value to the FrequencyReadings list
                        break;
                    case "cc":                                               //Real Impedance reading
                        RealImpedanceValues.Add(paramValueWithPrefix);       //Adds the value to RealImpedanceReadings list
                        break;
                    case "cd":                                               //Imaginary Impedance reading
                        ImgImpedanceValues.Add(paramValueWithPrefix);        //Adds the value to ImgImpedanceReadings list
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
            char strUnitPrefix = paramValueString[7];                           //Identifies the SI unit prefix from the package at position 8
            string strvalue = paramValueString.Remove(7);                       //Retrieves the value of the measured parameter from the package
            int value = Convert.ToInt32(strvalue, 16);                          //Converts the hex value to int
            double paramValue = value - OFFSET_VALUE;                           //Values offset to receive only positive values
            return (paramValue * SI_Prefix_Factor[strUnitPrefix.ToString()]);   //Returns the value of the parameter after appending the SI unit prefix
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
            ProcessReceivedPackets();                       //Parses the received response packets
            btnMeasure.Enabled = true;
        }

        #endregion
    }
}
