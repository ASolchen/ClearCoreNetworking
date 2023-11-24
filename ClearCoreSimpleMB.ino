#include "src/project_setup.h"
#define MAX_PACKET_LENGTH 256



Project cc_project; //object to hold the project

void setup() {
  cc_project.setup_serial();
  cc_project.setup_ethernet();
}

EthernetTcpClient tempClient;
unsigned char packetReceived[MAX_PACKET_LENGTH];
void loop() {
  Serial.print("In loop....");
  tempClient = cc_project.server->Accept();
  if(tempClient.Connected()){
    Serial.print("Client connected: ");
    Serial.println(tempClient.RemoteIp().StringValue());
    if(tempClient.BytesAvailable()){
      tempClient.Read(packetReceived, MAX_PACKET_LENGTH);
      Serial.println((char *)packetReceived);
    }
  }


  // put your main code here, to run repeatedly:
  EthernetMgr.Refresh();
  delay(100);
}
