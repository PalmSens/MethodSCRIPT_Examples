//
//  ContentView.swift
//  MethodSCRIPTExample_Swift
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

import SwiftUI
import CoreBluetooth
import Combine

struct ScanForDevicesView: View {
    @EnvironmentObject var communicationObject: CommunicationObject
    @ObservedObject var bleConnection: BLEConnection
    @Binding var isPresented: Bool

    var body: some View {
        NavigationView {
            List(bleConnection.scannedDevices) { device in
                Text(verbatim: device.name!)
                Spacer()
                Button(action: {
                    self.bleConnection.connectToDevice(device: device)
                    self.isPresented = false
                }) {
                    Image(systemName: "circle.fill")
                }.buttonStyle(DefaultButtonStyle(fixedWidth: 100, fixedHeight: 10))
            }.navigationBarTitle("scanning...")
                    .navigationBarItems(trailing: Button(action: {
                        self.isPresented = false
                    }) {
                        Image(systemName: "arrow.left")
                    }.buttonStyle(DefaultButtonStyle(color: .gray, fixedWidth: 60, fixedHeight: 5)))
                    .onAppear(perform: performOnAppear)
                    .onDisappear(perform: performOnDisappear)
        }
    }

    private func performOnAppear() {
        self.bleConnection.startScanningForDevices()
    }

    private func performOnDisappear() {
        self.bleConnection.stopScanningForDevices()
    }
}

struct ContentView: View {
    @State var modalIsPresented = false
    @EnvironmentObject var communicationObject: CommunicationObject
    @ObservedObject var bleConnection = BLEConnection()

    init() {
        // To remove only extra separators below the list:
        //UITableView.appearance().tableFooterView = UIView()

        // To remove all separators including the actual ones:
        //UITableView.appearance().separatorStyle = .none
    }

    var body: some View {
        //NavigationView() {
        VStack {
            HStack {
                Button(action: {
                    self.bleConnection.messages.removeAll()
                    self.modalIsPresented = true
                }) {
                    HStack(spacing: 10) {
                        Image(systemName: "dot.radiowaves.left.and.right")
                    }
                }.sheet(isPresented: $modalIsPresented, content: {
                    ScanForDevicesView(bleConnection: self.bleConnection, isPresented: self.$modalIsPresented).environmentObject(self.communicationObject)
                }).buttonStyle(DefaultButtonStyle(disabled: scanningDisabled())).disabled(scanningDisabled())
                Button(action: {
                    self.bleConnection.sendCommandVersion()
                }) {
                    Image(systemName: "info")
                }.buttonStyle(DefaultButtonStyle(disabled: runningDisabled())).disabled(runningDisabled())
                Button(action: {
                    //NOTE 1: If you measure to many points SwiftUI's list of measurement points could become very slow.
                    //NOTE 2: Methodscript below was generated using PSTrace
                    let script = "e\nvar c\nvar p\nset_pgstat_chan 1\nset_pgstat_mode 0\nset_pgstat_chan 0\nset_pgstat_mode 2\nset_max_bandwidth 80\nset_pot_range 500m 500m\nset_cr 590u\nset_autoranging 59n 590u\ncell_on\nmeas_loop_ca p c 500m 5m 5000m\npck_start\npck_add p\npck_add c\npck_end\nendloop\non_finished:\ncell_off\n\n"
                    self.bleConnection.sendMethodScript(methodScript: script)
                }) {
                    Image(systemName: "play.fill")
                }.buttonStyle(DefaultButtonStyle(disabled: runningDisabled())).disabled(runningDisabled())
                Button(action: {
                    self.bleConnection.sendCommandAbort()
                }) {
                    Image(systemName: "stop.fill")
                }.buttonStyle(DefaultButtonStyle(disabled: abortingDisabled())).disabled(abortingDisabled())
                Button(action: {
                    self.bleConnection.disconnect()
                }) {
                    Image(systemName: "bolt.slash")
                }.buttonStyle(DefaultButtonStyle(disabled: disconnectingDisabled())).disabled(disconnectingDisabled())

            }
            TabView {
                List {
                    ForEach(bleConnection.messages, id: \.self) { message in
                        VStack(alignment: .leading) {
                            Text(message)
                        }
                    }.padding(5) //EXAMPLE (color rows): .listRowBackground(Color.green)
                }.tabItem {
                    Image(systemName: "doc.text")
                    Text("log")
                }.tag(0)
                //NOTE: SwiftUI does not have an Excel like grid control, so code below is a pseudo-like grid solution...
                List {
                    ForEach(bleConnection.measurements) { measurement in
                        VStack(alignment: .leading) {
                            HStack {
                                Text("\(measurement.id)")
                                Text("\(String(format: "%.4f V", measurement.voltage!))")
                                Text("\(String(format: "%.16f A", measurement.current!))")
                                Text("\(measurement.readingStatus!)").fixedSize(horizontal: false, vertical: true)
                                Text("\(measurement.currentRange!)")
                                Spacer()
                            }
                        }
                    }
                }.tabItem {
                    Image(systemName: "table")
                    Text("measurements")
                }.tag(1)
            }
        }.padding().onAppear(perform: performOnAppear)
    }

    private func performOnAppear() {
    }

    func scanningDisabled() -> Bool {
        var result: Bool
        result = bleConnection.bleState != .idle;
        return result
    }

    func runningDisabled() -> Bool {
        var result: Bool
        result = bleConnection.bleState != .connected;
        return result
    }

    func abortingDisabled() -> Bool {
        var result: Bool
        result = bleConnection.bleState != .running;
        return result
    }

    func disconnectingDisabled() -> Bool {
        var result: Bool
        result = (bleConnection.bleState == .idle) || (bleConnection.bleState == .scanning)
        return result
    }

}

struct ContentView_Previews: PreviewProvider {
    static var previews: some View {
        ContentView()
    }
}

//NOTE: Setting disabled via configuration does not disable the button itself (that's why it is not used below (would save a separate call to '.disabled' on the Button)
struct DefaultButtonStyle: ButtonStyle {
    var color: Color = .blue
    var fixedWidth: CGFloat? = 200
    var fixedHeight: CGFloat? = 20
    var disabled = false

    public func makeBody(configuration: DefaultButtonStyle.Configuration) -> some View {
        configuration.label
                //.frame(width: fixedWidth!, height: fixedHeight!, alignment: .center)
                .foregroundColor(.white)
                .padding(EdgeInsets(top: 10, leading: 15, bottom: 10, trailing: 15))
                .background(RoundedRectangle(cornerRadius: 5).fill(color))
                .compositingGroup()
                .shadow(color: .white, radius: 0)
                .opacity(configuration.isPressed || disabled ? 0.5 : 1.0)
                .scaleEffect(configuration.isPressed ? 0.8 : 1.0)

    }
}

// Used for communication between views
class CommunicationObject: ObservableObject {
    @Published var pushed = false
}
