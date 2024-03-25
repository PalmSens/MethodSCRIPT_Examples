/*
Copyright (c) 2019-2021 PalmSens BV
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

   - Redistributions of source code must retain the above copyright notice,
     this list of conditions and the following disclaimer.
   - Neither the name of PalmSens BV nor the names of its contributors
     may be used to endorse or promote products derived from this software
     without specific prior written permission.
   - This license does not release you from any requirement to obtain separate
     licenses from 3rd party patent holders to use this software.
   - Use of the software either in source or binary form must be connected to,
     run on or loaded to an PalmSens BV component.

DISCLAIMER: THIS SOFTWARE IS PROVIDED BY PALMSENS "AS IS" AND ANY EXPRESS OR
IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO
EVENT SHALL THE REGENTS AND CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA,
OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

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

    private enum AppState
    {
        Idle,
        Connecting,
        IdleConnected,
        ScriptRunning,
    }

    private static final String TAG = "MethodSCRIPT Activity";

    private static final String CMD_VERSION_STRING = "t\n";
    private static final String CMD_ABORT_STRING = "Z\n";
    private static final String SCRIPT_FILE_NAME = "LSV_on_10kOhm.txt"; //"SWV_on_10kOhm.txt";
    private static final int BAUD_RATE = 230400;                                                    //Baud Rate for EmStat Pico
    private static final int LATENCY_TIMER = 16;                                                    //Read time out for the device in milli seconds.
    private static final int PACKAGE_DATA_VALUE_LENGTH = 8;                                         //Length of the data value in a package
    private static final int OFFSET_VALUE = 0x8000000;                                              //Offset value to receive positive values
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
        put('i', 1.0);
        put(' ', 1.0);
        put('k', 1e3);
        put('M', 1e6);
        put('G', 1e9);
        put('T', 1e12);
        put('P', 1e15);
        put('E', 1e18);
    }};

    private static List<Double> mCurrentReadings = new ArrayList<>();                               //Collection of current readings
    private static List<Double> mVoltageReadings = new ArrayList<>();                               //Collection of potential readings
    private static D2xxManager ftD2xxManager = null;
    private final Handler mHandler = new Handler();
    private FT_Device ftDevice;

    private TextView txtResponse;
    private Button btnConnect;
    private Button btnSend;
    private Button btnAbort;

    private int mNDataPointsReceived = 0;
    private String mVersionResp = "";
    private AppState mAppState;
    private boolean mThreadIsStopped = true;

    /**
     * The beginning characters of the measurement response packages to help parsing the response
     */
    private enum Reply {
        REPLY_BEGIN_VERSION('t'),
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
        CR_100nA(0),
        CR_2uA(1),
        CR_4uA(2),
        CR_8uA(3),
        CR_16uA(4),
        CR_32uA(5),
        CR_63uA(6),
        CR_125uA(7),
        CR_250uA(8),
        CR_500uA(9),
        CR_1mA(10),
        CR_5mA(11),
        HSCR_100nA(128),
        HSCR_1uA(129),
        HSCR_6uA(130),
        HSCR_13uA(131),
        HSCR_25uA(132),
        HSCR_50uA(133),
        HSCR_100uA(134),
        HSCR_200uA(135),
        HSCR_1mA(136),
        HSCR_5mA(137);

        private final int mCrIndex;

        CurrentRanges(int crIndex) {
            this.mCrIndex = crIndex;
        }

        public static CurrentRanges getCurrentRange(int currentRange) {
            for (CurrentRanges cr : values()) {
                if (cr.mCrIndex == currentRange) {
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

    /**
     * Broadcast Receiver for receiving action USB device attached/detached.
     * When the device is attached, calls the method discoverDevices to identify the device.
     * When the device is detached, calls closeDevice()
     */
    private BroadcastReceiver mUsbReceiverAttach = new BroadcastReceiver() {
        public void onReceive(Context context, Intent intent) {
            String action = intent.getAction();
            if (UsbManager.ACTION_USB_DEVICE_ATTACHED.equals(action)) {                             //When device is attached
                if (discoverDevice())
                    Toast.makeText(context, "Found device", Toast.LENGTH_SHORT).show();
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
     *      A runnable that keeps listening for response from the device until device is disconnected.
     *      The response is read character by character.
     *      When a line of measurement package is read, it is posted on the handler of the main thread (UI thread) for further processing.
     * </Summary>
     */
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
                                        processResponse(line);                  //Calls the method to process the measurement package
                                    }
                                });
                                readLine = new StringBuilder();                 //Resets the readLine to store the next measurement package
                            }
                        } // end of if(readSize>0)
                    }// end of synchronized
                }
            } catch (Exception ex) {
                Log.d(TAG, ex.getMessage());
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
        setAppState(AppState.Idle);
    }

    @Override
    protected void onResume() {
        super.onResume();
        discoverDevice();
        updateView();
    }

    /**
     * <Summary>
     *      Unregisters the broadcast receiver(s) to enable garbage collection.
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
     *      Looks up for the device info list using the D2xxManager to check if any USB device is connected.
     * </Summary>
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

    private void setAppState(AppState state)
    {
        mAppState = state;
        updateView();
    }

    /**
     * <Summary>
     *      Enables/disables the buttons and updates the UI
     * </Summary>
     */
    private void updateView() {
        switch(mAppState)
        {
            case Idle:
                btnConnect.setText("Connect");
                btnConnect.setEnabled(discoverDevice());
                btnSend.setEnabled(false);
                btnAbort.setEnabled(false);
                break;

            case Connecting:
                btnConnect.setText("Connect");
                btnConnect.setEnabled(false);
                btnSend.setEnabled(false);
                btnAbort.setEnabled(false);
                break;

            case IdleConnected:
                btnConnect.setText("Disconnect");
                btnConnect.setEnabled(true);
                btnSend.setEnabled(true);
                btnAbort.setEnabled(false);
                break;

            case ScriptRunning:
                btnConnect.setText("Disconnect");
                btnConnect.setEnabled(true);
                btnSend.setEnabled(false);
                btnAbort.setEnabled(true);
                break;
        }
    }

    /**
     * <Summary>
     *  Opens the device and calls the method to set the device configurations.
     *  Also starts a runnable thread (mLoop) that keeps listening to the response until device is disconnected.
     * </Summary>
     * @return A boolean to indicate if the device was opened and configured.
     */
    private boolean openDevice() {
        if (ftDevice != null) {
            ftDevice.close();
        }

        ftDevice = ftD2xxManager.openByIndex(this, 0);

        boolean isConfigured = false;
        if (ftDevice.isOpen()) {
            if (mThreadIsStopped) {
                SetConfig();                                                                        //Configures the port with necessary parameters
                ftDevice.purge((byte) (D2xxManager.FT_PURGE_TX | D2xxManager.FT_PURGE_RX));         //Purges data from the device's TX/RX buffer
                ftDevice.restartInTask();                                                           //Resumes the driver issuing USB in requests
                new Thread(mLoop).start();                                                          //Start parsing thread
                Log.d(TAG, "Device configured. ");
                isConfigured = true;
            }
        }
        return isConfigured;
    }

    /**
     * <Summary>
     *      Sets the device configuration properties like Baudrate (230400), databits (8), stop bits (1), parity (None) and bit mode.
     * </Summary>
     */
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
     *      Sends the version command to verify if the connected device is EmStat Pico.
     * </Summary>
     */
    private void sendVersionCmd() {
        //Send newline to clear command buf on pico, in case there was invalid data in it
        writeToDevice("\n");

        //After some time, send version command (needed if the EmStat Pico had received an invalid command)
        Handler handler = new Handler();
        handler.postDelayed(new Runnable() {
            public void run() {
                mVersionResp = "";
                writeToDevice(CMD_VERSION_STRING);
            }
        }, 200);
    }

    /**
     * <Summary>
     *      Verifies if the device connected is EmStat Pico by checking if the version response contains "esp"
     * </Summary>
     * @param versionStringLine The response string to be verified.
     */
    private void verifyEmstatPico(String versionStringLine) {
        mVersionResp += versionStringLine;
        if (mVersionResp.contains("*\n")) {
            if(mVersionResp.contains("tespico")) {
                Toast.makeText(this, "Connected to EmStatPico.", Toast.LENGTH_SHORT).show();
                setAppState(AppState.IdleConnected);
            }
            else
            {
                Toast.makeText(this, "Failed to connect.", Toast.LENGTH_SHORT).show();
                closeDevice();
            }
        }
    }

    /**
     * <Summary>
     *      Writes the input data to the USB device
     * </Summary>
     * @param script MethodSCRIPT to be written
     * @return A boolean indicating if the write operation succeeded.
     */
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
     *      Reads from the script file and writes it on the device using the bluetooth socket's out stream.
     * </Summary>
     * @return A boolean to indicate if the MethodSCRIPT file was sent successfully to the device.
     */
    private boolean sendScriptFile() {
        boolean isScriptSent = false;
        String line;
        BufferedReader bufferedReader = null;
        try {
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
     *      Aborts the MethodSCRIPT
     * </Summary>
     */
    private void abortScript() {
        if (writeToDevice(CMD_ABORT_STRING)) {
            setAppState(AppState.IdleConnected);
            Toast.makeText(this, "Method Script aborted", Toast.LENGTH_SHORT).show();
        }
    }

    /**
     * <Summary>
     *      Aborts active script if any and closes the device.
     *      Updates the view buttons.
     * </Summary>
     */
    private void closeDevice() {
        mThreadIsStopped = true;
        if (ftDevice != null && ftDevice.isOpen()) {
            if (mAppState == AppState.ScriptRunning) {
                abortScript();
            }
            ftDevice.close();
        }
        setAppState(AppState.Idle);
    }

    /**
     * <Summary>
     *      Processes the measurement packages from the device.
     * </Summary>
     * @param line A complete line of response read from the device.
     */
    private void processResponse(String line) {

        switch(mAppState)
        {
            case Connecting:
                verifyEmstatPico(line);          //Calls the verifyEmStatPico method to verify if the device is EmStat Pico
                break;
            case ScriptRunning:
                processReceivedPackage(line);   //Calls the method to process the received measurement package
                break;
        }
    }

    /**
     * <Summary>
     *      Processes the measurement package from the EmStat Pico and stores the response in RawData.
     * </Summary>
     * @param readLine A measurement package read from the device.
     * @return A boolean to indicate if measurement is complete.
     */
    private void processReceivedPackage(String readLine) {
        Reply beginChar = Reply.getReply(readLine.charAt(0));
        switch (beginChar) {
            case REPLY_EMPTY_LINE:
                setAppState(AppState.IdleConnected);
                break;
            case REPLY_END_MEAS_LOOP:
                txtResponse.append(String.format(Locale.getDefault(), "%n%nMeasurement completed. %d data points received.", mNDataPointsReceived));
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
    }

    /**
     * <summary>
     *      Parses a measurement data package and adds the parsed data values to their corresponding arrays
     * </summary>
     * @param packageLine The measurement data package to be parsed
     */
    private void parsePackageLine(String packageLine) {
        String[] variables;
        String variableIdentifier;
        String dataValue;

        int startingIndex = packageLine.indexOf('P');                            //Identifies the beginning of the measurement data package
        String responsePackageLine = packageLine.substring(startingIndex + 1);   //Removes the beginning character 'P'

        startingIndex = 0;
        txtResponse.append(String.format(Locale.getDefault(), "%n  %-4d", mNDataPointsReceived));
        Log.d(TAG, String.valueOf(mNDataPointsReceived) + "  " + responsePackageLine);

        variables = responsePackageLine.split(";");                       //The data values are separated by the delimiter ';'
        for (String variable : variables) {
            variableIdentifier = variable.substring(startingIndex, 2);           //The String (2 characters) that identifies the measurement variable
            dataValue = variable.substring(startingIndex + 2, startingIndex + 2 + PACKAGE_DATA_VALUE_LENGTH);
            double dataValueWithPrefix = parseParamValues(dataValue);           //Parses the variable values and returns the actual values with their corresponding SI unit prefixes
            switch (variableIdentifier) {
                case "da":                                                       //Potential reading
                    mVoltageReadings.add(dataValueWithPrefix);                  //Adds the value to the mVoltageReadings array
                    break;
                case "ba":                                                       //Current reading
                    mCurrentReadings.add(dataValueWithPrefix);                  //Adds the value to the mCurrentReadings array
                    break;
            }
            txtResponse.append(String.format(Locale.getDefault(), "%1$-3s %2$ -1.4e ", " ", dataValueWithPrefix));
            if (variable.length() > 10 && variable.charAt(10) == ',')
                parseMetaDataValues(variable.substring(11));                    //Parses the metadata values in the variable, if any
        }
    }

    /**
     * <Summary>
     *      Parses the data value package and appends the respective prefixes
     * </Summary>
     * @param paramValueString The data value package to be parsed
     * @return The actual data value (double) after appending the unit prefix
     */
    private double parseParamValues(String paramValueString) {
        if (paramValueString == "     nan")
            return Double.NaN;
        char strUnitPrefix = paramValueString.charAt(7);                         //Identifies the SI unit prefix from the package at position 8
        String strvalue = paramValueString.substring(0, 7);                      //Retrieves the value of the variable from the package
        int value = Integer.parseInt(strvalue, 16);                        //Converts the hex value to int
        double paramValue = value - OFFSET_VALUE;                                //Values offset to receive only positive values
        return (paramValue * SI_PREFIX_FACTORS.get(strUnitPrefix));              //Returns the actual data value after appending the SI unit prefix
    }

    /**
     * <Summary>
     *      Parses the metadata values of the variable, if any.
     *      The first character in each meta data value specifies the type of data.
     *      1 - 1 char hex mask holding the status (0 = OK, 2 = overload, 4 = underload, 8 = overload warning (80% of max))
     *      2 - 2 chars hex holding the current range index. First bit high (0x80) indicates a high speed mode cr.
     *      4 - 1 char hex holding the noise value
     * </Summary>
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
     *      Parses the reading status from the package. 1 char hex mask holding the status (0 = OK, 2 = overload, 4 = underload, 8 = overload warning (80% of max))
     * </Summary>
     * @param metaDatastatus The status metadata to be parsed
     */
    private void getReadingStatusFromPackage(String metaDatastatus) {
        String status = "";
        int statusBits = (Integer.parseInt(String.valueOf(metaDatastatus.charAt(1)), 16));
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
     *      Parses the bytes corresponding to current range from the package and prints the current range value.
     * </Summary>
     * @param metaDataCR The current range meta data value to be parsed
     */
    private byte getCurrentRangeFromPackage(String metaDataCR) {
        byte crByte;
        crByte = Byte.parseByte(String.valueOf(metaDataCR.substring(1, 2)), 16);                //Two characters of the metadata value corresponding to current range are retrieved as byte
        return crByte;
    }

    /**
     * <summary>
     *      Displays the string corresponding to the input cr byte
     * </summary>
     * @param crByte The current range value whose string is to be obtained
     */
    private void DisplayCR(byte crByte) {
        String currentRangeStr;
        CurrentRanges crRange = CurrentRanges.getCurrentRange(crByte);
        switch (crRange) {
            case CR_100nA:
                currentRangeStr = "100nA";
                break;
            case CR_2uA:
                currentRangeStr = "2uA";
                break;
            case CR_4uA:
                currentRangeStr = "4uA";
                break;
            case CR_8uA:
                currentRangeStr = "8uA";
                break;
            case CR_16uA:
                currentRangeStr = "16uA";
                break;
            case CR_32uA:
                currentRangeStr = "32uA";
                break;
            case CR_63uA:
                currentRangeStr = "63uA";
                break;
            case CR_125uA:
                currentRangeStr = "125uA";
                break;
            case CR_250uA:
                currentRangeStr = "250uA";
                break;
            case CR_500uA:
                currentRangeStr = "500uA";
                break;
            case CR_1mA:
                currentRangeStr = "1mA";
                break;
            case CR_5mA:
                currentRangeStr = "5mA";
                break;
            case HSCR_100nA:
                currentRangeStr = "100nA (High speed)";
                break;
            case HSCR_1uA:
                currentRangeStr = "1uA (High speed)";
                break;
            case HSCR_6uA:
                currentRangeStr = "6uA (High speed)";
                break;
            case HSCR_13uA:
                currentRangeStr = "13uA (High speed)";
                break;
            case HSCR_25uA:
                currentRangeStr = "25uA (High speed)";
                break;
            case HSCR_50uA:
                currentRangeStr = "50uA (High speed)";
                break;
            case HSCR_100uA:
                currentRangeStr = "100uA (High speed)";
                break;
            case HSCR_200uA:
                currentRangeStr = "200uA (High speed)";
                break;
            case HSCR_1mA:
                currentRangeStr = "1mA (High speed)";
                break;
            case HSCR_5mA:
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
     *      Parses the noise from the package.
     * </Summary>
     * @param metaDataNoise The metadata noise to be parsed.
     */
    private void GetNoiseFromPackage(String metaDataNoise) {

    }

    //region Events

    /**
     * <Summary>
     * O        pens the device on click of connect and sends the version command to verify if the device is EmStat Pico.
     * </Summary>
     * @param view btnConnect
     */
    public void onClickConnect(View view) {
        switch(mAppState)
        {
            case Idle:
                setAppState(AppState.Connecting);
                if(openDevice())
                    sendVersionCmd();
                break;
            case IdleConnected:
            case ScriptRunning:
                closeDevice();                                                    //Disconnects the device
                Toast.makeText(this, "Device is disconnected.", Toast.LENGTH_SHORT).show();
                break;
        }
    }

    /**
     * <Summary>
     *      Calls the method to abort the MethodSCRIPT.
     * </Summary>
     * @param view btnAbort
     */
    public void onClickAbort(View view) {
        abortScript();
    }

    /**
     * <Summary>
     *      Calls the method to send the MethodSCRIPT.
     * </Summary>
     * @param view btnSend
     */
    public void onClickSend(View view) {
        if (sendScriptFile()) {
            mNDataPointsReceived = 0;
            txtResponse.setText("");
            setAppState(AppState.ScriptRunning);
        } else {
            Toast.makeText(this, "Error sending MethodSCRIPT", Toast.LENGTH_SHORT).show();
        }
    }

    //endregion
}
