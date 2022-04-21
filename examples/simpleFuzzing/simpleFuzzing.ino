/*
  simpleFuzzing.ino
  Written by David Nnaji @ Colorado State University, April 21st, 2022

  Github:
    https://github.com/davidnnaji
    Do you find this library useful? Let me know online!

  Description:
    Simple J1708 message fuzzing. This code will send random sized
    messeges up to 21-bytes using MID 0x77 with random data.
    Compatible with Teensy 4.0 only.

  Liscense:
    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
    GNU General Public License for more details.
*/


//Libraries
#include "J1708.h"

//J1708 object for reading data
J1708 j1708_3;

//Message Transmit Test Stuff
int interval = 1000;
uint8_t senderMID = 0x77;
unsigned long previousMillis = 0;
unsigned long currentMillis;

void setup() {
  //All J1708 objects print to UART1 (Serial)
  Serial.begin(115200);

  //Serial display options (Default but shown here to be explicit)
  j1708_3.ShowRxData=true;
  j1708_3.ShowTime=true;
  j1708_3.ShowPort=true;
  j1708_3.ShowChecksum=true;
  j1708_3.ShowLength=true;
  j1708_3.ShowErrors=true;
  j1708_3.ShowBusload=false;
  j1708_3.RxLEDOn=true;

  //Bind J1708 object to UART3 (Serial3)
  j1708_3.begin(3);
}

void loop() {
  // J1708 Update Function (i.e., reading, handling, and sending messages)
  // Should be run frequently to ensure proper data handling
  j1708_3.J1708Update();

  currentMillis = millis();
  if (currentMillis - previousMillis >= interval) {
    previousMillis = currentMillis;
    
    int messageLength = random(1,20);
    uint8_t message[messageLength];
    message[0]=senderMID;
    for (int i=1; i<messageLength; i++) {
      message[i] = random(0,255);
    }
    // Send a message! (*Actually* message will be added to a Tx queue and sent when line is available)
    // example: myJ1708.J1708Send(buffer,message_length, priority);
    j1708_3.J1708Send(message,messageLength,8);
  }
}