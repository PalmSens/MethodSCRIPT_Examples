import Foundation
import UIKit
import CoreBluetooth
import Combine
import os

open class BLEConnection: NSObject, CBPeripheralDelegate, CBCentralManagerDelegate, ObservableObject {

    override init() {
        self.scannedDevices = []

        super.init()
        self.centralManager = CBCentralManager(delegate: self, queue: nil)
    }

    // Constants
    public static let dataValueLength = 10
    public static let dataValueUnitPosition = dataValueLength - 1
    public static let dataValueStartPosition = dataValueLength - 8
    public static let dataValueEndPosition = dataValueLength - 2

    public static let maximumNumberOfBytesAllowedToSend = 20

    public static let newLineCharacter: Character = "\n"
    public static let commandVersion = "t\n"
    public static let commandAbort = "Z\n";
    public static let unsignedToSignedOffset = 134217728; //Half of the 28 bit number, hex: 0x8000000

    public static let vspServiceUUID = CBUUID.init(string: "569a1101-b87f-490c-92cb-11ba5ea5167c")
    public static let receiveFromVSPCharacteristicUUID = CBUUID(string: "569a2000-b87f-490c-92cb-11ba5ea5167c")
    public static let writeToVSPCharacteristicUUID = CBUUID(string: "569a2001-b87f-490c-92cb-11ba5ea5167c")

    // Dictionaries
    let conversionFactors: [String: Double] = [
        "a": 1e-18,
        "f": 1e-15,
        "p": 1e-12,
        "n": 1e-9,
        "u": 1e-6,
        "m": 1e-3,
        " ": 1.0,
        "k": 1e3,
        "M": 1e6,
        "G": 1e9,
        "T": 1e12,
        "P": 1e15,
        "E": 1e18
    ]

    // Publishers
    @Published var bleState = BLEState.idle
    @Published var scannedDevices: [Device]
    @Published var connectedDevice = Device(nil, nil, nil)
    @Published var messages = [String]()
    @Published var numberOfSuccessfullyParsedDataPoints = 0
    @Published var numberOfFailedParsedDataPoints = 0
    @Published var measurements: [Measurement] = []

    // Properties
    var startTimeMeasurements: CFAbsoluteTime = CFAbsoluteTime()
    var endTimeMeasurements: CFAbsoluteTime = CFAbsoluteTime()
    private var centralManager: CBCentralManager! = nil
    weak var writeCharacteristic: CBCharacteristic?
    private var writeType: CBCharacteristicWriteType = .withResponse
    private var buffer: String = ""

    var isReady: Bool {
        get {
            guard self.centralManager != nil else {
                Log.w("Guard failure")
                return false
            }
            return centralManager.state == .poweredOn &&
                    connectedDevice.peripheral != nil &&
                    writeCharacteristic != nil
        }
    }

    // Whether this serial is looking for advertising peripherals
    var isScanning: Bool {
        get {
            guard self.centralManager != nil else {
                Log.w("Guard failure")
                return false
            }
            return self.centralManager.isScanning
        }
    }

    // Whether the state of the centralManager is .poweredOn
    var isPoweredOn: Bool {
        get {
            guard self.centralManager != nil else {
                Log.w("Guard failure")
                return false
            }
            return self.centralManager.state == .poweredOn
        }
    }

    func startScanningForDevices() {
        guard isPoweredOn else {
            Log.w("Guard failure")
            return
        }

        bleState = .scanning
        self.centralManager.scanForPeripherals(withServices: [BLEConnection.vspServiceUUID], options: [CBCentralManagerScanOptionAllowDuplicatesKey: false]) //TIP: Embed in a 'DispatchQueue.main.asyncAfter' when encountering problems with detecting devices
        Log.i("Now scanning for peripherals with service UUID: \(BLEConnection.vspServiceUUID)")
    }

    func stopScanningForDevices() {
        Log.i("Stopping scanning for devices...")
        guard isPoweredOn else {
            Log.w("Guard failure")
            return
        }

        if (self.bleState == .scanning) {
            self.bleState.reset()
        }
        self.centralManager.stopScan();
    }

