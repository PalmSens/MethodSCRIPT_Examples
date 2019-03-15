package com.palmsens.methodSCRIPTExample;

import android.content.BroadcastReceiver;
import android.content.Context;
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
import java.io.InputStream;
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
    private static final String SCRIPT_FILE_NAME = "meas_swv_test.txt"; //"meas_swv_test.txt";

    private static List<Double> mCurrentReadings = new ArrayList<>();                                    // Collection of current readings
    private static List<Double> mVoltageReadings = new ArrayList<>();                                    // Collection of potential readings

    private static final int PACKAGE_PARAM_VALUE_LENGTH = 8;                                              // Length of the parameter value in a package
    private static final int OFFSET_VALUE = 0x8000000;                                                    // Offset value to receive positive values

    private final Handler mHandler = new Handler();

    private static D2xxManager ftD2xxManager = null;
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
     * The SI unit of the prefixes and their corresponding factors
     */
    private static final Map<Character, Double> SI_PREFIX_FACTORS = new HashMap<Character, Double>() {{  // The SI unit of the prefixes and their corresponding factors
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

    /**
     * All possible current ranges that are supported by EmStat Pico.
     */
    private enum CurrentRanges
    {
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

        CurrentRanges(int crIndex)
        {
            this.crIndex = crIndex;
        }
    }
    
    /**
     * Reading status OK, Overload, Underload, Overload_warning
     */
    private enum ReadingStatus
    {
        OK(0x0),
        Overload(0x2),
        Underload(0x4),
        Overload_warning(0x8);

        private final int value;

        ReadingStatus(int value)
        {
            this.value = value;
        }
    }

    /**
     * Broadcast Receiver for receiving action USB device attached/detached.
     * When the device is attached, calls the method discoverDevices to identify the device.
     * When the device is detached, calls closeDevice()
     */
    private BroadcastReceiver mUsbReceiverAttach = new BroadcastReceiver() {
        public void onReceive(Context context, Intent intent) {
            String action = intent.getAction();
            if (UsbManager.ACTION_USB_DEVICE_ATTACHED.equals(action)) {
                if(discoverDevice())
                    Toast.makeText(context,"Found one device", Toast.LENGTH_SHORT).show();
            } else if (UsbManager.ACTION_USB_DEVICE_DETACHED.equals(action)) {
                closeDevice();
                btnConnect.setEnabled(false);
                Toast.makeText(context, "No device found", Toast.LENGTH_SHORT).show();
            }
        }
    };

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
        }
        discoverDevice();
        updateView();
    }

    /**
     * <Summary>
     *     Unregisters all the broadcast receivers to enable garbage collection.
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
     *     Looks up for the device info list using the D2xxManager to check if any USB device is connected.
     * </Summary>
     * @return A boolean indicating if any device is found
     */
    private boolean discoverDevice(){
        boolean isDeviceFound = false;
        int devCount;
        devCount = ftD2xxManager.createDeviceInfoList(this);

        D2xxManager.FtDeviceInfoListNode[] deviceList = new D2xxManager.FtDeviceInfoListNode[devCount];
        ftD2xxManager.getDeviceInfoList(devCount, deviceList);

        if (devCount > 0) {
            isDeviceFound = true;
            btnConnect.setEnabled(true);
        }
        else{
            Toast.makeText(this,"Discovered no device", Toast.LENGTH_SHORT).show();
            btnConnect.setEnabled(false);
        }
        return isDeviceFound;
    }

    /**
     * <Summary>
     *      Opens the device and calls the method to set the device configurations.
     * </Summary>
     * @return A boolean to indicate if the device was opened and configured.
     */
    private boolean openDevice() {
        if (ftDevice == null) {
            ftDevice = ftD2xxManager.openByIndex(this, 0);
        }

        boolean isConfigured = false;
        if (ftDevice.isOpen()) {
            if (mThreadIsStopped) {
                updateView();
                SetConfig();    //Configure the port with necessary parameters
                ftDevice.purge((byte) (D2xxManager.FT_PURGE_TX | D2xxManager.FT_PURGE_RX));       //Purge data from the device's TX/RX buffer
                ftDevice.restartInTask();                                                         //Resumes the driver issuing USB in requests
                Log.d(TAG, "Device configured. ");
                isConfigured = true;
            }
        }
        return isConfigured;
    }

    /**
     * <Summary>
     *     Sets the device configuration properties like Baudrate (230400), databits (8), stop bits (1), parity (None) and bit mode.
     * </Summary>
     */
    private  void SetConfig() {
        int baudRate = 230400;
        byte dataBits = D2xxManager.FT_DATA_BITS_8;
        byte stopBits = D2xxManager.FT_STOP_BITS_1;
        byte parity = D2xxManager.FT_PARITY_NONE;

        if (!ftDevice.isOpen()) {
            Log.e(TAG, "SetConfig: device not open");
            return;
        }
        // configure the port, reset to UART mode for 232 devices
        ftDevice.setBitMode((byte) 0, D2xxManager.FT_BITMODE_RESET);
        ftDevice.setBaudRate(baudRate);
        ftDevice.setDataCharacteristics(dataBits, stopBits, parity);
    }

    /**
     * <Summary>
     *     Sends the version command to verify if the connected device is EmStat Pico.
     *     Also starts a runnable thread (mLoop) that keeps listening to the response untill device is disconnected.
     * </Summary>
     */
    private void sendVersionCmd() {
        btnSend.setEnabled(false);
        mIsVersionCmdSent = writeToDevice(CMD_VERSION_STRING);
        new Thread(mLoop).start();
    }

    /**
     * <Summary>
     *     Checks for the string "esp" in the version response to confirm if the device is EmStat Pico.
     * </Summary>
     * @param versionString
     */
    private void verifyEmstatPico(String versionString) {
        if (versionString.contains("esp")) {
            Toast.makeText(this, "Connected to EmStatPico", Toast.LENGTH_SHORT).show();
            mIsConnected = true;
        }
        else
        {
            Toast.makeText(this, "Not connected to EmStatPico", Toast.LENGTH_SHORT).show();
            mIsConnected = false;
        }
        updateView();
    }

    /**
     * <Summary>
     *     Enables/Disables buttons based on the connection/script status.
     * </Summary>
     */
    private void updateView() {
        btnConnect.setText(mIsConnected? "Disconnect" : "Connect");
        btnSend.setEnabled(mIsConnected && !mIsScriptActive);
        btnAbort.setEnabled(mIsScriptActive);
    }

    /**
     * <Summary>
     *     Writes the input data to the USB device
     * </Summary>
     * @param script data to be written
     * @return A boolean indicating if the write operation succeeded.
     */
    private boolean writeToDevice(String script) {
        int bytesWritten;
        boolean isWriteSuccess = false;
        if (ftDevice != null) {
            synchronized (ftDevice) {
                if (!ftDevice.isOpen()) {
                    Log.e(TAG, "onClickWrite : Device is not open");
                    return isWriteSuccess;
                }

                ftDevice.setLatencyTimer((byte) 16);

                byte[] writeByte = script.getBytes();
                bytesWritten = ftDevice.write(writeByte, script.length());
                if (bytesWritten > 0) {
                    isWriteSuccess = true;
                }
            }
        }
        return isWriteSuccess;
    }

    /**
     * <Summary>
     * <Summary>
     *     Opens and reads from the script file and writes it on the device using the bluetooth socket's out stream.
     * </Summary>
     * @return A boolean to indicate if the script file was sent successfully to the device.
     */
    private boolean sendScriptFile() {
        boolean isScriptSent = false;
        String line;

        try {
            mIsVersionCmdSent = false;
            InputStream scriptFileStream = getAssets().open(SCRIPT_FILE_NAME);
            InputStreamReader inputStreamReader = new InputStreamReader(scriptFileStream);
            BufferedReader bufferedReader = new BufferedReader(inputStreamReader);
            StringBuilder StringBuilder = new StringBuilder();

            while ((line = bufferedReader.readLine()) != null) {
                line += "\n";
                writeToDevice(line);
                StringBuilder.append(System.getProperty("line.separator"));
            }
            scriptFileStream.close();
            bufferedReader.close();
            isScriptSent = true;
        } catch (FileNotFoundException ex) {
            Log.d(TAG, ex.getMessage());
        } catch (IOException ex) {
            Log.d(TAG, ex.getMessage());
        }
        return isScriptSent;
    }

    /**
     * <Summary>
     *     A runnable that keeps listening for response from the device until device is disconnected.
     *     The response is read character by character.
     *     when a line of response is read, it is posted on the handler of the main thread (UI thread) for further processing.
     * </Summary>
     */
    private Runnable mLoop = new Runnable() {
        StringBuilder readLine = new StringBuilder();

        @Override
        public void run() {
            int readSize;
            byte[] rbuf = new byte[1];
            mThreadIsStopped = false;
            while (true) {
                if (mThreadIsStopped) {
                    break;
                }

                synchronized (ftDevice) {
                    readSize = ftDevice.getQueueStatus();
                    if (readSize > 0) {
                        ftDevice.read(rbuf, 1);
                        String rchar = new String(rbuf);
                        readLine.append(rchar);

                        if (rchar.equals("\n")) {
                            mHandler.post(new Runnable() {
                                final String line = readLine.toString();
                                @Override
                                public void run() {
                                    if (mIsVersionCmdSent)
                                    {
                                        verifyEmstatPico(line);
                                    }
                                    else {
                                        if(processReceivedPackages(line)){
                                            btnSend.setEnabled(true);
                                            mIsScriptActive = false;
                                            btnAbort.setEnabled(false);
                                        }
                                    }
                                }
                            });
                            readLine = new StringBuilder();
                        }
                    } // end of if(readSize>0)
                }// end of synchronized
            }
        }
    };

    /**
     * <Summary>
     *     Aborts the script
     * </Summary>
     */
    private void abortScript(){
        if(writeToDevice(CMD_ABORT_STRING)){
            btnSend.setEnabled(mIsConnected);
            Toast.makeText(this, "Method Script aborted", Toast.LENGTH_SHORT).show();
        }
    }

    /**
     * <Summary>
     *     * Aborts active script if any and closes the device.
     *      * Updates the view buttons.
     * </Summary>
     */
    private void closeDevice() {
        mThreadIsStopped = true;
        if (ftDevice != null) {
            if(mIsScriptActive){
                abortScript();
                mIsScriptActive = false;
            }
            ftDevice.close();
            btnConnect.setEnabled(discoverDevice());
        }
        mIsConnected = false;
        updateView();
    }

    /**
     * <Summary>
     *     Processes the response packets from the EmStat Pico and store the response in RawData.
     * </Summary>
     * @param readLine A complete line of response read from the device.
     * @return A boolean to indicate if measurement is complete.
     */
    private boolean processReceivedPackages(String readLine) {
        boolean isComplete = false;
        if (readLine.equals("\n")) {
            isComplete = true;
        } else if (readLine.equals("*\n")) {
            txtResponse.append(String.format("%n%nMeasurement completed. %d data points received.", mNDataPointsReceived));
            isComplete = true;
        } else if ('M' == readLine.charAt(1)) {
            txtResponse.append(String.format("%nReceiving measurement response...%n", mNDataPointsReceived));
            txtResponse.append(String.format("%n%1$5s %2$15s %3$15s %4$12s %5$20s", "Index", "Potential(V)", "Current(A)", "Status", "Current range"));
        } else if ('P' == readLine.charAt(0)) {
            mNDataPointsReceived++;                                  // Increment the number of data points if the read line contains the header char 'P
            parsePackageLine(readLine);                              // Parse the line read
        }
        return isComplete;
    }

    /**
     * <summary>
     *      Parses a single line of the package and adds the values of the measurement parameters to the corresponding arrays
     * </summary>
     * @param packageLine The line from response package to be parsed
     */
    private void parsePackageLine(String packageLine) {
        String[] parameters;
        String paramIdentifier;
        String paramValue;
        int startingIndex = packageLine.indexOf('P');
        String responsePackageLine = packageLine.substring(startingIndex + 1);
        startingIndex = 0;
        txtResponse.append(String.format(Locale.getDefault(),"%n  %-4d", mNDataPointsReceived));
        Log.d(TAG, String.valueOf(mNDataPointsReceived));
        Log.d(TAG, responsePackageLine);
        parameters = responsePackageLine.split(";");
        for (String parameter : parameters) {
            paramIdentifier = parameter.substring(startingIndex, 2);             // The String that identifies the measurement parameter
            paramValue = responsePackageLine.substring(startingIndex + 2, startingIndex + 2 + PACKAGE_PARAM_VALUE_LENGTH);
            double paramValueWithPrefix = parseParamValues(paramValue);
            switch (paramIdentifier) {
                case "da":                                                       // Potential reading
                    mVoltageReadings.add(paramValueWithPrefix);                   // If potential reading add the value to the mVoltageReadings array
                    break;
                case "ba":                                                       // Current reading
                    mCurrentReadings.add(paramValueWithPrefix);                   // If current reading add the value to the CurrentReadings array
                    break;
            }
            txtResponse.append(String.format(Locale.getDefault(),"%1$-3s %2$ -1.4e ", " ", paramValueWithPrefix));
            if (parameter.length() > 10)
                parseMetaDataValues(parameter.substring(11));
        }
    }

    /**
     * <Summary>
     *     Parses the measurement parameter values and appends the respective prefixes
     * </Summary>
     * @param paramValueString The parameter value to be parsed
     * @return The parameter value (double) after appending the unit prefix
     */
    private double parseParamValues(String paramValueString) {
        char strUnitPrefix = paramValueString.charAt(7);                         // Identify the SI unit prefix from the package at position 8
        String strvalue = paramValueString.substring(0, 7);                      // Strip the value of the measured parameter from the package
        int value = Integer.parseInt(strvalue, 16);                        // Convert the hex value to int
        double paramValue = value - OFFSET_VALUE;                                // Values offset to receive only positive values
        return (paramValue * SI_PREFIX_FACTORS.get(strUnitPrefix));              // Return the value of the parameter after appending the SI unit prefix
    }

    /**
     * <Summary>
     *    Parses the meta data values of the variables, if any.
     *    The first character in each meta data value specifies the type of data.
     *    1 - 1 char hex mask holding the status (0 = OK, 2 = overload, 4 = underload, 8 = overload warning (80% of max))
     *    2 - 2 chars hex holding the current range index. First bit high (0x80) indicates a high speed mode cr.
     *    4 - 1 char hex holding the noise value
     * </Summary>
     * @param packageMetaData The metadata values from the package to be parsed.
     */
    private void parseMetaDataValues(String packageMetaData) {
        String[] metaDataValues;
        metaDataValues = packageMetaData.split(",");
        for (String metaData : metaDataValues) {
            switch (metaData.charAt(0)) {
                case '1':
                    getReadingStatusFromPackage(metaData);
                    break;
                case '2':
                    getCurrentRangeFromPackage(metaData);
                    break;
                case '4':
                    GetNoiseFromPackage(metaData);
                    break;
            }
        }
    }

    /**
     * <Summary>
     *     Parses the reading status from the package. 1 char hex mask holding the status (0 = OK, 2 = overload, 4 = underload, 8 = overload warning (80% of max))
     * </Summary>
     * @param metaDatastatus The status metadata to be parsed
     */
    private void getReadingStatusFromPackage(String metaDatastatus) {
        String status = "";
        long statusBits = (Integer.parseInt(String.valueOf(metaDatastatus.charAt(1)), 16));
        if ((statusBits & 0x0) == (long) ReadingStatus.OK.value)
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
     *     Parses the bytes corresponding to current range from the package and prints the current range value.
     * </Summary>
     * @param metaDataCR The current range meta data value to be parsed
     */
    private void getCurrentRangeFromPackage(String metaDataCR) {
        String currentRangeStr = "";
        int crByte;
        crByte = Integer.parseInt(String.valueOf(metaDataCR.substring(1, 2)), 16);
        if (crByte == CurrentRanges.cr100nA.crIndex)
            currentRangeStr = "100nA";
        else if (crByte == CurrentRanges.cr2uA.crIndex)
            currentRangeStr = "2uA";
        else if (crByte == CurrentRanges.cr4uA.crIndex)
            currentRangeStr = "4uA";
        else if (crByte == CurrentRanges.cr8uA.crIndex)
            currentRangeStr = "8uA";
        else if (crByte == CurrentRanges.cr16uA.crIndex)
            currentRangeStr = "16uA";
        else if (crByte == CurrentRanges.cr32uA.crIndex)
            currentRangeStr = "32uA";
        else if (crByte == CurrentRanges.cr63uA.crIndex)
            currentRangeStr = "63uA";
        else if (crByte == CurrentRanges.cr125uA.crIndex)
            currentRangeStr = "125uA";
        else if (crByte == CurrentRanges.cr250uA.crIndex)
            currentRangeStr = "250uA";
        else if (crByte == CurrentRanges.cr500uA.crIndex)
            currentRangeStr = "500uA";
        else if (crByte == CurrentRanges.cr1mA.crIndex)
            currentRangeStr = "1mA";
        else if (crByte == CurrentRanges.cr5mA.crIndex)
            currentRangeStr = "15mA";
        else if (crByte == CurrentRanges.hscr100nA.crIndex)
            currentRangeStr = "100nA (High speed)";
        else if (crByte == CurrentRanges.hscr1uA.crIndex)
            currentRangeStr = "1uA (High speed)";
        else if (crByte == CurrentRanges.hscr6uA.crIndex)
            currentRangeStr = "6uA (High speed)";
        else if (crByte == CurrentRanges.hscr13uA.crIndex)
            currentRangeStr = "13uA (High speed)";
        else if (crByte == CurrentRanges.hscr25uA.crIndex)
            currentRangeStr = "25uA (High speed)";
        else if (crByte == CurrentRanges.hscr50uA.crIndex)
            currentRangeStr = "50uA (High speed)";
        else if (crByte == CurrentRanges.hscr100uA.crIndex)
            currentRangeStr = "100uA (High speed)";
        else if (crByte == CurrentRanges.hscr200uA.crIndex)
            currentRangeStr = "200uA (High speed)";
        else if (crByte == CurrentRanges.hscr1mA.crIndex)
            currentRangeStr = "1mA (High speed)";
        else if (crByte == CurrentRanges.hscr5mA.crIndex)
            currentRangeStr = "5mA (High speed)";
        txtResponse.append(String.format("%1$s %2$-6s", " ", currentRangeStr));
    }

    /**
     * <Summary>
     *     Parses the noise from the package.
     * </Summary>
     * @param metaDataNoise The meta data noise to be parsed.
     */
    private void GetNoiseFromPackage(String metaDataNoise) {

    }

    //region Events

    /**
     * <Summary>
     *     Opens the device on click of connect and sends the version command to verify if the device is EmStat Pico.
     * </Summary>
     * @param view btnConnect
     */
    public void onClickConnect(View view) {
        if(!mIsConnected) {
            if(openDevice())
                sendVersionCmd();
            else
                Toast.makeText(this, "Cannot open device", Toast.LENGTH_SHORT).show();
        }
        else{
            closeDevice();
        }
    }

    /**
     * <Summary>
     *     Aborts the script on click of Abort button.
     * </Summary>
     * @param view btnAbort
     */
    public void onClickAbort(View view) {
        abortScript();
    }

    /**
     * <Summary>
     *     Sends the script file on click of Send button.
     * </Summary>
     * @param view btnSend
     */
    public void onClickSend(View view) {
        mIsScriptActive = sendScriptFile();
        if(mIsScriptActive)
        {
            btnSend.setEnabled(false);
            btnAbort.setEnabled(true);
            mNDataPointsReceived = 0;
            txtResponse.setText("");
        }
        else{
            Toast.makeText(this, "Error sending script", Toast.LENGTH_SHORT).show();
            btnSend.setEnabled(true);
        }
    }
    //endregion
}
