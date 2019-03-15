package com.palmsens.msbluetooth;

import android.Manifest;
import android.app.Activity;
import android.bluetooth.BluetoothAdapter;
import android.bluetooth.BluetoothDevice;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.os.Build;
import android.os.Handler;
import android.os.Message;
import android.os.ParcelUuid;
import android.support.annotation.RequiresApi;
import android.support.v7.app.AppCompatActivity;
import android.os.Bundle;
import android.text.method.ScrollingMovementMethod;
import android.util.Log;
import android.view.View;
import android.widget.AdapterView;
import android.widget.ArrayAdapter;
import android.widget.Button;
import android.widget.ProgressBar;
import android.widget.Spinner;
import android.widget.TextView;
import android.widget.Toast;

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
import java.util.Set;
import java.util.UUID;

public class MSBluetoothActivity extends AppCompatActivity implements AdapterView.OnItemSelectedListener {

    private static final String TAG = "MSBluetoothActivity";

    private static final String CMD_VERSION = "t\n";
    private static final String CMD_ABORT = "Z\n";
    private static final String SCRIPT_FILE_NAME = "LSV_test_script.txt"; //"meas_swv_test.txt"

    private static final int PACKAGE_PARAM_VALUE_LENGTH = 8;                                           // Length of the parameter value in a package
    private static final int OFFSET_VALUE = 0x8000000;

    private int mNDataPointsReceived = 0;
    private final Activity mActivity = this;

    // Message types sent from the BluetoothConnectionService Handler
    public static final int MESSAGE_READ = 1;
    public static final int MESSAGE_SOCKET_CLOSED = 2;
    public static final int MESSAGE_SOCKET_CONNECTED = 3;
    public static final int MESSAGE_TOAST = 4;

    static List<Double> mCurrentReadings = new ArrayList<>();                                    // Collection of current readings
    static List<Double> mVoltageReadings = new ArrayList<>();                                    // Collection of potential readings

    private Button btnConnect;
    private TextView txtResponse;
    private Button btnSend;
    private Button btnAbort;
    private Button btnScan;
    private ProgressBar mProgressScan;
    private ProgressBar mProgressConnect;
    private Spinner spinnerConnectedDevices;

    private BluetoothAdapter mBluetoothAdapter;
    private BluetoothConnectionService mBluetoothConnection;
    private BluetoothDevice mSelectedDevice;
    private UUID mSelectedUUID;
    private ArrayList<BluetoothDevice> mBTDevices = new ArrayList<>();
    private ArrayAdapter<String> mAdapterConnectedDevices;

    private boolean mIsVersionCmdSent = false;
    private boolean mIsConnected = false;
    private boolean mIsScriptActive = false;
    private boolean mIsScanning;

