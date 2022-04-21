/*
  simpleGateway.ino
  Written by David Nnaji @ Colorado State University, April 21st, 2022

  Github:
    https://github.com/davidnnaji
    Do you find this library useful? Let me know online!

  Description:
    Run-time script for a simple two-port J1708 gateway.
    You can also use this code for simple RX and TX on a J1708 bus.
    For configuration during run-time type "j1708config sp3 -h" for help
    To send messages type "j1708send sp3 -h" for help
    Compatible with Teensy 4.0 only.

  Liscense:
    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
    GNU General Public License for more details.
*/

//Librarie(s):
#include <J1708_T4.h>

bool stringComplete = false;  // Flag to indicate whether the string is complete
String inputString = "";      // A String to hold incoming data
String port;                  // Identifies which port command is directed to

J1708 j1708_3;                // J1708 Object for Network Side
J1708 j1708_4;                // J1708 Object for Host Side

void setup() {
  //All j1708 objects print to UART1 (Serial)
  Serial.begin(115200);

  //Bind J1708 objects a specific Serial port
  j1708_3.begin(3);
  j1708_4.begin(4);

  //Forward filtered traffic to Serial Port 4
  j1708_3.link(&j1708_4);

  //Forward filtered traffic to Serial Port 3
  j1708_4.link(&j1708_3);

  //Serial command buffer
  inputString.reserve(256);
}

void loop() {
  // J1708 Updates (i.e., reading, handling, and sending messages)
  j1708_3.J1708Update();
  j1708_4.J1708Update();

  // Command Handling
  if (stringComplete) {
    inputString.trim();
    if (getValue(inputString,' ',1)=="sp3"){
      if (j1708_3.J1708Settings(inputString)){
        Serial.println("Success!");
      }
      else{
        Serial.println("Invalid command. Try <command> -h for help.");
      }
    }
    else if (getValue(inputString,' ',1)=="sp4"){
      if (j1708_4.J1708Settings(inputString)){
        Serial.println("Success!");
      }
      else{
        Serial.println("Invalid command. Try <command> -h for help.");
      }
    }
    inputString = "";
    stringComplete = false;
  }
}

void serialEvent() {
  while (Serial.available()) {
    // Get the new byte:
    char inChar = (char)Serial.read();

    // If the incoming character is a newline, set a flag so the main loop can do something about it:
    if (inChar == '\n') {
      stringComplete = true;
    }
    else {
      // Add byte to the inputString:
      inputString += inChar;
    }
  }
}