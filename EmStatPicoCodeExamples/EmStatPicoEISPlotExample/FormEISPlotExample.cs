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

namespace EmStatPicoEISPlotExample
{
    public partial class frmEISPlotExample : Form
    {
        static string ScriptFileName = "EIS_test_script.txt";
        static string AppLocation = Assembly.GetExecutingAssembly().Location;
        static string FilePath = System.IO.Path.GetDirectoryName(AppLocation) + "\\scripts";        // Location of the script file
        static string ScriptFilePath = Path.Combine(FilePath, ScriptFileName);
        const int PACKAGE_PARAM_VALUE_LENGTH = 9;                                                   // Length of the parameter value in a package
        const int OFFSET_VALUE = 0x8000000;                                                         // Offset value to receive positive values

        private SerialPort SerialPortEsP;
        private List<double> FrequencyValues = new List<double>();                                  // Collection of Frequency values
        private List<double> RealImpedanceValues = new List<double>();                              // Collection of Real Impedance values
        private List<double> ImgImpedanceValues = new List<double>();                               // Collection of Imaginary Impedance values
        private List<Complex> ComplexImpedanceValues = new List<Complex>();                         // Collection of Complex Impedance values
        private List<double> ImpedanceMagnitudeValues = new List<double>();                         // Collection of Impedance mangnitude values
        private List<double> PhaseValues = new List<double>();                                      // Collection of Phase values

        private string RawData;
        private int NDataPointsReceived = 0;                                                        // The number of data points received from the measurement
        private PlotModel NyquistPlotModel = new PlotModel();                                       
        private PlotModel BodePlotModel = new PlotModel();
        private LineSeries NyquistPlotData;
        private LineSeries BodePlotDataMagnitude;
        private LineSeries BodePlotDataPhase;

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
                                                                              { "dc", "Frequency (Hz)" },
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
            NyquistPlotModel.Title = "Z(Re) vs Z(Im)";
            NyquistPlotModel.TitleFontSize = 14;
            BodePlotModel.Title = "Frequency vs Log Z/Phase";
            BodePlotModel.TitleFontSize = 14;

            nyquistPlotView.Model = NyquistPlotModel;
            bodePlotView.Model = BodePlotModel;

            SetAxes();                                               // Set the plot axes
            InitializePlotData();                                    // Initialize the plot data for Nyquist and Bode plots
            NyquistPlotModel.IsLegendVisible = true;
            BodePlotModel.IsLegendVisible = true;
        }

        /// <summary>
        /// Initializes the plot data for Nyquist and Bode plots
        /// </summary>
        private void InitializePlotData()
        {
            NyquistPlotData = GetLineSeries(OxyColors.Green, MarkerType.Circle, "Z vs Z''");
            NyquistPlotModel.Series.Add(NyquistPlotData);           // Add the data series to the plot model

            BodePlotDataMagnitude = GetLineSeries(OxyColors.Blue, MarkerType.Circle, " Frequency vs Z");
            BodePlotDataMagnitude.YAxisKey = "Z";                   // Set the secondary Y axis to Impedance magnitude (Z)
            BodePlotModel.Series.Add(BodePlotDataMagnitude);        // Add the data series to the plot model 

            BodePlotDataPhase = GetLineSeries(OxyColors.Red, MarkerType.Triangle, "Frequency vs Phase");
            BodePlotDataPhase.YAxisKey = "Phase";                   // Set the secondary Y axis to Phase
            BodePlotModel.Series.Add(BodePlotDataPhase);            // Add the data series to the plot model
        }

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
        /// Sets the x-axis and y-axis for the plot with corresponding axis labels 
        /// </summary>
        private void SetAxes()
        {
            SetAxesNyquistPlot();
            SetAxesBodePlot();
        }

        /// <summary>
        /// Sets the axes for Nyquist plot with grid lines and labels
        /// </summary>
        private void SetAxesNyquistPlot()
        {
            var xAxisNyquistPlot = new LinearAxis()
            {
                Position = OxyPlot.Axes.AxisPosition.Bottom,
                MajorGridlineStyle = LineStyle.Dash,
                Title = "Z-Re (Ohm)"
            };
            var yAxisNyquistPlot = new LinearAxis()
            {
                Position = OxyPlot.Axes.AxisPosition.Left,
                MajorGridlineStyle = LineStyle.Dash,
                Title = "Z-Im (Ohm)"
            };
            //Add the axes to the Nyquist plot model
            NyquistPlotModel.Axes.Add(xAxisNyquistPlot);
            NyquistPlotModel.Axes.Add(yAxisNyquistPlot);
        }

