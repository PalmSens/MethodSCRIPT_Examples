package com.palmsens.methodSCRIPTExample;

import android.app.AlertDialog;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.DialogInterface;
import android.content.Intent;
import android.content.IntentFilter;
import android.hardware.usb.UsbManager;
import android.os.Bundle;
import android.os.Handler;
import android.support.v7.app.AppCompatActivity;
import android.text.method.ScrollingMovementMethod;
import android.util.Log;
import android.view.View;
import android.widget.Button;
import android.widget.TextView;
import android.widget.Toast;

import com.ftdi.j2xx.D2xxManager;
import com.ftdi.j2xx.FT_Device;
import com.palmsens.methodscriptexample.R;

import java.io.BufferedReader;
import java.io.FileNotFoundException;
import java.io.IOException;
import java.io.InputStreamReader;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Locale;
import java.util.Map;

public class MethodSCRIPTExample extends AppCompatActivity {

    private static final String TAG = "MethodSCRIPT Activity";

    private static final String CMD_VERSION_STRING = "t\n";
    private static final String CMD_ABORT_STRING = "Z\n";
    private static final String SCRIPT_FILE_NAME = "LSV_on_10KOhm.txt"; //"SWV_on_10KOhm.txt";
    private static final int BAUD_RATE = 230400;                                                    // Baud Rate for EmStat Pico
    private static final int LATENCY_TIMER = 16;                                                    // Read time out for the device in milli seconds.
    private static final int PACKAGE_PARAM_VALUE_LENGTH = 8;                                        // Length of the parameter value in a package
    private static final int OFFSET_VALUE = 0x8000000;                                              // Offset value to receive positive values
    /**
     * The SI unit of the prefixes and their corresponding factors
     */
    private static final Map<Character, Double> SI_PREFIX_FACTORS = new HashMap<Character, Double>() {{  //The SI unit of the prefixes and their corresponding factors
        put('a', 1e-18);
        put('f', 1e-15);
        put('p', 1e-12);
        put('n', 1e-9);
        put('u', 1e-6);
        put('m', 1e-3);
        put(' ', 1.0);
        put('K', 1e3);
        put('M', 1e6);
        put('G', 1e9);
        put('T', 1e12);
        put('P', 1e15);
        put('E', 1e18);
    }};
    private static List<Double> mCurrentReadings = new ArrayList<>();                               // Collection of current readings
    private static List<Double> mVoltageReadings = new ArrayList<>();                               // Collection of potential readings
    private static D2xxManager ftD2xxManager = null;
    private final Handler mHandler = new Handler();
    private FT_Device ftDevice;

    private TextView txtResponse;
    private Button btnConnect;
    private Button btnSend;
    private Button btnAbort;

