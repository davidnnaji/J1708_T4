/*
  J1708_T4.h
  Written by David Nnaji @ Colorado State University, April 21st, 2022

  Github:
    https://github.com/davidnnaji
    Do you find this library useful? Let me know online!

  Description:
    A J1708 gateway utility library.
    Compatible with Teensy 4.0 only.

  Liscense:
    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
    GNU General Public License for more details.
*/

// Library Definition
#ifndef J1708_T4_H
#define J1708_T4_H

// Dependencies
#include <Arduino.h>

// Utility Functions
String getValue(String data, char separator, int index);

uint32_t hex2int(char *hex);

int string2Hex(String data);

//J1708 Object Definition
struct J1708 {
  //Constructor
  // J1708();

  //Teensy 4.0 Hardware Definitions
  int RxLED = 13; //High-LEDON, Low-LEDOFF
  int TxLED = 12; //High-LEDON, Low-LEDOFF
  int SEC_ERR_LED = 9;

  // Enums
  enum nodeMode {Gateway, Rogue, Compromised, Observer};

  //Flags
  bool RxLEDState = true;
  bool TxLEDState = true;
  bool SEC_ERR_LEDState = false;
  bool TP_Rx_Flag = false;
  bool TP_Tx_Flag = false;
  bool Loop_flag = false;
  bool Q_flag = false;
  bool ERR1_Checksum = false;
  bool ERR2_RxOverflow = false;
  bool ERR3_Tx_Overflow = false;
  bool ERR4_Collision = false;
  bool ERR5_DataNotSent = false;
  bool ERR6_HighBusload = false;
  bool rx_busy = false;
  bool tx_busy = false;
  bool tx_transmitting = false;
  bool J1708Object_Linked = false;
  int ERR2_MID_Hold = -1;

  //Timers
  elapsedMicros J1708Timer;         //Set up a microsecond timer to run after each byte is received.
  elapsedMicros J1708TxTimer;       //Set up a microsecond timer to run for Tx network access timing.
  elapsedMicros SerialTimer;        //Set up a microsecond timer when data is printed on Serial 1.
  elapsedMillis J1708LoopTimer;     //Set up a microsecond timer to run after each byte is received.
  elapsedMillis BusloadTimer;       //Busload calculation timer
  elapsedMillis ERR6_Timer;         //ERR6 Periodic Send Timer
  elapsedMillis ERR8_Timer;         //ERR8 Periodic Send Timer
  elapsedMillis TP_Session_Timer;   //Transport Session Timer
  elapsedMillis SEC_ERR_Timer;      //Security LED timer

  //Configurables
  bool ShowCommand = false;
  bool ShowRxData = true;
  bool ShowTime = true;
  bool ShowPort = true;
  bool ShowChecksum = false;
  bool ShowLength = true;
  bool ShowErrors = true;
  bool ShowBusload = false;
  bool ShowMIDShare = false;
  bool Rx_Forwarding = false;
  bool RxLEDOn = true;
  bool TxLEDOn = true;
  bool SECLEDOn = true;
  bool selfACL[256];
  uint8_t selfMID = 120;            //0x78 - Change this to define the gateway MID
  nodeMode selfMode = Gateway;
  bool selfHostPort = false;
  uint32_t ERR7_Limit = 256;        //65,535 (2-bytes) Max
  float maxBusload = 1.0;           //Don't forget to add "."
  float maxMIDShare = 1.0;          //Don't forget to add "."
  bool GatewaySpecificProcessing = false; //Allows the gateway to respond to requests (false means it will only perform normal fucntionality)
  int TxQmax = 32;  // Indicates TxQueue size. Can be used to size the leaky bucket 32 MAX
  uint8_t TxQueuePenalty = 0;
  const static uint8_t TxQmaxMaxPenalty = 1; 
  const static uint32_t PenaltyTime = 1000; // Leaky Bucket: P = PenaltyTime*TxQueuePenalty microseconds

