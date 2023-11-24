
#include "Arduino.h"
#include "ClearCore.h"
#include <EthernetTcpServer.h>

class Project{
    public:
        //constructor
        Project(){}
        //public attribs
        EthernetTcpServer *server;
        void setup_serial();
        void setup_ethernet();
};