    private int mNDataPointsReceived = 0;
    private boolean mIsConnected = false;
    private boolean mIsScriptActive = false;
    private boolean mThreadIsStopped = true;
    private boolean mIsVersionCmdSent = false;
    /**
     * Broadcast Receiver for receiving action USB device attached/detached.
     * When the device is attached, calls the method discoverDevices to identify the device.
     * When the device is detached, calls closeDevice()
     */
    //TODO: Close the device when detached - DONE
    private BroadcastReceiver mUsbReceiverAttach = new BroadcastReceiver() {
        public void onReceive(Context context, Intent intent) {
            String action = intent.getAction();
            if (UsbManager.ACTION_USB_DEVICE_ATTACHED.equals(action)) {                             //When device is attached
                if (discoverDevice())
                    Toast.makeText(context, "Found one device", Toast.LENGTH_SHORT).show();
            } else if (UsbManager.ACTION_USB_DEVICE_DETACHED.equals(action)) {                      //When device is detached
                synchronized (ftDevice) {
                    ftDevice.close();
                }
                closeDevice();                                                                      //Closes the device
                btnConnect.setEnabled(false);
                Toast.makeText(context, "No device found", Toast.LENGTH_SHORT).show();
            }
        }
    };
    /**
     * <Summary>
     * A runnable that keeps listening for response from the device until device is disconnected.
     * The response is read character by character.
     * when a line of response is read, it is posted on the handler of the main thread (UI thread) for further processing.
     * </Summary>
     */
    // TODO: Add more commenting - DONE
    // TODO: Move verifyEmStatPico elsewhere - DONE
    // TODO: Send the response line in run() and processing and updating the UI to be done elsewhere - DONE
    private Runnable mLoop = new Runnable() {
        StringBuilder readLine = new StringBuilder();

        @Override
        public void run() {
            int readSize;
            byte[] rbuf = new byte[1];
            mThreadIsStopped = false;
            try {
                while (!mThreadIsStopped && ftDevice != null) {
                    synchronized (ftDevice) {
                        readSize = ftDevice.getQueueStatus();                    //Retrieves the size of the available data on the device
                        if (readSize > 0) {
                            ftDevice.read(rbuf, 1);                      //Reads one character from the device
                            String rchar = new String(rbuf);
                            readLine.append(rchar);                             //Forms a line of response by appending the character read

                            if (rchar.equals("\n")) {                           //When a new line '\n' is read, the line is sent for processing
                                mHandler.post(new Runnable() {
                                    final String line = readLine.toString();

                                    @Override
                                    public void run() {
                                        processResponse(line);                  //Calls the method to process the response line
                                    }
                                });
                                readLine = new StringBuilder();                 //Resets the readLine to store the next line of response
                            }
                        } // end of if(readSize>0)
                    }// end of synchronized
                }
            } catch (Exception ex) {
                Log.d(TAG, ex.getMessage());
            }
        }
    };