    /**
     * The SI unit of the prefixes and their corresponding factors
     */
    private static final Map<Character, Double> SI_PREFIX_FACTORS = new HashMap<Character, Double>() {{
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
            this.crIndex = crIndex;
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
     *  The Handler that gets information back from the BluetoothConnectionService
     */
    private final android.os.Handler mHandler = new Handler() {
        @Override
        public void handleMessage(Message msg) {
            byte[] readBuff;
            String readMessage;
            switch (msg.what) {
                case MESSAGE_READ:
                    readBuff = (byte[]) msg.obj;                            // The message posted as bytes to the handler
                    readMessage = new String(readBuff);                     // Construct a string from the valid bytes in the buffer
                    if (mIsVersionCmdSent)
                        verifyEmstatPico(readMessage);                      // If version command is sent, verify if device is Emstat Pico
                    else if (processReceivedPackages(readMessage)) {        // Else, send process the received packages
                        btnSend.setEnabled(true);
                        mIsScriptActive = false;
                        btnAbort.setEnabled(false);
                    }
                    break;

                case MESSAGE_SOCKET_CLOSED:
                    readBuff = (byte[]) msg.obj;
                    readMessage = new String(readBuff);
                    Toast.makeText(mActivity, readMessage, Toast.LENGTH_SHORT).show();   // Shows a toast message if socket is closed and connection fails
                    updateView();
                    break;

                case MESSAGE_SOCKET_CONNECTED:
                    sendVersionCmd();                                                   // Sends the version command once the bluetooth socket is connected
                    break;

                case MESSAGE_TOAST:
                    readBuff = (byte[]) msg.obj;
                    readMessage = new String(readBuff);
                    Toast.makeText(mActivity, readMessage, Toast.LENGTH_SHORT).show();
                    break;
            }
        }
    };

    /**
     * Broadcast Receiver for listing paired devices upon on/off bluetooth.
     * When the bluetooth state is ON, calls the method to find the list of paired bluetooth devices.
     */
    private final BroadcastReceiver mBroadcastReceiverBTState = new BroadcastReceiver() {
        public void onReceive(Context context, Intent intent) {
            String action = intent.getAction();

            if (mBluetoothAdapter.ACTION_STATE_CHANGED.equals(action)) {                                // When Bluetooth state is changed
                final int state = intent.getIntExtra(BluetoothAdapter.EXTRA_STATE, mBluetoothAdapter.ERROR);

                switch (state) {
                    case BluetoothAdapter.STATE_ON:                                                     // When bluetooth is turned ON
                        Log.d(TAG, "mBroadcastReceiverBTState: STATE ON");
                        findPairedDevices();
                        break;
                }
            }
        }
    };

    /**
     * Broadcast Receiver for listing devices that are not paired yet; Executed by onClickScan method.
     * When a device is found, it is added to the mBTDevices array and once the discovery is finished, the device names are added to the spinner
     */
    private BroadcastReceiver mBroadcastReceiverDiscovery = new BroadcastReceiver() {
        @Override
        public void onReceive(Context context, Intent intent) {
            final String action = intent.getAction();
            Log.d(TAG, "onReceive: ACTION FOUND.");

            if (BluetoothDevice.ACTION_FOUND.equals(action)) {
                BluetoothDevice device = intent.getParcelableExtra(BluetoothDevice.EXTRA_DEVICE);
                if (device.getName() != null)
                    mBTDevices.add(device);
                Log.d(TAG, "onReceive: " + device.getName() + ": " + device.getAddress());
            }
            if (BluetoothAdapter.ACTION_DISCOVERY_FINISHED.equals(action)) {
                mBluetoothAdapter.cancelDiscovery();                                                    // Cancel discovery when discovery is finished
                if (mIsScanning) {
                    Toast.makeText(context, "Bluetooth devices scan complete.", Toast.LENGTH_SHORT).show();
                    mAdapterConnectedDevices = new ArrayAdapter<>(context, android.R.layout.simple_spinner_dropdown_item, GetDeviceNames(mBTDevices));
                    spinnerConnectedDevices.setAdapter(mAdapterConnectedDevices);
                    scanStopped();
                }
            }
        }
    };

    /**
     * Broadcast Receiver that detects bond state changes (Pairing status changes)
     * Once pairing is complete and the bond state of the device is BOND_BONDED, createBluetoothConnection() is called to create a new BluetoothConnectionService object.
     */
    private final BroadcastReceiver mBroadcastReceiverBondState = new BroadcastReceiver() {
        @Override
        public void onReceive(Context context, Intent intent) {
            final String action = intent.getAction();

            if (BluetoothDevice.ACTION_BOND_STATE_CHANGED.equals(action)) {
                BluetoothDevice mDevice = intent.getParcelableExtra(BluetoothDevice.EXTRA_DEVICE);
                if (mDevice.getBondState() == BluetoothDevice.BOND_BONDED) {                           // Device is bonded already

                    Toast.makeText(context, "Pairing complete.", Toast.LENGTH_SHORT).show();
                    createBluetoothConnection();
                    updateView();
                }
                if (mDevice.getBondState() == BluetoothDevice.BOND_BONDING) {                         // Creating a bond
                    Toast.makeText(context, "Pairing the device...", Toast.LENGTH_SHORT).show();
                }
            }
        }
    };

    /**
     *<Summary>
     *     Itemselected listener that creates a new bluetooth connection after verifying if the device is paired.
     *</Summary>
     * @param parent
     * @param view spinner
     * @param position Position of the selected item in the array adapter
     * @param id spinnerConnectedDevices
     */
    @RequiresApi(api = Build.VERSION_CODES.KITKAT)
    @Override
    public void onItemSelected(AdapterView<?> parent, View view, int position, long id) {
        mBluetoothAdapter.cancelDiscovery();
        mSelectedDevice = mBTDevices.get(position);
        //NOTE: Requires API 19+
        if ((Build.VERSION.SDK_INT > Build.VERSION_CODES.KITKAT) && (mSelectedDevice.getBondState() != BluetoothDevice.BOND_BONDED)) {
            Log.d(TAG, "Trying to pair with " + mSelectedDevice.getName() + " device address:" + mSelectedDevice.getAddress());
            btnConnect.setEnabled(false);
            mSelectedDevice.createBond();           //Create the bond if it is not paired already, response is received by the broadcastReceiverBondState
        } else {
            createBluetoothConnection();            // creates a new bluetooth connection service object
            if (mBluetoothConnection != null) {
                btnConnect.setEnabled(true);
            }
        }
    }

    @Override
    public void onNothingSelected(AdapterView<?> parent) {
        // Do nothing
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_msbluetooth);

        //Initialization of views
        btnConnect = findViewById(R.id.btnConnect);
        spinnerConnectedDevices = findViewById(R.id.spinnerConnectedDevices);
        btnScan = findViewById(R.id.btnScan);
        mProgressScan = findViewById(R.id.progressBarScan);
        mProgressConnect = findViewById(R.id.progressBarConnect);
        txtResponse = findViewById(R.id.txtResponse);
        btnSend = findViewById(R.id.btnSend);
        btnAbort = findViewById(R.id.btnAbort);

        btnConnect.setEnabled(false);
        txtResponse.setMovementMethod(new ScrollingMovementMethod());
        spinnerConnectedDevices.setOnItemSelectedListener(this);        // Spinner click listener

        mBTDevices = new ArrayList<>();
        mBluetoothAdapter = BluetoothAdapter.getDefaultAdapter();
        
        enableBluetooth();                                              // Method call to turn on bluetooth      
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
        unregisterReceiver(mBroadcastReceiverBTState);
        unregisterReceiver(mBroadcastReceiverDiscovery);
        unregisterReceiver(mBroadcastReceiverBondState);
    }

    /**
     * <Summary>
     *     Checks if bluetooth is turned on, else raises a request to turn on bluetooth and finds the paired devices.
     * </Summary>
     */
    public void enableBluetooth() {
        if (mBluetoothAdapter == null) {
            Log.d(TAG, "enableBluetooth: Does not have BT capabilities.");
            return;                                                                          // Returns if device doesn't support bluetooth
        }
        if (!mBluetoothAdapter.isEnabled()) {
            Log.d(TAG, "enableBluetooth: enabling BT.");
            Intent enableBTIntent = new Intent(BluetoothAdapter.ACTION_REQUEST_ENABLE);      // Creates a new bluetooth request intent if bluetooth is not enabled
            startActivity(enableBTIntent);                                                   // Starts the activity to request turning on bluetooth

            IntentFilter BTIntent = new IntentFilter(BluetoothAdapter.ACTION_STATE_CHANGED);
            registerReceiver(mBroadcastReceiverBTState, BTIntent);                           // Registers the broadcast receiver to receive bluetooth action state changed
        } else {
            Log.d(TAG, "BT enabled already");
            findPairedDevices();                                                             // Finds paired devices if bluetooth is enabled already
        }
    }

    /**
     * Finds and add the paired bluetooth devices to the spinner adapter.
     */
    private void findPairedDevices() {
        Set<BluetoothDevice> pairedDevices = BluetoothAdapter.getDefaultAdapter().getBondedDevices();   // Gets the list of paired/bonded devices
        if (pairedDevices.size() > 0) {
            for (BluetoothDevice d : pairedDevices) {
                Log.i(TAG, "paired device: " + d.getName());
                mBTDevices.add(d);                                                                      // Adds the paired device to mBTDevices arraylist
            }
            mAdapterConnectedDevices = new ArrayAdapter<>(this, android.R.layout.simple_spinner_dropdown_item, GetDeviceNames(mBTDevices));
            spinnerConnectedDevices.setAdapter(mAdapterConnectedDevices);
        }
    }

    /**
     * <Summary>
     *     Returns a list of the names of all connected devices.
     * </Summary>
     * @param connectedDevices List of connected bluetooth devices
     * @return A string array with the connected device names
     */
    public String[] GetDeviceNames(ArrayList<BluetoothDevice> connectedDevices) {
        String[] devices = new String[connectedDevices.size()];
        for (int i = 0; i < connectedDevices.size(); i++) {
            devices[i] = connectedDevices.get(i).getName();
        }
        return devices;
    }

    /**
     * <Summary>
     *     The bluetooth adapter starts discovering unpaired devices. Device found and Discovery finished actions are received and handled by the mBroadcastReceiverDiscovery
     * </Summary>
     */
    @RequiresApi(api = Build.VERSION_CODES.M)
    public void discoverUnpairedDevices() {
        Log.d(TAG, "Looking for unpaired devices.");

        if (mBluetoothAdapter.isDiscovering()) {
            mBluetoothAdapter.cancelDiscovery();                                                // Cancel discovery if it is already discovering
        }

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) {
            checkBTPermissions();                                                               // Bluetooth permissions are to be specified in the manifest file. Needs runtime permissions on SDK >= 23 (M)
        }

        mBTDevices.clear();                                                                     // Clears the connected devices list to remove devices not in range
        mBluetoothAdapter.startDiscovery();                                                     // Starts discovering nearby bluetooth devices
        IntentFilter discoverDevicesIntent = new IntentFilter(BluetoothDevice.ACTION_FOUND);    // Broadcasts when device is found
        discoverDevicesIntent.addAction(BluetoothAdapter.ACTION_DISCOVERY_FINISHED);            // Broadcasts when discovery is finished
        registerReceiver(mBroadcastReceiverDiscovery, discoverDevicesIntent);
    }

