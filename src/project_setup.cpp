#include "project_setup.h"

void Project::setup_serial(){
    Serial.begin(9600);
    while (!Serial) {
        ; // wait for serial port to connect. Needed for native USB port only
    }
    Serial.println("Serial Port Ready");
}

void Project::setup_ethernet(){
       
    while (!EthernetMgr.PhyLinkActive()) {
        Serial.println("The Ethernet cable is unplugged...");
        Delay_ms(1000);
    }
    // Set ClearCore IP address
    IpAddress ip = IpAddress(192, 168, 10, 177);
        
    // Optional: set additional network addresses if needed
    IpAddress gateway = IpAddress(192, 168, 10, 1);
    IpAddress netmask = IpAddress(255, 255, 255, 0);
    EthernetMgr.Setup();
    EthernetMgr.LocalIp(ip);
    EthernetMgr.GatewayIp(gateway);
    EthernetMgr.NetmaskIp(netmask);
    Serial.print("Assigned manual IP address: ");
    Serial.println(EthernetMgr.LocalIp().StringValue());
    EthernetTcpServer server(502);
    server.Begin();
    Serial.println("Server now listening for client connections...");
} 