    func disconnect() {
        Log.i("Disconnecting from the device started...")
        stopScanningForDevices()

        guard isPoweredOn else {
            Log.w("Guard failure")
            return

        }
        guard connectedDevice.peripheral != nil else {
            Log.w("Guard failure")
            return
        }

        centralManager.cancelPeripheralConnection(connectedDevice.peripheral!)
        bleState = .idle
    }

    func connectToDevice(device: Device) {
        Log.i("Connecting to the device started...")
        stopScanningForDevices()

        if let foundDevice = scannedDevices.first(where: { $0.id == device.id }) {
            self.connectedDevice.id = foundDevice.id;
            self.connectedDevice.name = foundDevice.name;
            self.connectedDevice.peripheral = foundDevice.peripheral
            self.connectedDevice.peripheral!.delegate = self
            self.centralManager.connect(self.connectedDevice.peripheral!, options: nil)
            Log.i("Connecting to device \(String(describing: device.name!)) (id: '\(String(describing: device.id!)))")
            return
        }

        Log.e("Could not connect to device with id '\(String(describing: device.id))' and name '\(String(describing: device.name!))', reason: device not found in array of scanned devices.")
    }

    //
    // MARK Read and Write Methods
    //

    func sendCommandVersion() {
        sendMessage(BLEConnection.commandVersion)
        bleState = .running
    }

    func sendCommandAbort() {
        sendMessage(BLEConnection.commandAbort)
        bleState = .connected
    }

    func sendMethodScript(methodScript: String) {
        numberOfSuccessfullyParsedDataPoints = 0
        numberOfFailedParsedDataPoints = 0
        sendMessage(methodScript)
        bleState = .running
    }

    /// Send a string to the device
    func sendMessage(_ message: String) {
        Log.i("Sending string message '\(message)' to the device started...")
        guard isReady else {
            Log.w("Guard failure")
            return
        }

        if let data = message.data(using: String.Encoding.ascii) {
            sendData(data)
        }
    }

    /// Send an array of bytes to the device
    func sendBytes(_ bytes: [UInt8]) {
        let bytesAsString = String(bytes: bytes, encoding: .ascii)
        Log.i("Sending bytes '\(bytesAsString!)' to the device started...")
        guard isReady else {
            Log.w("Guard failure")
            return
        }

        let data = Data(bytes: UnsafePointer<UInt8>(bytes), count: bytes.count)
        sendData(data)
    }

    /// Send data to the device
    private func sendData(_ data: Data) {
        guard isReady else {
            Log.w("Guard failure")
            return
        }

        let bytesToSend = data.count
        Log.i("Sending \(bytesToSend) of bytes started...")

        if bytesToSend <= BLEConnection.maximumNumberOfBytesAllowedToSend {
            connectedDevice.peripheral!.writeValue(data, for: writeCharacteristic!, type: writeType)
        } else {
            var from = 0
            while (from < bytesToSend) {
                var amountToSend = data.count - from
                if amountToSend > BLEConnection.maximumNumberOfBytesAllowedToSend {
                    amountToSend = BLEConnection.maximumNumberOfBytesAllowedToSend
                }
                let to = from + amountToSend
                let dataChunk = data.subdata(in: from..<to)
                connectedDevice.peripheral!.writeValue(dataChunk, for: writeCharacteristic!, type: writeType)
                from += amountToSend
            }
        }
    }

    // Reads data from device
    public func peripheral(_ peripheral: CBPeripheral, didUpdateValueFor characteristic: CBCharacteristic, error: Error?) {
        let data = characteristic.value
        guard data != nil else {
            Log.w("Guard failure")
            return
        }

        if let dataAsString = String(data: data!, encoding: String.Encoding.ascii) {

            processReceivedPackage(redPackage: dataAsString)

            //DEBUG OPTIONS:
            //Log.i("R: \(dataAsString.withEscapedNewlines)")
        } else {
            Log.e("Received an invalid string!")
        }

        //EXAMPLE: As bytes array
        //var bytes = [UInt8](repeating: 0, count: data!.count / MemoryLayout<UInt8>.size)
        //(data! as NSData).getBytes(&bytes, length: data!.count)
        //delegate?.serialDidReceiveBytes(bytes)
    }