    /**
     * This method is required for all devices running API23+
     * Android must programmatically check the permissions for bluetooth. Adding the proper permissions in manifest is not enough.
     * NOTE: This will only execute on versions > LOLLIPOP because it is not needed otherwise.
     */
    @RequiresApi(api = Build.VERSION_CODES.M)
    private void checkBTPermissions() {
        int permissionCheck = this.checkSelfPermission("Manifest.permission.ACCESS_FINE_LOCATION")
                                + this.checkSelfPermission("Manifest.permission.ACCESS_COARSE_LOCATION");
        if (permissionCheck != 0) {
            this.requestPermissions(new String[]{Manifest.permission.ACCESS_FINE_LOCATION, Manifest.permission.ACCESS_COARSE_LOCATION}, 1001); //Any number
        }
    }

    /**
     * <Summary>
     *     Enables/disables the buttons and updates the UI 
     * </Summary>
     */
    private void updateView() {
        btnConnect.setEnabled(true);
        btnConnect.setText(mIsConnected ? "Disconnect" : "Connect");
        btnSend.setEnabled(mIsConnected && !mIsScriptActive);
        btnAbort.setEnabled(mIsScriptActive);
        btnScan.setEnabled(!mIsConnected);
        mProgressConnect.setVisibility(View.INVISIBLE);
    }