        /// <summary>
        /// Sets the axes for Bode plot with grid lines and labels
        /// </summary>
        private void SetAxesBodePlot()
        { 
            var xAxisBodePlot = new LogarithmicAxis()
            {
                MajorGridlineStyle = LineStyle.Dash,
                Key = "Frequency",
                Position = AxisPosition.Bottom,
                Title = "Frequency (HZ)",
            };
            var yAxisBodePlot = new LogarithmicAxis()
            {
                MajorGridlineStyle = LineStyle.Dash,
                Key = "Z",
                Position = AxisPosition.Left,
                Title = "Z (Ohm)",
                TitleColor = OxyColors.Blue
            };
            var yAxisBodePlotSecondary = new LogarithmicAxis()
            {
                MajorGridlineStyle = LineStyle.Dash,
                Key = "Phase",
                Position = AxisPosition.Right,
                Title = "Phase (deg)",
                TitleColor = OxyColors.Red
            };
            //Add the axes to the Bode plot model
            BodePlotModel.Axes.Add(xAxisBodePlot);
            BodePlotModel.Axes.Add(yAxisBodePlot);
            BodePlotModel.Axes.Add(yAxisBodePlotSecondary);
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
                lbConsole.Items.Add($"Connected to EmStat Pico.");
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
            lbConsole.Items.Add($"Disconnected from EmStat Pico.");
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
            SendScriptFile();                               // Send the script file for EIS measurement
            ProcessReceivedPackets();                      // Parse the received response packets
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
                readLine = ReadResponseLine();               // Read a line from the response
                RawData += readLine;                         // Add the response to raw data
                if (readLine == "\n")
                    break;
                NDataPointsReceived++;                       // Increment the number of data points if the read line is not empty
                ParsePackageLine(readLine);                  // Parse the line read 
                CalcComplexImpedance();
                UpdatePlots();                               // Update the plot with the new data point added after parsing a line
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
            System.Diagnostics.Debug.Write($"\nindex = " + String.Format("{0,3} {1,5} ", NDataPointsReceived, " "));
            while (!(responsePackageLine.Substring(currentIndex) == "\n"))
            {
                paramIdentifier = responsePackageLine.Substring(currentIndex, 2);                           // The string that identifies the measurement parameter
                paramValue = responsePackageLine.Substring(currentIndex + 2, PACKAGE_PARAM_VALUE_LENGTH);   // The value of the measurement parameter
                double paramValueWithPrefix = ParseParamValues(paramValue);                                 // Append the SI prefix to the value
                switch (paramIdentifier)
                {
                    case "dc":                                                 //Frequency reading
                        System.Diagnostics.Debug.Write(String.Format("{0,14} :{1,10} {2,4}", MeasurementParameters[paramIdentifier], string.Format("{0:0.00}", paramValueWithPrefix).ToString(), " "));
                        FrequencyValues.Add(paramValueWithPrefix);           //If frequency reading add the value to the FrequencyReadings list
                        break;
                    case "cc":                                                 //Real Impedance reading
                        System.Diagnostics.Debug.Write(String.Format("{0,8} :{1,10} {2,4}", MeasurementParameters[paramIdentifier], string.Format("{0:0.000E+00}", paramValueWithPrefix).ToString(), " "));
                        RealImpedanceValues.Add(paramValueWithPrefix);       //If Z(Real) reading add the value to RealImpedanceReadings list
                        break;
                    case "cd":                                                 //Imaginary Impedance reading
                        System.Diagnostics.Debug.Write(String.Format("{0,8} :{1,10} {2,4}", MeasurementParameters[paramIdentifier], string.Format("{0:0.000E+00}", paramValueWithPrefix).ToString(), " "));
                        ImgImpedanceValues.Add(paramValueWithPrefix);        //If Z(Img) reading add the value to ImgImpedanceReadings list
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
            char strUnitPrefix = paramValueString[7];                           //Identify the SI unit prefix from the package at position 8
            string strvalue = paramValueString.Remove(7);                       //Strip the value of the measured parameter from the package
            int value = Convert.ToInt32(strvalue, 16);                          // Convert the hex value to int
            double paramValue = value - OFFSET_VALUE;                           //Values offset to receive only positive values
            return (paramValue * SI_Prefix_Factor[strUnitPrefix.ToString()]);   //Return the value of the parameter after appending the SI unit prefix
        }

        private void CalcComplexImpedance()
        {
            Complex ZComplex = new Complex(RealImpedanceValues.Last(), ImgImpedanceValues.Last());
            ComplexImpedanceValues.Add(ZComplex);
            ImpedanceMagnitudeValues.Add(ComplexImpedanceValues.Last().Magnitude);
            PhaseValues.Add(ComplexImpedanceValues.Last().Phase * 180 / Math.PI);
        }

        /// <summary>
        /// Update the plot with the measurement response values.
        /// </summary>
        private void UpdatePlots()
        {
            NyquistPlotData.Points.Add(new DataPoint(RealImpedanceValues.Last(), ImgImpedanceValues.Last()));    // Add the last added measurement values as new data points and update the plot
            NyquistPlotModel.InvalidatePlot(true);

            BodePlotDataMagnitude.Points.Add(new DataPoint(FrequencyValues.Last(), ImpedanceMagnitudeValues.Last()));
            BodePlotModel.InvalidatePlot(true);

            BodePlotDataPhase.Points.Add(new DataPoint(FrequencyValues.Last(), PhaseValues.Last()));
            BodePlotModel.InvalidatePlot(true);
        }

        ///// <summary>
        ///// Clears the plot before every measurement
        ///// </summary>
        private void ClearPlot()
        {
            NyquistPlotData.Points.Clear();
            NyquistPlotModel.InvalidatePlot(true);
            BodePlotDataMagnitude.Points.Clear();
            BodePlotDataPhase.Points.Clear();
            BodePlotModel.InvalidatePlot(true);
        }

        #endregion
    }
}