    // Processes the measurement packages from the EmStat Pico and stores the response in RawData.
    private func processReceivedPackage(redPackage: String) {
        var isComplete = false

        buffer.append(redPackage)

        guard buffer.count > 0 else {
            return
        }

        repeat {
            let bufferline: String

            if let indexOfFirstNewLine = buffer.firstIndex(of: BLEConnection.newLineCharacter) {
                bufferline = String(buffer[..<indexOfFirstNewLine])

                if let indexAfterNewLine = buffer.index(indexOfFirstNewLine, offsetBy: 1, limitedBy: buffer.endIndex) {
                    buffer = String(buffer[indexAfterNewLine...])
                } else {
                    buffer = "" // Because there is not any content after the newline we can remove this newline
                }
            } else {
                return
            }

            guard bufferline.count > 0 else {
                return
            }

            //Log.i("Received from device: \(bufferline)")

            let firstCharacter = Character(String(bufferline.prefix(1)))
            if let reply = PackageType.asEnum(firstCharacter) {
                switch reply {
                case .emptyLine:
                    isComplete = true
                    break;
                case .startMeasuring:
                    startTimeMeasurements = CFAbsoluteTimeGetCurrent()
                    self.messages.insert("Starting measurements... '\(bufferline)", at: 0)
                    self.measurements.removeAll()
                    break;
                case .measurementPackage:
                    parsePackageLine(packageLine: bufferline)
                    break;
                case .endMeasuring:
                    endTimeMeasurements = CFAbsoluteTimeGetCurrent()
                    let timeElapsed = endTimeMeasurements - startTimeMeasurements
                    self.messages.insert("Measurement completed, successfully parsed \(numberOfSuccessfullyParsedDataPoints) data point(s) and failed parsing \(numberOfFailedParsedDataPoints) data point(s). Total process time \(String(format: "%.2f", timeElapsed)) seconds.", at: 0)
                    self.bleState = .connected
                    break;
                case .aborted:
                    self.messages.insert("Measurement aborted!", at: 0)
                    self.bleState = .connected
                    break;
                case .deviceVersion:
                    self.messages.insert("Device version: '\(bufferline)", at: 0)
                    bleState = .connected
                    break;
                case .methodScriptReceived:
                    self.messages.insert("Measurement initiated, waiting for packages...", at: 0)
                    break;
                case .error:
                    if (bufferline == "F!0003") {
                        self.messages.insert("Error occurred, processed \(numberOfSuccessfullyParsedDataPoints)", at: 0)
                        self.bleState = .connected
                    }
                    break;
                }
            }
        } while !isComplete
    }

    /**
     Parses a measurement package which contains a voltage reading a current reading and metadata.
     The structure is the first character always contains a 'P' followed by a voltage measurement (begins with 'da') and
     a current measurement (begins with 'ba'). The voltage an current measurement are separated by a semicolon.
     The last part of the package contains the metadata. The different metadata is seperated by comma's.

     An example of a measurement package is: Pda807A1CAu;ba6FFA74Ba,14,200
     */
    private func parsePackageLine(packageLine: String) {
        var message: String
        var failed = false
        var metadata: Metadata?
        var voltage: Double?
        var current: Double?

        let measurement = Measurement()

        let packageLineParsed = packageLine.dropFirst(1) // Remove the first character which is always the 'P' character
        let dataValues = packageLineParsed.components(separatedBy: ";");
        for dataValueString in dataValues {
            let dataValueStringParsed: String
            if (dataValueString.count > BLEConnection.dataValueLength) {
                let metadataValues = dataValueString.components(separatedBy: ",")
                dataValueStringParsed = metadataValues[0]

                metadata = parseMetadata(Array(metadataValues.dropFirst())) // Drop first item of the array which contains the voltage or current package (this is parsed below)
            } else {
                dataValueStringParsed = dataValueString
            }

            if let dataValue = parseDataValue(dataValueStringParsed) {
                let dataValuePrefix = dataValueStringParsed.prefix(2)
                switch dataValuePrefix {
                case "da":
                    voltage = dataValue
                    measurement.voltage = voltage
                    break;
                case "ba":
                    current = dataValue
                    measurement.current = current
                    break;
                default:
                    Log.e("Unknown data value prefix found, value: \(dataValuePrefix)")
                    failed = true
                    continue
                }
            } else {
                failed = true
            }
        }

        if (failed) {
            numberOfFailedParsedDataPoints = numberOfFailedParsedDataPoints + 1
            message = "Failed to parse measurement package \(packageLine)"
            Log.w(message)
            self.messages.insert(message, at: 0)
            return
        }

        numberOfSuccessfullyParsedDataPoints = numberOfSuccessfullyParsedDataPoints + 1
        measurement.id = numberOfSuccessfullyParsedDataPoints
        message = "Measurement[No: \(numberOfSuccessfullyParsedDataPoints), Voltage (V): \(String(format: "%.4f", voltage!)), Current (A): \(String(format: "%.16f", current!))"

        if (metadata == nil) {
            message = message + ", metadata (Reading Status, Current Range) not available"
        } else {
            measurement.readingStatus = ReadingStatus.asFriendlyString(readingStatus: metadata?.readingStatus)
            measurement.currentRange = CurrentRange.asFriendlyString(currentRange: metadata?.currentRange)

            message = message + ", Reading Status: \(measurement.readingStatus!), Current Range \(measurement.currentRange!))"
        }

        measurements.append(measurement)
        Log.i(message)
    }

