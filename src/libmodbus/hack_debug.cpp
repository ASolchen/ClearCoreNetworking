#include <Arduino.h>
extern "C" {
  #include "hack_debug.h"
}


void serial_print(const char *msg){
    Serial.println(msg);
}
void serial_print_err(const char *msg, int errno=0){
    Serial.print(msg);
    if(errno){
        Serial.print(" Error Num: ");
        Serial.println(msg);
    }
}

void debugDelay(int micros){
    delay(micros);
}