    //TODO: If D2xxManager null, exit application with an alert - DONE
    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_method_script_example);

        txtResponse = findViewById(R.id.txtResponse);
        txtResponse.setMovementMethod(new ScrollingMovementMethod());
        btnConnect = findViewById(R.id.btnConnect);
        btnSend = findViewById(R.id.btnSend);
        btnAbort = findViewById(R.id.btnAbort);

        IntentFilter filter = new IntentFilter();
        filter.addAction(UsbManager.ACTION_USB_DEVICE_ATTACHED);
        filter.addAction(UsbManager.ACTION_USB_DEVICE_DETACHED);
        registerReceiver(mUsbReceiverAttach, filter);

        try {
            ftD2xxManager = D2xxManager.getInstance(this);
        } catch (D2xxManager.D2xxException ex) {
            Log.e(TAG, ex.toString());
            AlertDialog.Builder builder = new AlertDialog.Builder(this);                    // Exit the application if D2xxManager instance is null.
            builder.setMessage("Failed to retrieve an instance of D2xxManager. The application is forced to exit.")
                    .setCancelable(false)
                    .setPositiveButton("OK", new DialogInterface.OnClickListener() {
                        public void onClick(DialogInterface dialog, int id) {
                            MethodSCRIPTExample.this.finish();
                        }
                    });
            AlertDialog alert = builder.create();
            alert.show();
            return;
        }
        discoverDevice();
        updateView();
    }

    /**
     * <Summary>
     * Unregisters the broadcast receiver(s) to enable garbage collection.
     * </Summary>
     */
    @Override
    protected void onDestroy() {
        Log.d(TAG, "onDestroy: called.");
        super.onDestroy();
        unregisterReceiver(mUsbReceiverAttach);
    }

    /**
     * <Summary>
     * Looks up for the device info list using the D2xxManager to check if any USB device is connected.
     * </Summary>
     *
     * @return A boolean indicating if any device is found
     */
    private boolean discoverDevice() {
        boolean isDeviceFound = false;
        int devCount;
        devCount = ftD2xxManager.createDeviceInfoList(this);

        D2xxManager.FtDeviceInfoListNode[] deviceList = new D2xxManager.FtDeviceInfoListNode[devCount];
        ftD2xxManager.getDeviceInfoList(devCount, deviceList);

        if (devCount > 0) {
            isDeviceFound = true;
            btnConnect.setEnabled(true);
        } else {
            Toast.makeText(this, "Discovered no device", Toast.LENGTH_SHORT).show();
            btnConnect.setEnabled(false);
        }
        return isDeviceFound;
    }

    @Override
    protected void onResume() {
        super.onResume();
        discoverDevice();
        updateView();
    }

    /**
     * <Summary>
     * Opens the device and calls the method to set the device configurations.
     * </Summary>
     *
     * @return A boolean to indicate if the device was opened and configured.
     */
    // TODO: Check if ftDevice is not null, if so close and reopen - DONE
    private boolean openDevice() {
        if (ftDevice != null) {
            ftDevice.close();
        }

        ftDevice = ftD2xxManager.openByIndex(this, 0);

        boolean isConfigured = false;
        if (ftDevice.isOpen()) {
            if (mThreadIsStopped) {
                updateView();
                SetConfig();                                                                        //Configures the port with necessary parameters
                ftDevice.purge((byte) (D2xxManager.FT_PURGE_TX | D2xxManager.FT_PURGE_RX));         //Purges data from the device's TX/RX buffer
                ftDevice.restartInTask();                                                           //Resumes the driver issuing USB in requests
                Log.d(TAG, "Device configured. ");
                isConfigured = true;
            }
        }
        return isConfigured;
    }

    /**
     * <Summary>
     * Sets the device configuration properties like Baudrate (230400), databits (8), stop bits (1), parity (None) and bit mode.
     * </Summary>
     */
    //TODO: Latency timer make it constant and mention that it is in ms. - DONE
    private void SetConfig() {
        byte dataBits = D2xxManager.FT_DATA_BITS_8;
        byte stopBits = D2xxManager.FT_STOP_BITS_1;
        byte parity = D2xxManager.FT_PARITY_NONE;

        if (!ftDevice.isOpen()) {
            Log.e(TAG, "SetConfig: device not open");
            return;
        }
        // configures the port, reset to UART mode for 232 devices
        ftDevice.setBitMode((byte) 0, D2xxManager.FT_BITMODE_RESET);
        ftDevice.setBaudRate(BAUD_RATE);
        ftDevice.setLatencyTimer((byte) LATENCY_TIMER);
        ftDevice.setDataCharacteristics(dataBits, stopBits, parity);
    }

    /**
     * <Summary>
     * Sends the version command to verify if the connected device is EmStat Pico.
     * Also starts a runnable thread (mLoop) that keeps listening to the response untill device is disconnected.
     * </Summary>
     */
    private void sendVersionCmd() {
        btnSend.setEnabled(false);
        mIsVersionCmdSent = writeToDevice(CMD_VERSION_STRING);
        new Thread(mLoop).start();
    }

    /**
     * <Summary>
     * Checks for the string "esp" in the version response to confirm if the device is EmStat Pico.
     * </Summary>
     *
     * @param versionString The response string to be verified
     */
    // TODO: Change version string to "espico" - DONE.
    private void verifyEmstatPico(String versionString) {
        if (versionString.contains("espico")) {
            Toast.makeText(this, "Connected to EmStatPico", Toast.LENGTH_SHORT).show();
            mIsConnected = true;
        } else {
            Toast.makeText(this, "Not connected to EmStatPico", Toast.LENGTH_SHORT).show();
            mIsConnected = false;
        }
        updateView();
    }

    /**
     * <Summary>
     * Enables/Disables buttons based on the connection/script status.
     * </Summary>
     */
    private void updateView() {
        btnConnect.setText(mIsConnected ? "Disconnect" : "Connect");
        btnSend.setEnabled(mIsConnected && !mIsScriptActive);
        btnAbort.setEnabled(mIsScriptActive);
    }

    /**
     * <Summary>
     * Writes the input data to the USB device
     * </Summary>
     *
     * @param script data to be written
     * @return A boolean indicating if the write operation succeeded.
     */
    //TODO: Verify if the bytes written equals bytes sent - DONE
    private boolean writeToDevice(String script) {
        int bytesWritten;
        boolean isWriteSuccess = false;
        if (ftDevice != null) {
            synchronized (ftDevice) {
                if (!ftDevice.isOpen()) {
                    Log.e(TAG, "onClickWrite : Device is not open");
                    return false;
                }

                byte[] writeByte = script.getBytes();
                bytesWritten = ftDevice.write(writeByte, script.length());                          //Writes to the device
                if (bytesWritten == writeByte.length) {                                             //Verifies if the bytes written equals the total number of bytes sent
                    isWriteSuccess = true;
                }
            }
        }
        return isWriteSuccess;
    }

    /**
     * <Summary>
     * <Summary>
     * Opens and reads from the script file and writes it on the device using the bluetooth socket's out stream.
     * </Summary>
     *
     * @return A boolean to indicate if the script file was sent successfully to the device.
     */
    //TODO: Close the bufferedReader in finally - DONE
    private boolean sendScriptFile() {
        boolean isScriptSent = false;
        String line;
        BufferedReader bufferedReader = null;
        try {
            mIsVersionCmdSent = false;
            bufferedReader = new BufferedReader(new InputStreamReader(getAssets().open(SCRIPT_FILE_NAME)));
            while ((line = bufferedReader.readLine()) != null) {
                line += "\n";
                writeToDevice(line);
            }
            isScriptSent = true;
        } catch (FileNotFoundException ex) {
            Log.d(TAG, ex.getMessage());
        } catch (IOException ex) {
            Log.d(TAG, ex.getMessage());
        } finally {
            if (bufferedReader != null) {
                try {
                    bufferedReader.close();
                } catch (IOException ex) {
                    Log.d(TAG, ex.getMessage());
                }
            }
        }
        return isScriptSent;
    }

    /**
     * <Summary>
     * Aborts the script
     * </Summary>
     */
    private void abortScript() {
        if (writeToDevice(CMD_ABORT_STRING)) {
            btnSend.setEnabled(mIsConnected);
            Toast.makeText(this, "Method Script aborted", Toast.LENGTH_SHORT).show();
        }
    }

    /**
     * <Summary>
     * Aborts active script if any and closes the device.
     * Updates the view buttons.
     * </Summary>
     */
    //TODO: Check if ftDevice is open, before aborting on disconnect - DONE
    private void closeDevice() {
        mThreadIsStopped = true;
        if (ftDevice != null && ftDevice.isOpen()) {
            if (mIsScriptActive) {
                abortScript();
            }
            ftDevice.close();
            btnConnect.setEnabled(discoverDevice());                            //Calls the method discoverDevice() and updates the connect button
        }
        mIsScriptActive = false;
        mIsConnected = false;
        updateView();
    }

    /**
     * <Summary>
     * Processes the response from the EmStat Pico.
     * </Summary>
     *
     * @param line A complete line of response read from the device.
     */
    //TODO: Remove 'e' before sending it for parsing - DONE
    //TODO: A separate method to process the line read and update UI when complete - DONE
    private void processResponse(String line) {
        Reply beginChar = Reply.getReply(line.charAt(0));
        if (beginChar == null)                                          //If connected to Palmsens, the response to version cmd might be different
        {
            Toast.makeText(this, "Failed to connect", Toast.LENGTH_SHORT).show();
            return;
        }
        if (mIsVersionCmdSent)                                          //Identifies if it is the response to version command
        {
            if (line.contains("*\n"))
                return;
            verifyEmstatPico(line);                                    //Calls the verifyEmStatPico method to verify if the device is EmStat Pico
        } else {
            if (beginChar == Reply.REPLY_BEGIN_RESPONSE)
                line = line.substring(1);                              //Removes the beginning character of the response if it is 'e'
            if (mIsConnected && processReceivedPackages(line)) {         //Calls the method to process the received response packets
                btnSend.setEnabled(true);                              //Updates the UI upon completion of parsing and displaying of the response
                mIsScriptActive = false;
                btnAbort.setEnabled(false);
            }
        }
    }

    /**
     * <Summary>
     * Processes the response packets from the EmStat Pico and store the response in RawData.
     * </Summary>
     *
     * @param readLine A complete line of response read from the device.
     * @return A boolean to indicate if measurement is complete.
     */
    private boolean processReceivedPackages(String readLine) {
        boolean isComplete = false;
        Reply beginChar = Reply.getReply(readLine.charAt(0));
        switch (beginChar) {
            case REPLY_EMPTY_LINE:
                isComplete = true;
                break;
            case REPLY_END_MEAS_LOOP:
                txtResponse.append(String.format(Locale.getDefault(), "%n%nMeasurement completed. %d data points received.", mNDataPointsReceived));
                isComplete = true;
                break;
            case REPLY_MEASURING:
                txtResponse.append(String.format(Locale.getDefault(), "%nReceiving measurement response...%n"));
                txtResponse.append(String.format(Locale.getDefault(), "%n%1$5s %2$15s %3$15s %4$12s %5$20s", "Index", "Potential(V)", "Current(A)", "Status", "Current range"));
                break;
            case REPLY_BEGIN_PACKETS:
                mNDataPointsReceived++;                                  //Increments the number of data points if the read line contains the header char 'P
                parsePackageLine(readLine);                              //Parses the line read
                break;
            case REPLY_ABORTED:
                txtResponse.append(String.format(Locale.getDefault(), "%nMeasurement aborted.%n"));
            default:
                break;
        }
        return isComplete;
    }

    /**
     * <summary>
     * Parses a single line of the package and adds the values of the measurement parameters to the corresponding arrays
     * </summary>
     *
     * @param packageLine The line from response package to be parsed
     */
    //TODO: To verify if metadata present use the 10th character ',' condition, just that it is more explicit- DONE
    private void parsePackageLine(String packageLine) {
        String[] parameters;
        String paramIdentifier;
        String paramValue;

        int startingIndex = packageLine.indexOf('P');                            //Identifies the beginning of the package
        String responsePackageLine = packageLine.substring(startingIndex + 1);   //Removes the beginning character 'P'

        startingIndex = 0;
        txtResponse.append(String.format(Locale.getDefault(), "%n  %-4d", mNDataPointsReceived));
        Log.d(TAG, String.valueOf(mNDataPointsReceived) + "  " + responsePackageLine);

        parameters = responsePackageLine.split(";");                       //Parameter values are separated by the delimiter ';'
        for (String parameter : parameters) {
            paramIdentifier = parameter.substring(startingIndex, 2);             //The String (2 characters) that identifies the measurement parameter
            paramValue = parameter.substring(startingIndex + 2, startingIndex + 2 + PACKAGE_PARAM_VALUE_LENGTH);
            double paramValueWithPrefix = parseParamValues(paramValue);          //Parses the parameter values and returns the actual values with their corresponding SI unit prefixes
            switch (paramIdentifier) {
                case "da":                                                       //Potential reading
                    mVoltageReadings.add(paramValueWithPrefix);                  //Adds the value to the mVoltageReadings array
                    break;
                case "ba":                                                       //Current reading
                    mCurrentReadings.add(paramValueWithPrefix);                  //Adds the value to the mCurrentReadings array
                    break;
            }
            txtResponse.append(String.format(Locale.getDefault(), "%1$-3s %2$ -1.4e ", " ", paramValueWithPrefix));
            if (parameter.length() > 10 && parameter.charAt(10) == ',')
                parseMetaDataValues(parameter.substring(11));                    //Parses the metadata values in the parameter, if any
        }
    }

    /**
     * <Summary>
     * Parses the measurement parameter values and appends the respective prefixes
     * </Summary>
     *
     * @param paramValueString The parameter value to be parsed
     * @return The parameter value (double) after appending the unit prefix
     */
    private double parseParamValues(String paramValueString) {
        char strUnitPrefix = paramValueString.charAt(7);                         //Identifies the SI unit prefix from the package at position 8
        String strvalue = paramValueString.substring(0, 7);                      //Retrieves the value of the measured parameter from the package
        int value = Integer.parseInt(strvalue, 16);                        //Converts the hex value to int
        double paramValue = value - OFFSET_VALUE;                                //Values offset to receive only positive values
        return (paramValue * SI_PREFIX_FACTORS.get(strUnitPrefix));              //Returns the value of the parameter after appending the SI unit prefix
    }

    /**
     * <Summary>
     * Parses the meta data values of the variables, if any.
     * The first character in each meta data value specifies the type of data.
     * 1 - 1 char hex mask holding the status (0 = OK, 2 = overload, 4 = underload, 8 = overload warning (80% of max))
     * 2 - 2 chars hex holding the current range index. First bit high (0x80) indicates a high speed mode cr.
     * 4 - 1 char hex holding the noise value
     * </Summary>
     *
     * @param packageMetaData The metadata values from the package to be parsed.
     */
    private void parseMetaDataValues(String packageMetaData) {
        String[] metaDataValues;
        byte crByte;
        metaDataValues = packageMetaData.split(",");
        for (String metaData : metaDataValues) {
            switch (metaData.charAt(0)) {
                case '1':
                    getReadingStatusFromPackage(metaData);
                    break;
                case '2':
                    crByte = getCurrentRangeFromPackage(metaData);
                    if (crByte != 0) DisplayCR(crByte);
                    break;
                case '4':
                    GetNoiseFromPackage(metaData);
                    break;
            }
        }
    }

    /**
     * <Summary>
     * Parses the reading status from the package. 1 char hex mask holding the status (0 = OK, 2 = overload, 4 = underload, 8 = overload warning (80% of max))
     * </Summary>
     *
     * @param metaDatastatus The status metadata to be parsed
     */
    private void getReadingStatusFromPackage(String metaDatastatus) {
        String status = "";
        long statusBits = (Integer.parseInt(String.valueOf(metaDatastatus.charAt(1)), 16));
        if ((statusBits) == (long) ReadingStatus.OK.value)
            status = ReadingStatus.OK.name();
        if ((statusBits & 0x2) == (long) ReadingStatus.Overload.value)
            status = ReadingStatus.Overload.name();
        if ((statusBits & 0x4) == (long) ReadingStatus.Underload.value)
            status = ReadingStatus.Underload.name();
        if ((statusBits & 0x8) == (long) ReadingStatus.Overload_warning.value)
            status = ReadingStatus.Overload_warning.name();
        txtResponse.append(String.format("%1$4s %2$-16s", " ", status));
    }

    /**
     * <Summary>
     * Parses the bytes corresponding to current range from the package and prints the current range value.
     * </Summary>
     *
     * @param metaDataCR The current range meta data value to be parsed
     */
    //TODO: Change if-else to Switch case - DONE
    private byte getCurrentRangeFromPackage(String metaDataCR) {
        byte crByte;
        crByte = Byte.parseByte(String.valueOf(metaDataCR.substring(1, 2)), 16);                //Two characters of the meta data value corresponding to current range are retrieved as byte
        return crByte;
    }

    /// <summary>
    /// Displays the string corresponding to the input cr byte
    /// </summary>
    /// <param name="crByte">The crByte value whose string is to be obtained</param>
    private void DisplayCR(byte crByte) {
        String currentRangeStr;
        CurrentRanges crRange = CurrentRanges.getCurrentRange(crByte);
        switch (crRange) {
            case cr100nA:
                currentRangeStr = "100nA";
                break;
            case cr2uA:
                currentRangeStr = "2uA";
                break;
            case cr4uA:
                currentRangeStr = "4uA";
                break;
            case cr8uA:
                currentRangeStr = "8uA";
                break;
            case cr16uA:
                currentRangeStr = "16uA";
                break;
            case cr32uA:
                currentRangeStr = "32uA";
                break;
            case cr63uA:
                currentRangeStr = "63uA";
                break;
            case cr125uA:
                currentRangeStr = "125uA";
                break;
            case cr250uA:
                currentRangeStr = "250uA";
                break;
            case cr500uA:
                currentRangeStr = "500uA";
                break;
            case cr1mA:
                currentRangeStr = "1mA";
                break;
            case cr5mA:
                currentRangeStr = "5mA";
                break;
            case hscr100nA:
                currentRangeStr = "100nA (High speed)";
                break;
            case hscr1uA:
                currentRangeStr = "1uA (High speed)";
                break;
            case hscr6uA:
                currentRangeStr = "6uA (High speed)";
                break;
            case hscr13uA:
                currentRangeStr = "13uA (High speed)";
                break;
            case hscr25uA:
                currentRangeStr = "25uA (High speed)";
                break;
            case hscr50uA:
                currentRangeStr = "50uA (High speed)";
                break;
            case hscr100uA:
                currentRangeStr = "100uA (High speed)";
                break;
            case hscr200uA:
                currentRangeStr = "200uA (High speed)";
                break;
            case hscr1mA:
                currentRangeStr = "1mA (High speed)";
                break;
            case hscr5mA:
                currentRangeStr = "5mA (High speed)";
                break;
            default:
                currentRangeStr = "N/A";
                break;
        }
        txtResponse.append(String.format("%1$s %2$-6s", " ", currentRangeStr));
    }

    /**
     * <Summary>
     * Parses the noise from the package.
     * </Summary>
     *
     * @param metaDataNoise The meta data noise to be parsed.
     */
    private void GetNoiseFromPackage(String metaDataNoise) {

    }

    /**
     * <Summary>
     * Opens the device on click of connect and sends the version command to verify if the device is EmStat Pico.
     * </Summary>
     *
     * @param view btnConnect
     */
    public void onClickConnect(View view) {
        if (!mIsConnected) {
            if (openDevice())
                sendVersionCmd();                                       //Sends the version command if device is opened
            else
                Toast.makeText(this, "Cannot open device", Toast.LENGTH_SHORT).show();
        } else {
            closeDevice();                                              //Closes the device upon disconnecting it
        }
    }

    /**
     * <Summary>
     * Aborts the script on click of Abort button.
     * </Summary>
     *
     * @param view btnAbort
     */
    public void onClickAbort(View view) {
        abortScript();
    }

    /**
     * <Summary>
     * Sends the script file on click of Send button.
     * </Summary>
     *
     * @param view btnSend
     */
    public void onClickSend(View view) {
        mIsScriptActive = sendScriptFile();
        if (mIsScriptActive) {
            btnSend.setEnabled(false);
            btnAbort.setEnabled(true);
            mNDataPointsReceived = 0;
            txtResponse.setText("");
        } else {
            Toast.makeText(this, "Error sending script", Toast.LENGTH_SHORT).show();
            btnSend.setEnabled(true);
        }
    }

    //region Events

    /**
     * The beginning characters of the measurement response to help parsing the response
     */
    private enum Reply {
        REPLY_BEGIN_VERSION('t'),
        REPLY_VERSION_TYPE('B'),
        REPLY_BEGIN_RESPONSE('e'),
        REPLY_MEASURING('M'),
        REPLY_BEGIN_PACKETS('P'),
        REPLY_END_MEAS_LOOP('*'),
        REPLY_EMPTY_LINE('\n'),
        REPLY_ABORTED('Z');

        private final char mReply;

        Reply(char reply) {
            this.mReply = reply;
        }

        public static Reply getReply(char reply) {
            for (Reply replyChar : values()) {
                if (replyChar.mReply == reply) {
                    return replyChar;
                }
            }
            return null;
        }
    }

    /**
     * All possible current ranges that are supported by EmStat Pico.
     */
    private enum CurrentRanges {
        cr100nA(0),
        cr2uA(1),
        cr4uA(2),
        cr8uA(3),
        cr16uA(4),
        cr32uA(5),
        cr63uA(6),
        cr125uA(7),
        cr250uA(8),
        cr500uA(9),
        cr1mA(10),
        cr5mA(11),
        hscr100nA(128),
        hscr1uA(129),
        hscr6uA(130),
        hscr13uA(131),
        hscr25uA(132),
        hscr50uA(133),
        hscr100uA(134),
        hscr200uA(135),
        hscr1mA(136),
        hscr5mA(137);

        private final int crIndex;

        CurrentRanges(int crIndex) {
            this.crIndex = crIndex;                 //Assigns an index (int) to the enum values
        }

        public static CurrentRanges getCurrentRange(int currentRange) {
            for (CurrentRanges cr : values()) {
                if (cr.crIndex == currentRange) {
                    return cr;
                }
            }
            return null;
        }
    }

    /**
     * Reading status OK, Overload, Underload, Overload_warning
     */
    private enum ReadingStatus {
        OK(0x0),
        Overload(0x2),
        Underload(0x4),
        Overload_warning(0x8);

        private final int value;

        ReadingStatus(int value) {
            this.value = value;
        }
    }
    //endregion
}