    /**
    Parses a value which contains a voltage or a current. An example of a value is 'ba6FFA74Ba' which is a
    current measurement. The value is in hex format and starts at index 2 and end at 1 character before the unit. The unit
    is the last character. The hex value is an unsigned int which is converted to a signed int.

    - Returns: The value of the measurement. If nil then something has gone wrong while parsing the value.
    */
    private func parseDataValue(_ dataValueString: String) -> Double? {
        // Check length
        if (dataValueString.count != BLEConnection.dataValueLength) {
            Log.e("Data value length incorrect: \(dataValueString)")
            return nil
        }

        let dataValueHexed = dataValueString[BLEConnection.dataValueStartPosition...BLEConnection.dataValueEndPosition] // Example data value strings: da807A1CAu or ba6FFA74Ba
        if let dataValueUnHexed = Int(dataValueHexed, radix: 16) {
            let dataValueUnit = dataValueString[BLEConnection.dataValueUnitPosition]
            if let dataValueConversionFactor = conversionFactors[dataValueUnit] {
                let dataValue = Double(dataValueUnHexed - BLEConnection.unsignedToSignedOffset) * dataValueConversionFactor
                return dataValue
            } else {
                Log.e("Unknown conversion factor found, value: \(dataValueUnit)")
                return nil
            }
        } else {
            Log.e("Non hexed value found, value: \(dataValueHexed)")
            return nil
        }
    }

    /**
    Parses the metadata values of the variable. The first character in each meta data value specifies the type of data.
    If the value is 1 then it is then it contains the reading status.
    If the value is 2 then it contains the current range index. First bit high (0x80 = 128) indicates a high speed mode CR.
    If the value is 4 then it contains the noise value.
    */
    private func parseMetadata(_ metadataValues: [String]) -> Metadata {
        let metaData = Metadata()

        for metadataValue in metadataValues {
            if let metadataType = MetadataType.asEnum(String(metadataValue.prefix(1))) {
                switch metadataType {
                case .ReadingStatus:
                    metaData.readingStatus = parseReadingStatus(metadataValue)
                case .CurrentRange:
                    metaData.currentRange = parseCurrentRange(metadataValue)
                case .Noise:
                    metaData.noise = parseNoise(metadataValue)
                }
            } else {
                Log.e("Meta data contains unknown type, value: \(metadataValue)")
            }
        }

        return metaData
    }

