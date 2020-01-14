/*
 * SerialPort.h
 *
 *  Created on: 9 Jan 2020
 *      Author: Robert
 */

#ifndef SERIALPORT_H_
#define SERIALPORT_H_


/// Opens the serial port to which Emstat pico is connected.
/// Returns: 1 on successful connection, 0 in case of failure.
int OpenSerialPort();

/// Writes the input character to the device
/// Returns: 1 if data is written successfully, 0 in case of failure.
int WriteToDevice(char c);

/// Reads a character read from the EmStat Pico
/// Returns: -1 on failure or the value of the received byte on success
int ReadFromDevice();

/// Closes the serial port
/// Returns: 1 if closed successfully, 0 in case of failure.
int CloseSerialPort();



#endif /* SERIALPORT_H_ */