  //Variables
  uint32_t idleTime = 1250;
  float busload; // approx: character_count/(9600/10) % max_characters/s
  uint32_t TotalByteCount = 0;
  uint32_t MIDByteCount[256];
  uint8_t J1708FrameLength = 0;
  uint32_t J1708ByteCount;
  uint8_t J1708Checksum = 0;
  uint8_t TP_Rx_NBytes=0;
  uint8_t TP_Rx_NSegments=0;
  uint8_t TP_Tx_NBytes=0;
  uint8_t TP_Tx_NSegments=0;
  uint8_t TP_Session_MID =0;
  uint8_t TP_Default_Segment_Size=15;
  uint8_t fx = 0;
  const uint32_t N_Rate = 1000; //Normal
  const uint32_t C_Rate = 2000; //Cooldown
  uint32_t P = N_Rate;
  uint8_t Q_Counter = 0;
  uint32_t ERR_Counter = 0;
  uint32_t ERR1_Counter = 0; // Checksum Error
  uint32_t ERR2_Counter = 0; // Buffer Overflow Error
  uint32_t ERR3_Counter = 0; // Transmit Buffer Overflow Error (i.e. Too many messages being sent)
  uint32_t ERR4_Counter = 0; // Message Collision Error
  uint32_t ERR5_Counter = 0; // Transmit Error (i.e. Disconnected from bus, hardware fault)
  uint32_t ERR6_Counter = 0; // High Busload Warning
  uint32_t ERR6_ConsecutiveCounter = 0;
  uint8_t ERR6_ConsecutiveMax = 4; // approx. n/2 seconds of consecutive high busload
  uint32_t ERR7_Counter = 0; // Spoofed Message Error
  uint16_t ERR7_IDCounter[256];
  uint16_t ERR8_Counter = 0; // Rogue Node Detected Error
  const static int ERR8_Interval = 10000;
  bool ERR8_Tracker[256];
  uint16_t ERR9_Counter = 0; // Compromised Node Error
  bool ERR9_Tracker[256];
  uint16_t ERR10_Counter = 0; // Compromised Host Error
  bool ERR10_Tracker[256];
  uint32_t SEC_ERR_Counter = 0;
  uint32_t RX_Counter = 0;
  uint32_t TX_Counter = 0;
  uint32_t FWD_Counter = 0;
  uint8_t N_TxQ = 0;
  uint8_t N_TxS = 0;
  uint8_t N_TxQ_Total = 0;
  int selfPN;

  // Timing Constants (microseconds)
  const static uint32_t onebit = 105;
  const static uint32_t twobit = 209;
  const static uint32_t tenbit = 1042;
  const static uint32_t elevenbit = 1446;
  const static uint32_t twelvebit = 1250;
  const static uint32_t nineteenbit = 1980;

  //Global Buffers & Arrays
  const static int RxBufferSize = 22;
  uint8_t J1708RxBuffer[RxBufferSize]; //Buffer for unprinted Rx frames
  uint8_t J1708TxQ[32][21];        //Buffer for queued Tx frames
  int J1708TxQLengths[32];     //Buffer for queued Tx frame lengths
  uint8_t J1708TxQPriorities[32];  //Buffer for queued Tx frame priorities
  char hexDisp[4]; //Character display buffer
  uint8_t TP_Tx_Buffer[256] = {}; //Transport Protocol Buffer
  uint8_t TP_Rx_Buffer[256] = {}; //Transport Protocol Buffer
  uint8_t Loopbuffer[21];
  uint8_t TP_TxMessageQueue[10][21]; //Max sendable bytes in J1587 TP Protocol: 3825
  uint8_t Q_Matrix[20][21];
  uint8_t Q_Lengths[20];
  uint8_t Q_Message[] = {};
  float MIDShareTracker[256];


  // Setup Functions
  bool begin(int port_number=3, int baud=9600, int rx_led=13, int tx_led=12);
  void link(J1708 *_j1708Object);
  void unlink();


  // Primary Functions
  uint8_t J1708Rx(uint8_t (&J1708RxFrame)[RxBufferSize]);

  void J1708AppendChecksum(uint8_t J1708TxData[],const uint8_t &TxFrameLength);

  bool J1708Tx(uint8_t J1708TxData[], const uint8_t &TxFrameLength, const uint8_t &TxFramePriority, bool AutoChecksum=true);

  bool J1708Send(uint8_t J1708TxData[], const int &TxFrameLength, const int &TxFramePriority);
  
  bool RTS_Handler(uint8_t TP_Data[]);
  
  bool CTS_Handler(uint8_t TP_Data[]);
  
  bool CDP_Handler(uint8_t TP_Data[]);
  
  void EOM_Handler(uint8_t TP_Data[]);
  
  void Abort_Handler(uint8_t TP_Data[]);
  
  bool J1708CheckChecksum(uint8_t J1708Message[],const uint8_t &FrameLength);
  
  bool J1708CheckACL(const uint8_t &mid);
  
  void UpdateNetworkStatistics();
  
  void J1708CheckNetwork();
  
  void J1708ResetACL(bool mode);
  
  void J1708UpdateACL(const uint8_t &mid, bool set=true);
  
  int J1708Parse();
  
  void J1708Listen();
  
  bool J1708TransportTx(uint8_t TP_Data[], const uint16_t &nBytes, const uint8_t &D_MID);
  
  void J1708Log();
  
  void J1708Update();
  
  bool J1708Settings(String &command);
  
  private:
  //Object References
  HardwareSerial *_streamRef; // The Rx/Tx Serial Port for this object.
  J1708 *_j1708Ref;           // Used for linking to another J1708 object
};

#endif