    /**
     Parses the reading status from the metadata value. It has a hexadecimal format.
     If the value is 0x0 it is ok,
     If the value is 0x2 it is an overload
     If the value is 0x4 it is an underload
     If the value is 0x8 it is an overload warning (80% of max)
    */
    private func parseReadingStatus(_ metadataValue: String) -> ReadingStatus? {
        if let readStatusUnhexed = Int(metadataValue[1], radix: 16) {
            let readingStatus = ReadingStatus.asEnum(readStatusUnhexed)
            if readingStatus == nil {
                Log.e("Reading status not found, unhexed value: \(readStatusUnhexed)")
            }
            return readingStatus
        } else {
            Log.e("Non hexed value found for the reading status, value: \(metadataValue)")
        }

        return nil
    }

    /**
     Parses the current range from the metadata. It has a hexadecimal format.
    */
    private func parseCurrentRange(_ metadataValue: String) -> CurrentRange? {
        if let currentRangeUnhexed = Int(metadataValue[1...2], radix: 16) {
            let currentRange = CurrentRange.asEnum(currentRangeUnhexed)
            if currentRange == nil {
                Log.e("Current range not found, unhexed value: \(currentRangeUnhexed)")
            }
            return currentRange
        } else {
            Log.e("Non hexed value found for the current range, value: \(metadataValue)")
        }

        return nil
    }

    private func parseNoise(_ metadataValue: String) -> String {
        return "Not Implemented" //NOTE: Noise is deliberately not implemented (it is also not implemented in PSTrace)
    }

    //
    // MARK: Central Manager Methods
    //

    // Handles BT Turning On/Off
    public func centralManagerDidUpdateState(_ central: CBCentralManager) {
        Log.i("State update detected!")

        switch central.state {
        case .unsupported:
            Log.e("BLE is Unsupported")
            break
        case .unauthorized:
            Log.e("BLE is Unauthorized")
            break
        case .unknown:
            Log.e("BLE is Unknown")
            break
        case .resetting:
            Log.i("BLE is Resetting")
            break
        case .poweredOff:
            Log.i("BLE is Powered Off")
            break
        case .poweredOn:
            Log.i("BLE is Powered On")
            return
        @unknown default:
            Log.e("Unknown state found: '\(central.state)'")
        }

        connectedDevice.peripheral = nil
    }

    // Handles the result of the scan
    public func centralManager(_ central: CBCentralManager, didDiscover peripheral: CBPeripheral, advertisementData: [String: Any], rssi RSSI: NSNumber) {
        if (peripheral.name != nil) {
            Log.i("Found device \(String(describing: peripheral.name!)) (RSSI: \(String(RSSI.doubleValue)))")

            //NOTE: There is no specific event detecting when a device is out of range. You could apply a trick: implement some waiting mechanism which removes a device when it is not advertising anymore (setting 'CBCentralManagerScanOptionAllowDuplicatesKey' must be true then)
            self.scannedDevices.appendIfNotContains(Device(peripheral.identifier, peripheral.name!, peripheral))
        }
    }

    // Handles if we do connect successfully
    public func centralManager(_ central: CBCentralManager, didConnect peripheral: CBPeripheral) {
        guard self.connectedDevice.peripheral != nil else {
            Log.w("Guard failure")
            return
        }

        if peripheral == self.connectedDevice.peripheral {
            Log.i("Connected to device \(String(describing: peripheral.name!)), starting detecting characteristics...")
            peripheral.discoverServices([BLEConnection.vspServiceUUID])
            messages.insert("Connected to \(String(describing: self.connectedDevice.name!))", at: 0)
            bleState = .connected
            return
        }
        messages.insert("Failed to connect device \(String(describing: self.connectedDevice.name!))", at: 0)
        Log.e("Failed to connect device \(String(describing: peripheral.name!))")
    }

    // Handles a disconnect
    public func centralManager(_ central: CBCentralManager, didDisconnectPeripheral peripheral: CBPeripheral, error: Error?) {
        connectedDevice.peripheral = nil
        self.scannedDevices.removeAll(where: { $0.id == peripheral.identifier })
        self.messages.insert("Disconnected device", at: 0)

        self.bleState = .idle
        Log.i("Disconnected device \(String(describing: peripheral.name!))")
    }