    /**
     * <Summary>
     *     Creates a new bluetooth connection service object and assigns the handler for call back to handle message postings to update the UI
     * </Summary>
     */
    private void createBluetoothConnection() {
        ParcelUuid[] uuid = mSelectedDevice.getUuids();
        mSelectedUUID = uuid[0].getUuid();
        mBluetoothConnection = new BluetoothConnectionService(MSBluetoothActivity.this, mHandler);
    }
    
    /**
     * <Summary>
     *     Starts a bluetooth connection (Opens a bluetooth socket and prepares the in stream and out stream for data)
     * </Summary>
     */
    public void startBTConnection() {
        Log.d(TAG, "startBTConnection: Initializing RFCOM Bluetooth Connection.");
        mBluetoothConnection.startClient(mSelectedDevice, mSelectedUUID);
    }

    /**
     * <Summary>
     *     Sends the version command to verify if the connected device is EmStat Pico.
     * </Summary>
     */
    private void sendVersionCmd() {
        btnSend.setEnabled(false);
        byte[] bytes = CMD_VERSION.getBytes();
        mBluetoothConnection.write(bytes);
        mIsVersionCmdSent = true;
    }

    /**
     * <Summary>
     *     Verifies if the device connected is EmStat Pico by checking if the version response contains "esp"
     * </Summary>
     */
    private void verifyEmstatPico(String versionString) {
        if (versionString.contains("esp")) {
            Toast.makeText(this, "Connected to EmStatPico", Toast.LENGTH_SHORT).show();
            spinnerConnectedDevices.setEnabled(false);
            mIsConnected = true;
        } else {
            Toast.makeText(this, "Connected device is not EmStat Pico", Toast.LENGTH_SHORT).show();
            mIsConnected = false;
            mBluetoothConnection.disconnect();
        }
        updateView();
    }

