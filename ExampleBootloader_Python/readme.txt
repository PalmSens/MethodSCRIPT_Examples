This example can be used to upload firmware on any of the following PalmSens devices:
- EmStat4
- EmStat Pico
- Nexus

Only serial port connections (generally Serial USB Converters) are supported. 
To add another type of port, copy and modify the file "fw_upload_serial_port.py".

The device must be already in bootloader mode before running the bootloader example.
On most PalmSens devices, this can be done either by pulling the DWNLD pin low, or sending the "dlfw\n" command.