    // Handles a connection failure
    public func centralManager(_ central: CBCentralManager, didFailToConnect peripheral: CBPeripheral, error: Error?) {
        connectedDevice.peripheral = nil
        self.scannedDevices.removeAll(where: { $0.id == peripheral.identifier })
        Log.e("Connection failure to device \(String(describing: peripheral.name!))")
    }

    // Handles discovery event
    public func peripheral(_ peripheral: CBPeripheral, didDiscoverServices error: Error?) {
        if let services = peripheral.services {
            for service in services {
                if service.uuid == BLEConnection.vspServiceUUID {
                    //Now kick off discovery of characteristics
                    peripheral.discoverCharacteristics([BLEConnection.receiveFromVSPCharacteristicUUID, BLEConnection.writeToVSPCharacteristicUUID], for: service)
                    Log.i("Discover services success!")
                    return
                }
            }
        }
        Log.e("Failure to discover the services!")
    }

    // Handles the discovery of characteristics
    public func peripheral(_ peripheral: CBPeripheral, didDiscoverCharacteristicsFor service: CBService, error: Error?) {
        var foundReadCharacteristic = false
        var foundWriteCharacteristic = false

        if let characteristics = service.characteristics {
            for characteristic in characteristics {
                if characteristic.uuid == BLEConnection.receiveFromVSPCharacteristicUUID {
                    peripheral.setNotifyValue(true, for: characteristic) // This enables calling the peripheral:didUpdateNotificationStateForCharacteristic:error method of its delegate object to indicate if the action succeeded. If successful, the peripheral then calls the peripheral:didUpdateValueForCharacteristic:error: method of its delegate object whenever the characteristic value changes.
                    foundReadCharacteristic = true
                    Log.i("Receive from VSP characteristic found")
                }
                if characteristic.uuid == BLEConnection.writeToVSPCharacteristicUUID {
                    // keep a reference to this characteristic so we can write to it
                    writeCharacteristic = characteristic
                    writeType = .withResponse //NOTE: If you want to write without a response use '.withoutResponse'. Unfortunately this does NOT work for the EmStat Pico when sending a methodscript

                    foundWriteCharacteristic = true
                    Log.i("Write to VSP characteristic found")
                }
            }
        }

        if (!foundReadCharacteristic) {
            Log.e("Read characteristic not found for UUID: \(BLEConnection.receiveFromVSPCharacteristicUUID)")
        }
        if (!foundWriteCharacteristic) {
            Log.e("Write characteristic not found for UUID: \(BLEConnection.writeToVSPCharacteristicUUID)")
        }

        messages.insert("Successfully red characteristics", at: 0)
        Log.i("Finished reading characteristics")
    }

    // Handles result of a write operation
    public func peripheral(_ peripheral: CBPeripheral, didWriteValueFor characteristic: CBCharacteristic, error: Error?) {
        if let error = error {
            Log.e("Error occurred while writing a value to the device, error: \(error.localizedDescription)")
        }
    }

}

//
// MARK: Helper Classes
//

// Device is in essence a wrapper around a CBPeripheral
class Device: ObservableObject, Identifiable, Equatable {
    var id: UUID?
    @Published var name: String?
    var peripheral: CBPeripheral?

    init(_ id: UUID?, _ name: String?, _ peripheral: CBPeripheral?) {
        self.id = id
        self.name = name
        self.peripheral = peripheral
    }

    static func ==(lhs: Device, rhs: Device) -> Bool {
        lhs.id == rhs.id
    }
}

class Metadata {
    var readingStatus: ReadingStatus?
    var currentRange: CurrentRange?
    var noise: String?
}

public class Measurement: Identifiable, Equatable {
    public var id: Int = 0
    public var voltage: Double?
    public var current: Double?
    public var readingStatus: String?
    public var currentRange: String?

    public static func ==(lhs: Measurement, rhs: Measurement) -> Bool {
        lhs.id == rhs.id
    }
}

//
// MARK: Enums
//

// State of the bluetooth connection
enum BLEState {
    case idle
    case scanning
    case connected
    case running
    case error

    //NOTE: The mutating functions 'next' and 'previous' below can be used to mimic a State Machine (not used yet in this solution)
    mutating func next() {
        switch self {
        case .idle: self = .scanning
        case .scanning: self = .connected
        case .connected: self = .running
        case .running: self = .connected
        case .error: self = .idle
        }
    }

