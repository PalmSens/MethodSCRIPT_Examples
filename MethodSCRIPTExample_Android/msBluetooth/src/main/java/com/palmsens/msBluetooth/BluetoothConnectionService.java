package com.palmsens.msBluetooth;

import android.bluetooth.BluetoothAdapter;
import android.bluetooth.BluetoothDevice;
import android.bluetooth.BluetoothSocket;
import android.content.Context;
import android.util.Log;

import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.nio.charset.Charset;
import java.util.UUID;

public class BluetoothConnectionService {
    private static final String TAG = "BTConnectionService";
    private final BluetoothAdapter mBluetoothAdapter;
    private final MSBluetoothActivity.CallBackHandler mHandler;
    private boolean mThreadisStopped;

    private BluetoothDevice mDevice;
    private UUID mUUID;
    private BluetoothSocket mSocket;
    private InputStream mInStream;
    private OutputStream mOutStream;

    public boolean mIsDeviceConnected = false;
    /**
     * <Summary>
     *      A thread implementing the Runnable to connect to a device, listen to the response continuously and post the messages back to the main activity.
     *      Creates a RfcommSocket and connects to the bluetooth device using that socket.
     *      The input and output memory streams of this socket is used to perform read and write operations on the device.
     *      If the device is disconnected, the thread is stopped and socket is closed.
     * </Summary>
     */
    private Runnable connectedThread = new Runnable() {

        StringBuilder mReadLine = new StringBuilder();

        /**
         * <Summary>
         *     Creates a bluetooth socket connection
         * </Summary>
         * @return True if device is connected successfully
         */
        private boolean connectDevice() {
            mIsDeviceConnected = false;

            try {
                Log.d(TAG, "ConnectedThread: Trying to create InsecureRfcomSocket using UUID: " + mDevice.toString() + mUUID);
                mSocket = mDevice.createInsecureRfcommSocketToServiceRecord(mUUID);
            } catch (IOException e) {
                Log.e(TAG, "ConnectedThread: Could not create InsecureRfcomSocket " + e.getMessage());
                return false;
            }

            if(mBluetoothAdapter.isDiscovering())
                mBluetoothAdapter.cancelDiscovery();                                    //Always cancel discovery because it will slow down a connection

            try {
                // This is a blocking call and will only return on successful connection or an exception, hence in a runnable
                mSocket.connect();
                mIsDeviceConnected = true;
                onSocketConnected();
            } catch (IOException e) {
                try {
                    mSocket.close();                                                    //Closes the socket
                    mIsDeviceConnected = false;
                    Log.d(TAG, "run: Closed Socket.");
                    mHandler.obtainMessage(MSBluetoothActivity.MESSAGE_SOCKET_CLOSED, -1, -1, "Could not connect to device".getBytes())
                            .sendToTarget();
                } catch (IOException e1) {
                    Log.e(TAG, "ConnectedThread: run: Unable to close connection in socket " + e1.getMessage());
                }
                Log.d(TAG, "run: ConnectedThread: Could not connect to Device: ");
            }
            return mIsDeviceConnected;
        }

        /**
         * <Summary>
         *     Fetches the input/output streams from the socket and sends the message to UI thread upon successful connection.
         * </Summary>
         * @throws IOException
         */
        public void onSocketConnected() throws IOException {
            try{
                mIsDeviceConnected = true;
                mInStream = mSocket.getInputStream();
                mOutStream = mSocket.getOutputStream();
                mHandler.obtainMessage(MSBluetoothActivity.MESSAGE_SOCKET_CONNECTED, -1, -1, "Connected to device".getBytes())
                        .sendToTarget();
                Log.d(TAG, "run: ConnectedThread connected.");
            }
            catch (IOException e){
                throw new IOException(e);
            }
        }

        @Override
        public void run() {
            int offset = 0;
            int readSize;                                                                         //bytes returned from read
            String rchar;
            byte[] rbuf = new byte[1];

            if (connectDevice()) {
                mThreadisStopped = false;
                //Keeps listening to the InputStream until an exception occurs
                while (!mThreadisStopped) {
                    try {
                        if (mInStream != null && mInStream.available() > 0) {
                            readSize = mInStream.read(rbuf, offset, 1);                     //Reads a character from the socket in stream
                            if (readSize > 0) {
                                rchar = new String(rbuf);
                                mReadLine.append(rchar);                                         //A line of response package is formed until new line is encountered

                                if (rchar.equals("\n")) {
                                    mHandler.post(new Runnable() {
                                        final String line = mReadLine.toString();

                                        @Override
                                        public void run() {
                                            //Sends the obtained bytes to the UI Activity
                                            mHandler.obtainMessage(MSBluetoothActivity.MESSAGE_READ, -1, -1, line.getBytes())
                                                    .sendToTarget();
                                        }
                                    });
                                    mReadLine = new StringBuilder();
                                }
                            }
                        }
                    } catch (IOException e) {
                        mHandler.obtainMessage(MSBluetoothActivity.MESSAGE_TOAST, -1, -1, "Error reading input stream".getBytes())
                                .sendToTarget();
                        break;
                    }
                }
                closeSocket();                                                                 //Closes the socket upon disconnecting the device
            }
        }
    };

    /**
     * <Summary>
     *     Initializes the BluetoothConnectionService object
     * </Summary>
     * @param context The main activity (MSBluetoothActivity)
     * @param handler The handler to post messages back to the main activity
     */
    public BluetoothConnectionService(Context context, MSBluetoothActivity.CallBackHandler handler) {
        mBluetoothAdapter = BluetoothAdapter.getDefaultAdapter();
        mHandler = handler;                                                                    //The handler to post back messages to the main activity
    }

    /**
     * <Summary>
     *      Called from the MSBluetoothActivity on click of button Connect.
     *      Sets the selected device and the UUID of the device and starts a new runnable to connect to the device.
     * </Summary>
     * @param device The selected bluetooth device
     * @param uuid   The UUID of the selected device
     */
    public void startClient(BluetoothDevice device, UUID uuid) {
        Log.d(TAG, "startClient: Started.");
        mDevice = device;
        mUUID = uuid;
        new Thread(connectedThread).start();                                                   //Starts a new runnable thread to connect to the device
    }

    /**
     * <Summary>
     *      Called from the MSBluetoothActivity on click of Disconnect.
     *      Marks that the thread is stopped to enable closing of the socket from within the runnable thread (connectedThread)
     * </Summary>
     */
    public void disconnect() {
        mThreadisStopped = true;
    }

    /**
     * <Summary>
     *      Closes the socket and sets the instream and out stream to null to shut down the connection when the device is disconnected.
     * </Summary>
     */
    private void closeSocket() {
        try {
            mSocket.close();
        } catch (IOException e) {
            Log.d(TAG, "closeSocket: " + e.getMessage());
        }
    }

    /**
     * <Summary>
     *      Called from the MSBluetoothActivity for sending the version command and sending the MethodSCRIPT.
     *      Writes to the output stream of the bluetooth socket.
     * </Summary>
     * @param bytes The data (bytes) to be written to the output stream
     */
    public void write(byte[] bytes) {
        String text = new String(bytes, Charset.defaultCharset());
        Log.d(TAG, "write: Writing to outputstream: " + text);
        try {
            mOutStream.write(bytes);
        } catch (IOException e) {
            mHandler.obtainMessage(MSBluetoothActivity.MESSAGE_TOAST, -1, -1, "Error writing to output stream".getBytes())
                    .sendToTarget();
        }
    }
}
























