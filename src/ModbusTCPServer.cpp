/*
  This file is part of the ArduinoModbus library.
  Copyright (c) 2018 Arduino SA. All rights reserved.

  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either
  version 2.1 of the License, or (at your option) any later version.

  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with this library; if not, write to the Free Software
  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
*/

#include <errno.h>


extern "C" {
//#include "libmodbus/modbus.h"
#include "libmodbus/modbus-tcp.h"
}

#include "ModbusTCPServer.h"

ModbusTCPServer::ModbusTCPServer() :
  _client(NULL)
{
}

ModbusTCPServer::~ModbusTCPServer()
{
}



int ModbusTCPServer::begin(int id)
{
  modbus_t* mb = modbus_new_tcp(NULL, IPAddress(0, 0, 0, 0), 0);
 

  if (!ModbusServer::begin(mb, id)) {
    return 0;
  }
 
  if (modbus_tcp_listen(mb) != 0) {
    return 0;
  }

  return 1;
}
/* Attaches the new client to this class.

*/
void ModbusTCPServer::accept(Client& client)
{
  if (modbus_tcp_accept(_mb, &client) == 0) {
    _client = &client;
#ifdef DEBUG
      Serial.println("Client Accepted");
#endif    
    
  }
}

/* Handles ethernet polling.

*/
int ModbusTCPServer::poll()
{


#ifdef DEBUG
  Serial.println("Polling");
#endif

  if (_client != NULL) {
    uint8_t request[MODBUS_TCP_MAX_ADU_LENGTH] ;
    //int requestLength = _client->read(request, sizeof(request));
    if (_client->available()){
      int requestLength = modbus_receive(_mb, request);
#ifdef DEBUG
    Serial.print("Modbus request recieved: ");
    for (int i; i<requestLength; ++i){
      Serial.print(request[i], HEX);
    }
      Serial.println("");
      Serial.print("requestLength = ");
      Serial.println(requestLength);
#endif
      if (requestLength > 0) {
        modbus_reply(_mb, request, requestLength, &_mbMapping);
    }
/*
    client has connected, need to check if data availible
    the mb lib does the following steps:
      1. modbus_receive() // returns the length of the request and puts the request data into a buffer
      2.modbus_reply() // send in the mb_context, the request, the length
        a.checks ctx->backend->header_length which is 7 for tcp modbus
        b. msg_type = MSG_INDICATION, which is server side so it waits
        c. runs checks ctx->backend->select() this runs ctx_tcp->client->available() and returns true or false (1 or 0)
        d. Runs ctx->backend->recv(ctx, msg + msg_length, length_to_read); //pads a zero at the end of the message. This will be the function code.
        e. then finally runs: return ctx_tcp->client->read(rsp, rsp_length); //this should read the data into the buffer "rsp"
*/

      
      return 1;
    }
  }
  return 0;
}