    mutating func previous() {
        switch self {
        case .scanning: self = .idle
        case .connected: self = .idle
        case .running: self = .connected
        case .idle: self = .idle
        case .error: self = .idle
        }
    }

    mutating func reset() {
        self = .idle
    }
}

// The beginning characters of the measurement response packages to help parsing the response
enum PackageType: Character, CaseIterable {
    case deviceVersion = "t"
    case methodScriptReceived = "e"
    case startMeasuring = "M"
    case measurementPackage = "P"
    case endMeasuring = "*"
    case emptyLine = "\n"
    case aborted = "Z"
    case error = "F"

    static func asEnum(_ character: Character) -> PackageType? {
        PackageType.allCases.first {
            $0.rawValue == character
        }
    }
}

// Reading Status of a metadata value
enum ReadingStatus: Int, CaseIterable {
    case Ok = 0
    case Overload = 2
    case Underload = 4
    case OverloadWarning = 8

    static func asEnum(_ value: Int) -> ReadingStatus? {
        ReadingStatus.allCases.first {
            $0.rawValue == value
        }
    }

    static func asFriendlyString(readingStatus: ReadingStatus?) -> String {
        switch readingStatus {
        case .Ok:
            return "Ok"
        case .Overload:
            return "Overload"
        case .Underload:
            return "Underload"
        case .OverloadWarning:
            return "Overload warning"
        case .none:
            return "N/A"
        }
    }
}

// Metadata types
enum MetadataType: String, CaseIterable {
    case ReadingStatus = "1"
    case CurrentRange = "2"
    case Noise = "4"

    static func asEnum(_ value: String) -> MetadataType? {
        MetadataType.allCases.first {
            $0.rawValue == value
        }
    }
}

// Metadata types
enum CurrentRange: Int, CaseIterable {
    case CR_100nA = 0
    case CR_2uA = 1
    case CR_4uA = 2
    case CR_8uA = 3
    case CR_16uA = 4
    case CR_32uA = 5
    case CR_63uA = 6
    case CR_125uA = 7
    case CR_250uA = 8
    case CR_500uA = 9
    case CR_1mA = 10
    case CR_5mA = 11
    case HSCR_100nA = 128
    case HSCR_1uA = 129
    case HSCR_6uA = 130
    case HSCR_13uA = 131
    case HSCR_25uA = 132
    case HSCR_50uA = 133
    case HSCR_100uA = 134
    case HSCR_200uA = 135
    case HSCR_1mA = 136
    case HSCR_5mA = 137

    static func asEnum(_ value: Int) -> CurrentRange? {
        CurrentRange.allCases.first {
            $0.rawValue == value
        }
    }

    static func asFriendlyString(currentRange: CurrentRange?) -> String {
        switch currentRange {
        case .CR_100nA:
            return "100nA"
        case .CR_2uA:
            return "2uA"
        case .CR_4uA:
            return "4uA"
        case .CR_8uA:
            return "8uA"
        case .CR_16uA:
            return "16uA"
        case .CR_32uA:
            return "32uA"
        case .CR_63uA:
            return "63uA"
        case .CR_125uA:
            return "125uA"
        case .CR_250uA:
            return "250uA"
        case .CR_500uA:
            return "500uA"
        case .CR_1mA:
            return "1mA"
        case .CR_5mA:
            return "5mA"
        case .HSCR_100nA:
            return "100nA (High Speed)"
        case .HSCR_1uA:
            return "1uA (High speed)"
        case .HSCR_6uA:
            return "6uA (High speed)"
        case .HSCR_13uA:
            return "13uA (High speed)"
        case .HSCR_25uA:
            return "25uA (High speed)"
        case .HSCR_50uA:
            return "50uA (High speed)"
        case .HSCR_100uA:
            return "100uA (High speed)"
        case .HSCR_200uA:
            return "200uA (High speed)"
        case .HSCR_1mA:
            return "1mA (High speed)"
        case .HSCR_5mA:
            return "5mA (High speed)"
        case .none:
            return "N/A"
        }
    }
}
