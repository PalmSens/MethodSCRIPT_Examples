using OxyPlot;
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
        const int PACKAGE_PARAM_VALUE_LENGTH = 9;
        private SerialPort SerialPortEsP;
        static string ScriptFileName = "LSV_test_script.txt";
        static string AppLocation = Assembly.GetExecutingAssembly().Location;
        //static string ScriptFileName = "200k_500_15mV_cr5m.mscr";
        static string FilePath = System.IO.Path.GetDirectoryName(AppLocation) + "\\scripts";
        static string ScriptFilePath = Path.Combine(FilePath, ScriptFileName);
        const int OFFSET_VALUE = 0x8000000; // Offset value to receive positive values

        const string POTENTIAL_READING = "aa";
        const string CURRENT_READING = "ba";

        private ObservableCollection<double> CurrentReadings = new ObservableCollection<double>();
        private ObservableCollection<double> VoltageReadings = new ObservableCollection<double>();
        private string RawData;
        private int NDataPointsReceived = 0;
        private PlotModel plotModel = new PlotModel();
        private LineSeries plotData;

        readonly static Dictionary<string, double> Prefix_Factor = new Dictionary<string, double> { { "a", 1e-18 }, { "f", 1e-15 }, { "p", 1e-12 }, { "n", 1e-9 }, { "u", 1e-6 }, { "m", 1e-3 },
                                                                                                      { " ", 1 }, { "K", 1e3 }, { "M", 1e6 }, { "G", 1e9 }, { "T", 1e12 }, { "P", 1e15 }, { "E", 1e18 }};

        readonly static Dictionary<string, string> MeasurementParameters = new Dictionary<string, string> { { "aa", "E (V)" }, { "ba", "I (A)" }, { "dc", "Frequency" }, { "cc", "Z'" }, { "cd", "Z''" } };

        public frmPlotExample()
        {
            InitializeComponent();
            samplePlotView.Model = plotModel;
            plotData = new LineSeries()
            {
                Color = OxyColors.Green,
                MarkerType = MarkerType.Circle,
                MarkerSize = 6,
                MarkerStroke = OxyColors.White,
                MarkerFill = OxyColors.Green,
                MarkerStrokeThickness = 1.5,
            };
            //CurrentReadings.CollectionChanged += NewDataAdded;
            //VoltageReadings.CollectionChanged += NewDataAdded;
        }
        
        private void NewDataAdded(object sender, System.Collections.Specialized.NotifyCollectionChangedEventArgs e)
        {
            UpdatePlot();
        }

        /// <summary>
        /// Opens the serial ports and identifies the port connected to EsPico 
        /// </summary>
        /// <returns> The serial port connected to EsPico</returns>
        /// 
        private SerialPort OpenSerialPort()
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
        private void SendScriptFile()
        {
            using (StreamReader stream = new StreamReader(ScriptFilePath))
            {
                string line;
                while (!stream.EndOfStream)
                {
                    line = stream.ReadLine();
                    if (!(line == null))
                    {
                        //Console.WriteLine(line);
                        line += "\n";
                        SerialPortEsP.Write(line);
                    }
                }
                lbConsole.Items.Add("Measurement started.");
            }
        }
        /// <summary>
        /// Connects to the EmStatPico if available .
        /// </summary>
        /// <param name="sender"></param>
        /// <param name="e"></param>
        private void btnConnect_Click(object sender, EventArgs e)
        {
            SerialPortEsP = OpenSerialPort();        // Open and identify the port connected to EsPico
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
        /// Disconnects the EmStatPico.
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
            ClearPlot();
            plotModel.Series.Add(plotData);
            SendScriptFile();
            ProcessReceivedPackets();            //Parse the received response packets
            //UpdatePlot();
            btnMeasure.Enabled = true;
        }

        /// <summary>
        /// Processes the response packets from the EsPico and store the response in RawData.
        /// </summary>
        private void ProcessReceivedPackets()
        {
            string readLine = "";
            lbConsole.Items.Add("Measurement response received:");
            while (true)
            {
                readLine = ReadResponseLine();
                RawData += readLine;
                if (readLine == "\n")
                    break;
                NDataPointsReceived++;
                ParsePackageLine(readLine);
                UpdatePlot();
            }
            lbConsole.Items.Add($"Measurement completed.");
            lbConsole.Items.Add($"{NDataPointsReceived} data points received.");
        }

        /// <summary>
        /// Reads a line from the data received
        /// </summary>
        /// <returns></returns>
        private string ReadResponseLine()
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
        private void ParsePackageLine(string responsePackageLine)
        {
            string paramIdentifier;
            string paramValue;
            int startingIndex = responsePackageLine.IndexOf('P');
            int currentIndex = startingIndex + 1;
            //lbConsole.Items.Add($"\nindex = {NDataPointsReceived}, ");
            while (!(responsePackageLine.Substring(currentIndex) == "\n"))
            {
                paramIdentifier = responsePackageLine.Substring(currentIndex, 2);
                paramValue = responsePackageLine.Substring(currentIndex + 2, PACKAGE_PARAM_VALUE_LENGTH);
                double paramValueWithPrefix = ParseParamValues(paramValue);
                //lbConsole.Items.Add(MeasurementParameters[paramIdentifier] + " : " + string.Format("{0:0.###E+00}", paramValueWithPrefix).ToString() + " ");
                switch (paramIdentifier)
                {
                    case POTENTIAL_READING:
                        VoltageReadings.Add(paramValueWithPrefix);      //If potential reading add the value to the VoltageReadings array
                        break;
                    case CURRENT_READING:
                        CurrentReadings.Add(paramValueWithPrefix);      //If current reading add the value to the CurrentReadings array
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
            char strUnitPrefix = paramValueString[7];
            string strvalue = paramValueString.Remove(7);
            int value = Convert.ToInt32(strvalue, 16);         // Convert hex value to int
            double paramValue = value - OFFSET_VALUE;          //Values offset to receive only positive values
            return (paramValue * Prefix_Factor[strUnitPrefix.ToString()]);
        }

        /// <summary>
        /// Update the plot with the measurement response values.
        /// </summary>
        private void UpdatePlot()
        {
            plotData.Points.Add(new DataPoint(VoltageReadings.Last(), CurrentReadings.Last()));
            plotData.XAxis.Title = "Potential/V";
            plotData.YAxis.Title = "Current/A";
            plotModel.InvalidatePlot(true);
        }

        private void ClearPlot()
        {
            plotModel.Series.Clear();
            plotData.Points.Clear();
            plotModel.InvalidatePlot(true);
        }
    }
}
