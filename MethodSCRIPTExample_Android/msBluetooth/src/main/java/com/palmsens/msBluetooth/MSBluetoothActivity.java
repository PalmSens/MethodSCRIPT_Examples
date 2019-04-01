package com.palmsens.msBluetooth;

import android.Manifest;
import android.app.Activity;
import android.app.AlertDialog;
import android.bluetooth.BluetoothAdapter;
import android.bluetooth.BluetoothDevice;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.DialogInterface;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.pm.PackageManager;
import android.os.Build;
import android.os.Bundle;
import android.os.Handler;
import android.os.Message;
import android.os.ParcelUuid;
import android.support.annotation.NonNull;
import android.support.annotation.RequiresApi;
import android.support.v4.app.ActivityCompat;
import android.support.v7.app.AppCompatActivity;
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
import java.io.InputStreamReader;
import java.lang.ref.WeakReference;
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
    private static final String SCRIPT_FILE_NAME = "LSV_on_10kOhm.txt"; //"SWV_on_10kOhm.txt"
    private static final UUID DEFAULT_UUID = UUID.fromString("00001101-0000-1000-8000-00805F9B34FB");

    private static final int REQUEST_CODE_LOCATION = 1;

    private static final int PACKAGE_DATA_VALUE_LENGTH = 8;                       //Length of the data value in a package
    private static final int OFFSET_VALUE = 0x8000000;

    private int mNDataPointsReceived = 0;

    // Message types sent from the BluetoothConnectionService Handler
    public static final int MESSAGE_READ = 1;
    public static final int MESSAGE_SOCKET_CLOSED = 2;
    public static final int MESSAGE_SOCKET_CONNECTED = 3;
    public static final int MESSAGE_TOAST = 4;

    static List<Double> mCurrentReadings = new ArrayList<>();                      //Collection of current readings
    static List<Double> mVoltageReadings = new ArrayList<>();                      //Collection of potential readings

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
    private boolean mIsDeviceVerified = false;
    private boolean mIsScriptActive = false;
    private boolean mIsScanning;
    private final CallBackHandler mHandler = new CallBackHandler(this);

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
     * The beginning characters of the measurement response to help parsing the response
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

    //region Bluetooth broadcast receivers

    /**
     * Broadcast Receiver for listing paired devices upon on/off bluetooth.
     * When the bluetooth state is ON, calls the method to find the list of paired bluetooth devices.
     */
    private final BroadcastReceiver mBroadcastReceiverBTState = new BroadcastReceiver() {
        public void onReceive(Context context, Intent intent) {
            String action = intent.getAction();

            if (BluetoothAdapter.ACTION_STATE_CHANGED.equals(action)) {                                //When Bluetooth state is changed
                final int state = intent.getIntExtra(BluetoothAdapter.EXTRA_STATE, BluetoothAdapter.ERROR);

                switch (state) {
                    case BluetoothAdapter.STATE_ON:                                                    //When bluetooth is turned ON
                        Log.d(TAG, "mBroadcastReceiverBTState: STATE ON");
                        findPairedDevices();
                        btnConnect.setEnabled(spinnerConnectedDevices.getAdapter() != null && !spinnerConnectedDevices.getAdapter().isEmpty());
                        break;
                    case BluetoothAdapter.STATE_OFF:
                        Log.d(TAG, "mBroadcastReceiverBTState: STATE OFF");
                        onBluetoothOff();
                        break;
                }
                btnScan.setEnabled(true);
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
                if (device.getName() != null && device.getName().startsWith("PS") && !mBTDevices.contains(device))
                    mBTDevices.add(device);
                Log.d(TAG, "onReceive: " + device.getName() + ": " + device.getAddress());
            } else if (BluetoothAdapter.ACTION_DISCOVERY_FINISHED.equals(action)) {
                if(mBluetoothAdapter.isDiscovering())
                    mBluetoothAdapter.cancelDiscovery();                                                 //Cancels discovery when discovery is finished
                if (mIsScanning) {
                    Toast.makeText(context, "Bluetooth devices scan complete.", Toast.LENGTH_SHORT).show();
                    onScanStopped();
                }
            }
        }
    };
    //endregion

    /**
     * <Summary>
     *      Itemselected listener that creates a new bluetooth connection after verifying if the device is paired.
     * </Summary>
     * @param parent   The adapter of the spinnerConnectedDevices
     * @param view     spinner
     * @param position Position of the selected item in the array adapter
     * @param id       spinnerConnectedDevices
     */
    //@RequiresApi(api = Build.VERSION_CODES.KITKAT)
    @Override
    public void onItemSelected(AdapterView<?> parent, View view, int position, long id) {
        if(mBluetoothAdapter.isDiscovering())
            mBluetoothAdapter.cancelDiscovery();
        if (mIsScanning) onScanStopped();
        mSelectedDevice = mBTDevices.get(position);
    }

    @Override
    public void onNothingSelected(AdapterView<?> parent) {
        // Do nothing
    }

    /**
     * <Summary>
     *     The Handler that gets information back from the BluetoothConnectionService
     *     Used as a static class with a weak reference to the activity inorder to avoid possible memory leaks.
     * </Summary>
     */
    public static class CallBackHandler extends Handler {
        WeakReference<MSBluetoothActivity> mWeakRefActivity;

        CallBackHandler(MSBluetoothActivity act) {
            mWeakRefActivity = new WeakReference<>(act);
        }

        @Override
        public void handleMessage(Message msg) {
            MSBluetoothActivity activity = mWeakRefActivity.get();
            if (activity != null) {
                byte[] readBuff;
                String readMessage;
                switch (msg.what) {
                    case MESSAGE_READ:
                        readBuff = (byte[]) msg.obj;                                         //The message posted as bytes to the handler
                        readMessage = new String(readBuff);                                  //Constructs a string from the valid bytes in the buffer
                        activity.processResponse(readMessage);
                        break;

                    case MESSAGE_SOCKET_CLOSED:
                        readBuff = (byte[]) msg.obj;
                        readMessage = new String(readBuff);
                        Toast.makeText(activity, readMessage, Toast.LENGTH_SHORT).show();   //Shows a toast message if socket is closed and connection fails
                        activity.updateView();
                        break;

                    case MESSAGE_SOCKET_CONNECTED:
                        activity.sendVersionCmd();                                           //Sends the version command once the bluetooth socket is connected
                        break;

                    case MESSAGE_TOAST:
                        readBuff = (byte[]) msg.obj;
                        readMessage = new String(readBuff);
                        Toast.makeText(activity, readMessage, Toast.LENGTH_SHORT).show();
                        activity.updateView();
                        break;
                }//end switch
            }
        }
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
        spinnerConnectedDevices.setOnItemSelectedListener(this);                                //Spinner click listener

        mBTDevices = new ArrayList<>();
        mBluetoothAdapter = BluetoothAdapter.getDefaultAdapter();

        IntentFilter BTIntent = new IntentFilter(BluetoothAdapter.ACTION_STATE_CHANGED);
        registerReceiver(mBroadcastReceiverBTState, BTIntent);                                  //Registers the broadcast receiver to receive bluetooth action state changed

        IntentFilter discoverDevicesIntent = new IntentFilter(BluetoothDevice.ACTION_FOUND);    //Broadcasts when device is found
        discoverDevicesIntent.addAction(BluetoothAdapter.ACTION_DISCOVERY_FINISHED);            //Broadcasts when discovery is finished
        registerReceiver(mBroadcastReceiverDiscovery, discoverDevicesIntent);

        enableBluetooth();                                                                     //Method call to turn on bluetooth
        updateView();
    }

    /**
     * <Summary>
     *      Unregisters all the broadcast receivers to enable garbage collection.
     * </Summary>
     */
    @Override
    protected void onDestroy() {
        Log.d(TAG, "onDestroy: called.");
        super.onDestroy();
        unregisterReceiver(mBroadcastReceiverBTState);
        unregisterReceiver(mBroadcastReceiverDiscovery);
    }

    /**
     * <Summary>
     *      Checks if bluetooth is turned on, else raises a request to turn on bluetooth and finds the paired devices.
     * </Summary>
     */
    private void enableBluetooth() {
        if (mBluetoothAdapter == null) {
            Log.d(TAG, "enableBluetooth: Does not have BT capabilities.");
            return;                                                                          //Returns if device doesn't support bluetooth
        }
        if (!mBluetoothAdapter.isEnabled()) {
            Log.d(TAG, "enableBluetooth: enabling BT.");                               //Starts the activity to request turning on bluetooth
            BluetoothAdapter.getDefaultAdapter().enable();

        } else {
            Log.d(TAG, "BT enabled already");
            findPairedDevices();                                                             //Finds paired devices if bluetooth is enabled already
        }
    }

    /**
     * <Summary>
     *      Clears the spinner and updates UI if bluetooth connection is lost.
     * </Summary>
     */
    private void onBluetoothOff() {
        Toast.makeText(this, "Lost connection. Bluetooth is disabled.", Toast.LENGTH_SHORT).show();
        mBTDevices.clear();
        spinnerConnectedDevices.setAdapter(null);
        mIsConnected = false;
        mIsScriptActive = false;
        updateView();
    }

    /**
     * <Summary>
     *      Returns a list of the names of all connected devices.
     * </Summary>
     *
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
     *      Finds and adds the paired bluetooth devices to the spinner adapter.
     * </Summary>
     */
    private void findPairedDevices() {
        Set<BluetoothDevice> pairedDevices = BluetoothAdapter.getDefaultAdapter().getBondedDevices();   //Gets the list of paired/bonded devices
        if (pairedDevices.size() > 0) {
            for (BluetoothDevice d : pairedDevices) {
                Log.i(TAG, "paired device: " + d.getName());
                mBTDevices.add(d);                                                                      //Adds the paired device to mBTDevices arraylist
            }
            mAdapterConnectedDevices = new ArrayAdapter<>(this, android.R.layout.simple_spinner_dropdown_item, GetDeviceNames(mBTDevices));
            spinnerConnectedDevices.setAdapter(mAdapterConnectedDevices);
        }
    }

    /**
     * <Summary>
     *      The bluetooth adapter starts discovering unpaired devices. Device found and Discovery finished actions are received and handled by the mBroadcastReceiverDiscovery
     * </Summary>
     */
    @RequiresApi(api = Build.VERSION_CODES.M)
    public void discoverUnpairedDevices() {
        Log.d(TAG, "Looking for unpaired devices.");

        if (mBluetoothAdapter.isDiscovering()) {
            mBluetoothAdapter.cancelDiscovery();                                                //Cancels discovery if it is already discovering
        }

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) {
            checkBTPermissions();                                                               //Bluetooth permissions are to be specified in the manifest file. Needs runtime permissions on SDK >= 23 (M)
        }
        discover();
    }

    /**
     * <Summary>
     *      Clears the list of bluetooth devices array and starts a new bluetooth discovery.
     * </Summary>
     */
    private void discover() {
        mBTDevices.clear();                                                                     //Clears the connected devices list to remove devices not in range
        mProgressScan.setVisibility(View.VISIBLE);
        mBluetoothAdapter.startDiscovery();                                                     //Starts discovering nearby bluetooth devices
    }

    /**
     * <Summary>
     *     Pairs the device if not paired already.
     * </Summary>
     * @return True if the device is paired already.
     */
    @RequiresApi(api = Build.VERSION_CODES.KITKAT)
    private boolean verifyPairing(){
        if ((Build.VERSION.SDK_INT > Build.VERSION_CODES.KITKAT) && (mSelectedDevice.getBondState() != BluetoothDevice.BOND_BONDED)) {
            Log.d(TAG, "Trying to pair with " + mSelectedDevice.getName() + " device address:" + mSelectedDevice.getAddress());
            btnConnect.setEnabled(false);
            try {
                mSelectedDevice.createBond();                                   //Creates the bond if it is not paired already, response is received by the broadcastReceiverBondState
                return false;
            } catch (Exception e) {
                Log.d(TAG, e.getMessage() + mSelectedDevice.getName() + " device address:" + mSelectedDevice.getAddress());
            }
        }
        return true;
    }

    @Override
    public void onRequestPermissionsResult(int requestCode, @NonNull String[] permissions, @NonNull int[] grantResults) {
        super.onRequestPermissionsResult(requestCode, permissions, grantResults);
        AlertDialog alertDialog;
        final Activity activity = this;
        switch (requestCode) {
            case REQUEST_CODE_LOCATION:
                boolean isGranted = true;
                if (grantResults.length > 0) {
                    for (int i : grantResults) {
                        if (i != PackageManager.PERMISSION_GRANTED) {
                            isGranted = false;
                            break;
                        }
                    }
                }

                if (isGranted)                  //Location permission has been granted, okay to retrieve the location of the device.
                {
                    discover();                 //Discovers nearyby bluetooth devices
                }

                // User selected the Never Ask Again Option, alerts to change settings in app settings manually
                else if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M && !shouldShowRequestPermissionRationale(permissions[0]) && !shouldShowRequestPermissionRationale(permissions[1])) {
                    AlertDialog.Builder alertDialogBuilder = new AlertDialog.Builder(this);
                    alertDialogBuilder.setTitle("Change permissions in Settings");
                    alertDialogBuilder
                            .setMessage("Location permission is necessary.\n" + "Please go to Application settings to manually set permissions")
                            .setCancelable(false)
                            .setPositiveButton("EXIT",
                                    new DialogInterface.OnClickListener() {
                                        @Override
                                        public void onClick(DialogInterface dialog, int which) {
                                            MSBluetoothActivity.this.finish();
                                        }
                                    });
                    alertDialog = alertDialogBuilder.create();
                    alertDialog.show();
                } else {
                    // User selected Deny Dialog to EXIT App ==> OR <== RETRY to have a second chance to Allow Permissions
                    if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M && checkSelfPermission(Manifest.permission_group.LOCATION) == PackageManager.PERMISSION_DENIED) {
                        AlertDialog.Builder alertDialogBuilder = new AlertDialog.Builder(this);
                        alertDialogBuilder.setTitle("Permission required");
                        alertDialogBuilder
                                .setMessage("Location permission is necessary to detect device.\n" + "Click RETRY to set permissions\n" + "Click EXIT to close the App")
                                .setCancelable(false)
                                .setPositiveButton("RETRY",
                                        new DialogInterface.OnClickListener() {
                                            @Override
                                            public void onClick(DialogInterface dialog, int which) {
                                                ActivityCompat.requestPermissions(activity, new String[]{Manifest.permission.ACCESS_FINE_LOCATION, Manifest.permission.ACCESS_COARSE_LOCATION}, REQUEST_CODE_LOCATION);
                                            }
                                        })
                                .setNegativeButton("EXIT",
                                        new DialogInterface.OnClickListener() {
                                            @Override
                                            public void onClick(DialogInterface dialog, int which) {
                                                MSBluetoothActivity.this.finish();
                                            }
                                        });
                        alertDialog = alertDialogBuilder.create();
                        alertDialog.show();
                    }
                }
                break;

            default:
                break;
        }
    }

    /**
     * <Summary>
     *      This method is required for all devices running API23+
     *      Android must programmatically check the permissions for bluetooth. Adding the proper permissions in manifest is not enough.
     *      NOTE: This will only execute on versions > LOLLIPOP because it is not needed otherwise.
     * </Summary>
     */
    @RequiresApi(api = Build.VERSION_CODES.M)
    private void checkBTPermissions() {
        if (checkSelfPermission(Manifest.permission_group.LOCATION) == PackageManager.PERMISSION_DENIED) {
            this.requestPermissions(new String[]{Manifest.permission.ACCESS_FINE_LOCATION, Manifest.permission.ACCESS_COARSE_LOCATION}, REQUEST_CODE_LOCATION);
        }
    }

    /**
     * <Summary>
     *      Enables/disables the buttons and updates the UI
     * </Summary>
     */
    private void updateView() {
        btnConnect.setEnabled(mBluetoothAdapter.isEnabled() &&
                (spinnerConnectedDevices.getAdapter() != null && !spinnerConnectedDevices.getAdapter().isEmpty()));
        btnConnect.setText(mIsConnected ? "Disconnect" : "Connect");
        btnScan.setText(mIsScanning ? "Cancel" : "Scan");
        btnSend.setEnabled(mIsConnected && !mIsScriptActive);
        spinnerConnectedDevices.setEnabled(!mIsConnected);
        btnAbort.setEnabled(mIsScriptActive);
        btnScan.setEnabled(!mIsConnected);
        mProgressConnect.setVisibility(View.INVISIBLE);
    }

    /**
     * <Summary>
     *      Creates a new bluetooth connection service object and assigns the handler for call back to handle message postings to update the UI
     * </Summary>
     */
    private void createBluetoothConnection() {
        Toast.makeText(this, "Connecting to device....", Toast.LENGTH_SHORT).show();
        ParcelUuid[] uuid = mSelectedDevice.getUuids();
        mSelectedUUID = uuid != null ? uuid[0].getUuid() : DEFAULT_UUID;
        if(mBluetoothConnection != null && mBluetoothConnection.mIsDeviceConnected)
            return;
        mProgressConnect.setVisibility(View.VISIBLE);
        mBluetoothConnection = new BluetoothConnectionService(MSBluetoothActivity.this, mHandler);
        startBTConnection();
    }

    /**
     * <Summary>
     *      Starts a bluetooth connection (Opens a bluetooth socket and prepares the in stream and out stream for data)
     * </Summary>
     */
    public void startBTConnection() {
        Log.d(TAG, "startBTConnection: Initializing RFCOM Bluetooth Connection.");
        mBluetoothConnection.startClient(mSelectedDevice, mSelectedUUID);
    }

    /**
     * <Summary>
     *      Sends the version command to verify if the connected device is EmStat Pico.
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
     *      Verifies if the device connected is EmStat Pico by checking if the version response contains "esp"
     * </Summary>
     * @param versionString The response string to be verified.
     */
    private void verifyEmstatPico(String versionString) {
        if (versionString.contains("espico")) {
            Toast.makeText(this, "Connected to EmStatPico.", Toast.LENGTH_SHORT).show();
            spinnerConnectedDevices.setEnabled(false);
            mIsConnected = true;
        } else {
            Toast.makeText(this, "Failed to connect.", Toast.LENGTH_SHORT).show();
            mIsConnected = false;
            disconnect();
        }
        mIsDeviceVerified = true;
        updateView();
    }

    /**
     * <Summary>
     *      Reads from the script file and writes it on the device using the bluetooth socket's out stream.
     * </Summary>
     * @return A boolean to indicate if the MethodSCRIPT was sent successfully to the device.
     */
    public boolean sendScriptFile() {
        boolean isScriptSent = false;
        mIsVersionCmdSent = false;
        String line;
        BufferedReader bufferedReader = null;
        try {
            bufferedReader = new BufferedReader(new InputStreamReader(getAssets().open(SCRIPT_FILE_NAME)));
            while ((line = bufferedReader.readLine()) != null) {
                line += "\n";
                mBluetoothConnection.write(line.getBytes());
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
     *      Aborts the MethodSCRIPT by sending the command to abort (CMD_ABORT)
     * </Summary>
     */
    private void abortScript() {
        mBluetoothConnection.write(CMD_ABORT.getBytes());
        mIsScriptActive = false;
        btnSend.setEnabled(mIsConnected);
    }

    /**
     * <Summary>
     *      Trigerred when the bluetooth scanning is finished.
     *      Enables the UI elements to start a new scan for bluetooth devices
     * </Summary>
     */
    private void onScanStopped() {
        mIsScanning = false;
        if (mBluetoothAdapter.isDiscovering())
            mBluetoothAdapter.cancelDiscovery();                  // Cancels discovery if still discovering
        mAdapterConnectedDevices = new ArrayAdapter<>(this, android.R.layout.simple_spinner_dropdown_item, GetDeviceNames(mBTDevices));
        spinnerConnectedDevices.setAdapter(mAdapterConnectedDevices);
        mProgressScan.setVisibility(View.INVISIBLE);
        updateView();
    }

    /**
     * <Summary>
     *      Trigerred on click of disconnect button.
     *      Aborts active script if any and disconnects the bluetooth connection.
     *      Enables the UI elements to start a new scan for bluetooth devices
     * </Summary>
     */
    private void disconnect() {
        mIsConnected = false;
        if (mIsScriptActive)
            abortScript();                                              //Aborts any running script upon disconnection
        if(mBluetoothConnection != null)
        {
            mBluetoothConnection.disconnect();                          //Disconnects the device
            mBluetoothConnection = null;
        }
        spinnerConnectedDevices.setEnabled(true);
        updateView();                                                   //Updates the UI (enable/disable buttons)
    }

    /**
     * <Summary>
     *      Processes the measurement packages from the device.
     * </Summary>
     * @param line A complete line of response read from the device.
     */
    private void processResponse(String line) {
        char beginChar = line.charAt(0);
        if (mIsVersionCmdSent)                                         //Identifies if it is the response to version command
        {
            if (line.contains("*\n"))
                return;
            if(!mIsDeviceVerified) verifyEmstatPico(line);             //Calls the verifyEmStatPico method to verify if the device is EmStat Pico
        } else {
            if (beginChar == 'e')
                line = line.substring(1);                              //Removes the beginning character of the response if it is 'e'
            if (mIsConnected && processReceivedPackage(line)) {        //Calls the method to process the received measurement package
                btnSend.setEnabled(true);                              //Updates the UI upon completion of parsing and displaying of the response
                mIsScriptActive = false;
                btnAbort.setEnabled(false);
            }
        }
    }

    /**
     * <Summary>
     *      Processes the measurement package from the EmStat Pico and stores the response in RawData.
     * </Summary>
     * @param readLine A measurement package read from the device.
     * @return A boolean to indicate if measurement is complete.
     */
    private boolean processReceivedPackage(String readLine) {
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
     *      Parses a measurement data package and adds the parsed data values to their corresponding arrays
     * </summary>
     * @param packageLine The measurement data package to be parsed
     */
    private void parsePackageLine(String packageLine) {
        String[] variables;
        String variableIdentifier;
        String dataValue;

        int startingIndex = packageLine.indexOf('P');                            //Identifies the beginning of the package
        String responsePackageLine = packageLine.substring(startingIndex + 1);   //Removes the beginning character 'P'

        startingIndex = 0;
        txtResponse.append(String.format(Locale.getDefault(), "%n  %-4d", mNDataPointsReceived));
        Log.d(TAG, String.valueOf(mNDataPointsReceived) + "  " + responsePackageLine);

        variables = responsePackageLine.split(";");                       //The data values are separated by the delimiter ';'
        for (String variable : variables) {
            variableIdentifier = variable.substring(startingIndex, 2);             //The String (2 characters) that identifies the measurement variable
            dataValue = variable.substring(startingIndex + 2, startingIndex + 2 + PACKAGE_DATA_VALUE_LENGTH);
            double dataValueWithPrefix = parseParamValues(dataValue);          //Parses the data values and returns the actual values with their corresponding SI unit prefixes
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
     *      Parses the measurement data values and appends their respective prefixes
     * </Summary>
     * @param paramValueString The data value package value to be parsed
     * @return The actual data value (double) after appending the unit prefix
     */
    private double parseParamValues(String paramValueString) {
        char strUnitPrefix = paramValueString.charAt(7);                         //Identifies the SI unit prefix from the package at position 8
        String strvalue = paramValueString.substring(0, 7);                      //Retrieves the value of the variable from the package
        int value = Integer.parseInt(strvalue, 16);                        //Converts the hex value to int
        double paramValue = value - OFFSET_VALUE;                                //Values offset to receive only positive values
        return (paramValue * SI_PREFIX_FACTORS.get(strUnitPrefix));              //Returns the actual data value after appending the SI unit prefix
    }

    /**
     * <Summary>
     *      Parses the meta data values of the variable, if any.
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
        metaDataValues = packageMetaData.split(",");                        //The metadata values are separated by the delimiter ','
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
        long statusBits = (Integer.parseInt(String.valueOf(metaDatastatus.charAt(1)), 16));     //One char of the metadata value corresponding to status is retrieved
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
     *      Parses the bytes corresponding to current range from the package.
     * </Summary>
     * @param metaDataCR The current range metadata value to be parsed
     * @return The current range byte value
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
     *      Calls the method to send the MethodSCRIPT.
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
            Toast.makeText(this, "Error sending MethodSCRIPT", Toast.LENGTH_SHORT).show();
            btnSend.setEnabled(true);
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
        Toast.makeText(this, "Measurement aborted.", Toast.LENGTH_SHORT).show();
    }

    /**
     * <Summary>
     *      Calls the method to discover unpaired devices. Also registers the broadcast receiver to receive bluetooth bond state changes
     * </Summary>
     * @param view btnScan
     */
    @RequiresApi(api = Build.VERSION_CODES.M)
    public void onClickScan(View view) {
        if (!mIsScanning) {
            btnConnect.setEnabled(false);
            spinnerConnectedDevices.setEnabled(false);
            mIsScanning = true;
            btnScan.setText("Cancel");
            if (!mBluetoothAdapter.isEnabled()) {
                Log.d(TAG, "enableBluetooth: enabling BT.");             //Starts the activity to request turning on bluetooth
                BluetoothAdapter.getDefaultAdapter().enable();
            }
            discoverUnpairedDevices();                                        //Discovers unpaired devices
        } else {
            onScanStopped();                                                  //Cancels scanning
        }
    }

    /**
     * <Summary>
     *      Creates a new bluetooth connection with the selected device if it is not already connected. Else disconnects the device.
     * </Summary>
     * @param view btnConnect
     */
    @RequiresApi(api = Build.VERSION_CODES.KITKAT)
    public void onClickConnect(View view) {
        mIsDeviceVerified = false;
        btnConnect.setEnabled(false);
        btnScan.setEnabled(false);
        spinnerConnectedDevices.setEnabled(false);
        if (!mIsConnected) {
                createBluetoothConnection();                                 //Creates a new bluetooth connection service object
        } else {
            disconnect();                                                    //Disconnects the device
            Toast.makeText(this, "Device is disconnected.", Toast.LENGTH_SHORT).show();
        }
    }
    //endregion

}