    /**
     * <Summary>
     *     Opens and reads from the script file and writes it on the device using the bluetooth socket's out stream.
     * </Summary>
     * @return A boolean to indicate if the script file was sent successfully to the device.
     */
    public boolean sendScriptFile() {
        boolean isScriptSent = false;
        String line;

        try {
            mIsVersionCmdSent = false;
            InputStream scriptFileStream = getAssets().open(SCRIPT_FILE_NAME);
            InputStreamReader inputStreamReader = new InputStreamReader(scriptFileStream);
            BufferedReader bufferedReader = new BufferedReader(inputStreamReader);

            while ((line = bufferedReader.readLine()) != null) {
                line += "\n";
                mBluetoothConnection.write(line.getBytes());
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
     *     Aborts the script by sending the command to abort (CMD_ABORT)
     * </Summary>
     */
    private void abortScript() {
        mBluetoothConnection.write(CMD_ABORT.getBytes());
        mIsScriptActive = false;
        btnSend.setEnabled(mIsConnected);
    }

    /**
     * Trigerred when the bluetooth scanning is finished.
     * Enables the UI elements to start a new scan for bluetooth devices
     */
    private void scanStopped() {
        mIsScanning = false;
        mProgressScan.setVisibility(View.INVISIBLE);
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
            txtResponse.append(String.format(Locale.getDefault(),"%n%nMeasurement completed. %d data points received.", mNDataPointsReceived));
            isComplete = true;
        } else if ('M' == readLine.charAt(1)) {
            txtResponse.append(String.format(Locale.getDefault(),"%nReceiving measurement response...%n", mNDataPointsReceived));
            txtResponse.append(String.format(Locale.getDefault(),"%n%1$5s %2$15s %3$15s %4$12s %5$20s", "Index", "Potential(V)", "Current(A)", "Status", "Current range"));
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
                    mVoltageReadings.add(paramValueWithPrefix);                  // If potential reading add the value to the mVoltageReadings array
                    break;
                case "ba":                                                       // Current reading
                    mCurrentReadings.add(paramValueWithPrefix);                  // If current reading add the value to the CurrentReadings array
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
        ;
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
     *     Calls the method to send the script file.
     * </Summary>
     * @param view btnSend
     */
    public void onClickSendScript(View view) {
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

    /**
     * <Summary>
     *     Calls the method to abort the script.
     * </Summary>
     * @param view btnAbort
     */
    public void onClickAbort(View view) {
        abortScript();
        Toast.makeText(this, "Method Script aborted", Toast.LENGTH_SHORT).show();
    }

    /**
     * Calls the method to discover unpaired devices. Also registers the broadcast receiver to receive bluetooth bond state changes
     * @param view btnScan
     */
    @RequiresApi(api = Build.VERSION_CODES.M)
    public void onClickScan(View view) {
        btnScan.setEnabled(false);
        btnConnect.setEnabled(false);
        mProgressScan.setVisibility(View.VISIBLE);
        mIsScanning = true;
        IntentFilter filter = new IntentFilter(BluetoothDevice.ACTION_BOND_STATE_CHANGED);
        registerReceiver(mBroadcastReceiverBondState, filter);         //Broadcasts when bond state changes (ie:pairing)
        discoverUnpairedDevices();
    }

    /**
     * <Summary>
     *     Creates a new bluetooth connection with the selected device if it is not already connected. Else disconnects the device.
     * </Summary>
     * @param view   btnConnect
     */
    public void onClickConnect(View view) {
        btnConnect.setEnabled(false);
        btnScan.setEnabled(false);
        if (!mIsConnected) {
            mProgressConnect.setVisibility(View.VISIBLE);               // Sets the progress bar visible
            startBTConnection();                                        // Creates a new bluetooth connection
        } else {
            mIsConnected = false;
            if (mIsScriptActive) abortScript();                         // Aborts any running script upon disconnection
            mBluetoothConnection.disconnect();                          // Disconnects the device
            spinnerConnectedDevices.setEnabled(true);
            updateView();                                               // Updates the UI (enable/disable buttons)
        }
    }
    //endregion
}
