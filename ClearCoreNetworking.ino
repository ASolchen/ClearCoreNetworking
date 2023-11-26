#include "Arduino.h"
#include "ClearCore.h"
#include <Ethernet.h>
#include "src/ModbusTCPServer.h"
#define DEBUG

byte mac[] = {
  0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED
};
IPAddress ip(192, 168, 10, 177);

EthernetServer ethServer(502);

ModbusTCPServer modbusTCPServer;

const int ledPin = LED_BUILTIN;


void setup() {
  Serial.begin(9600);
  while (!Serial) {
    ; // wait for serial port to connect. Needed for native USB port only
  }
  Serial.println("Ethernet Modbus TCP Example");

  // start the Ethernet connection and the server:
  Ethernet.begin(mac, ip);

  // Check for Ethernet hardware present
  if (Ethernet.hardwareStatus() == EthernetNoHardware) {
    Serial.println("Ethernet shield was not found.  Sorry, can't run without hardware. :(");
    while (true) {
      delay(1); // do nothing, no point running without Ethernet hardware
    }
  }
  if (Ethernet.linkStatus() == LinkOFF) {
    Serial.println("Ethernet cable is not connected.");
  }

  // start the server
  ethServer.begin();
  
  // start the Modbus TCP server
  if (!modbusTCPServer.begin()) {
    Serial.println("Failed to start Modbus TCP Server!");
    while (1);
  }



  // configure a single coil at address 0x00
  modbusTCPServer.configureCoils(0x00, 100);
}

void loop() {
  // listen for incoming clients
  EthernetClient client = ethServer.available();
  
  if (client) {
    // a new client connected
    Serial.println("new client");
    Serial.println(client);

    // let the Modbus TCP accept the connection 
    modbusTCPServer.accept(client);

    while (client.connected()) {
      // poll for Modbus TCP requests, while client connected
      modbusTCPServer.poll();
/*
      if(client.available()){
        while(client.available()){
          char nextByte = client.read();
          Serial.print(nextByte, HEX);
        }
        Serial.println("");
*/
      delay(50);
      }
      Serial.println("client disconnected");
    }
    
}
