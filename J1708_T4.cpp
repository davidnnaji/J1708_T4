/*
  J1708_T4.cpp
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

// Dependencies
#include <J1708_T4.h>
// #include <EEPROM.h>

// Utility Functions
String getValue(String data, char separator, int index){
  int found = 0;
  int strIndex[] = { 0, -1 };
  int maxIndex = data.length() - 1;

  for (int i = 0; i <= maxIndex && found <= index; i++) {
      if (data.charAt(i) == separator || i == maxIndex) {
          found++;
          strIndex[0] = strIndex[1] + 1;
          strIndex[1] = (i == maxIndex) ? i+1 : i;
      }
  }
  return found > index ? data.substring(strIndex[0], strIndex[1]) : "";
}

uint32_t hex2int(char *hex) {
    uint32_t val = 0;
    while (*hex) {
        // get current character then increment
        uint8_t byte = *hex++; 
        // transform hex character to the 4bit equivalent number, using the ascii table indexes
        if (byte >= '0' && byte <= '9') byte = byte - '0';
        else if (byte >= 'a' && byte <='f') byte = byte - 'a' + 10;
        else if (byte >= 'A' && byte <='F') byte = byte - 'A' + 10;    
        // shift 4 to make space for new digit, and add the 4 bits of the new digit 
        val = (val << 4) | (byte & 0xF);
    }
    return val;
}

int string2Hex(String data){
  if (data.length()==2){
    char temp[4];
    for (int i=0;i<2;i++){
      if (!isHexadecimalDigit(data[i])){
        return -1;
      }
    }
    data.toCharArray(temp,4);
    return hex2int(temp);
  }
  else {
    return -1;
  }
}

// Setup Functions
bool J1708::begin(int port_number, int baud, int rx_led, int tx_led){
  /*
    Setup UART Connection
    1 - Reserved Print console
    2 - Reserved Future use
    3 - Default J1708
    4 - Secondary J1708
    5 - Free
    6 - -
    7 - -
  */
  if (port_number==3){
    _streamRef = &Serial3;
    _streamRef->begin(baud);
    selfPN = port_number;
  }
  else if (port_number==4){
    _streamRef = &Serial4;
    _streamRef->begin(baud);
    Serial4.begin(baud);
    selfPN = port_number;
    tx_led=6;
    rx_led=5;
  }
  else if (port_number==5){
    _streamRef = &Serial5;
    _streamRef->begin(baud);
    Serial5.begin(baud);
    selfPN = port_number;
    tx_led=4;
    rx_led=3;
  }
  else{
    return false;
  }
  //Rx Pin Configuration
  RxLED = rx_led;
  pinMode(RxLED,OUTPUT);
  digitalWrite(RxLED,RxLEDState);
  //Tx Pin Configuration
  TxLED = tx_led;
  pinMode(TxLED,OUTPUT);
  digitalWrite(TxLED,TxLEDState);
  pinMode(SEC_ERR_LED,OUTPUT);
  digitalWrite(SEC_ERR_LED,SEC_ERR_LEDState);
  selfACL[selfMID]=true;
  return true;
}

void J1708::link(J1708 *_j1708Object){
  _j1708Ref = _j1708Object;
  J1708Object_Linked = true;
  Rx_Forwarding = true;
}

void J1708::unlink(){
  J1708Object_Linked = false;
  Rx_Forwarding = false;
  delete _j1708Ref;
}

// Primary Functions
uint8_t J1708::J1708Rx(uint8_t (&J1708RxFrame)[RxBufferSize]){
  if (_streamRef->available()){
    J1708Timer = 0; //Reset the RX message timer for J1708 message framing
    J1708TxTimer = 0;
    J1708ByteCount++; // Increment the recieved byte counts
    TotalByteCount++; // Include self transmitted or forwarded messages in calculation
    rx_busy = true;
    
    if (J1708ByteCount < sizeof(J1708RxBuffer)){ //Ensure the RX buffer can handle the new messages
      J1708RxBuffer[J1708ByteCount] = _streamRef->read();
      J1708Checksum += J1708RxBuffer[J1708ByteCount];
    }
    else{
      //This is what we do if we don't have room in the RX buffer., "J1708 Buffer Overflow"
      ERR2_RxOverflow = true;
      ERR_Counter++;
      ERR2_Counter++;
      J1708ByteCount = 0;
      rx_busy = false;
      J1708TxTimer = 0;
      if (ERR2_MID_Hold<0){
        ERR2_MID_Hold = J1708RxBuffer[1];
      }
      if (ShowErrors){
        Serial.print("ERR2:"); //debug
        Serial.print("[");Serial.print(ERR_Counter);Serial.println("] "); //debug
      }
    }
  }

  //Check to see if a message has been framed?
  if (J1708Timer > twelvebit && J1708ByteCount > 0){
    J1708FrameLength = J1708ByteCount;
    J1708ByteCount = 0; //reset the counter
    
    J1708Checksum -= J1708RxBuffer[J1708FrameLength]; //remove the received Checksum byte (last one)
    J1708Checksum = (~J1708Checksum + 1) & 0xFF; // finish calculating the checksum
    bool J1708ChecksumOK = J1708Checksum == J1708RxBuffer[J1708FrameLength];
    
    if (ERR2_RxOverflow){
      //Spilled over data from ERR2
      J1708Checksum = 0;
      rx_busy = false;
      ERR2_RxOverflow = false;
      MIDByteCount[ERR2_MID_Hold] += J1708FrameLength;
      ERR2_MID_Hold = -1;
      return 0;
    }

    if (J1708ChecksumOK) {
      ERR1_Checksum = false;
      ERR2_RxOverflow = false;
      rx_busy = false;
      RX_Counter++;
      MIDByteCount[J1708RxBuffer[1]] += J1708FrameLength;
      ERR2_MID_Hold = -1;
      return J1708FrameLength;
    }
    else {
      ERR1_Checksum = true;
      ERR1_Counter++;
      ERR_Counter++;
      RX_Counter++;
      J1708Checksum = 0;
      rx_busy = false;
      if (ShowErrors){
        Serial.print("ERR1 ");  //debug
        Serial.print("[");Serial.print(ERR_Counter);Serial.println("] ");
      }
      MIDByteCount[J1708RxBuffer[1]] += J1708FrameLength;
      ERR2_MID_Hold = -1;
      return 0; //data would not be valid, so pretend it didn't come
    }
  }
  else {
    return 0; //A message isn't ready yet.
  }
}

void J1708::J1708AppendChecksum(uint8_t J1708TxData[],const uint8_t &TxFrameLength){
  uint8_t chk = 0;
  for (int i=0; i<(TxFrameLength-1);i++){
    chk+=J1708TxData[i];
  }
  chk=((~chk<<24)>>24)+1;
  J1708TxData[TxFrameLength-1] = chk;
}

bool J1708::J1708Tx(uint8_t J1708TxData[], const uint8_t &TxFrameLength, const uint8_t &TxFramePriority, bool AutoChecksum){ 
  //Max bytes in a J1708 frame is 21.
  tx_busy = true;
  //Append checksum
  if (AutoChecksum){
    J1708AppendChecksum(J1708TxData,TxFrameLength);
  }
  uint8_t mid = J1708TxData[0];
  J1708Timer=0;
  J1708TxTimer = 0;
  //Send the MID
  _streamRef->write(mid);
  while (!_streamRef->available() && J1708TxTimer < twelvebit){
  }
  //Check for collision
  if (_streamRef->available()){
    if (_streamRef->peek()==mid){   
      // Great, no collision. send the rest of the data
      for (int i=1; i<TxFrameLength;i++){
        _streamRef->write(J1708TxData[i]);
      }
      J1708Timer=0;
      J1708TxTimer = 0;
      if (TxLEDOn){
        if (TxLEDState){
          TxLEDState = false;
          digitalWrite(TxLED,TxLEDState);
        }
        else{
          TxLEDState = true;
          digitalWrite(TxLED,TxLEDState);
        }
      }
      tx_busy = false;
      ERR4_Collision = false;
      TX_Counter++;
      tx_transmitting=true;
      return 1;
    }
    else{
      //There was a collision.
      //Collision handling routine
      ERR4_Collision = true;
      ERR4_Counter++;
      ERR_Counter++;
      if (ShowErrors){
        Serial.print("ERR4:"); //debug
        Serial.print("[");Serial.print(ERR_Counter);Serial.println("] "); //debug
      }
    }
  }
  else{
    //Did not send MID or it was not captured
    ERR5_DataNotSent = true;
    ERR_Counter++;
    ERR5_Counter++;
    if (ShowErrors){
      Serial.print("ERR5:");
      Serial.print("[");Serial.print(ERR_Counter);Serial.println("] ");
    }
  }
  J1708Timer=0;
  J1708TxTimer = 0;
  TX_Counter++;
  tx_busy = false;
  return 0;
}

bool J1708::J1708Send(uint8_t J1708TxData[], const int &TxFrameLength, const int &TxFramePriority){
  if (N_TxQ_Total < TxQmax){
    if (TxQueuePenalty > 0){
      TxQueuePenalty--;
    }
    if (N_TxS==TxQmax-1){
      memcpy(J1708TxQ[N_TxS], J1708TxData, TxFrameLength);
      J1708TxQLengths[N_TxS] = TxFrameLength;
      J1708TxQPriorities[N_TxS] = TxFramePriority;
      N_TxQ_Total++;
      N_TxS=0;
      ERR3_Tx_Overflow = false;
      return 1;
    }
    else {
      memcpy(J1708TxQ[N_TxS], J1708TxData, TxFrameLength);
      J1708TxQLengths[N_TxS] = TxFrameLength;
      J1708TxQPriorities[N_TxS] = TxFramePriority;
      N_TxQ_Total++;
      N_TxS++;
      ERR3_Tx_Overflow = false;
      return 1;
    }
  }
  else {
    ERR3_Tx_Overflow = true;
    ERR_Counter++;
    ERR3_Counter++;
    if (TxQueuePenalty < TxQmaxMaxPenalty){
      TxQueuePenalty ++;
    }
    if (ShowErrors){
      Serial.print("ERR3:");
      Serial.print("[");Serial.print(ERR_Counter);Serial.println("] ");
    }
    return 0;
  }
}

bool J1708::RTS_Handler(uint8_t TP_Data[]){
  //Serial.print("RTS Handler Started [");Serial.print(selfMID);Serial.println("]");
  uint8_t D_MID=TP_Data[1];
  if (!TP_Rx_Flag && !TP_Tx_Flag){
    TP_Rx_NSegments=TP_Data[6];
    uint16_t TP_Rx_NBytes = ( (uint16_t)TP_Data[8]<<8 ) | ( (uint16_t)TP_Data[7] );
    //Sanity check
    if (TP_Rx_NSegments>0 && TP_Rx_NBytes>0){
      if (TP_Rx_NBytes<=256){
        //Good to go...
        uint8_t CTS_message[8] = {selfMID,197,4,D_MID,2,TP_Rx_NSegments,1,0};
        J1708Send(CTS_message,8,8);
        TP_Rx_Flag = true;
        TP_Session_MID = D_MID;
        TP_Session_Timer = 0;
        return 1;
        //Serial.print("RTS Handler Complete [");Serial.print(selfMID);Serial.println("]");
      }
      else{
        //Restricting to 256 for now to save memory; Protocol max is 3825-bytes. Abort the request.
        uint8_t Abort_msg[6] = {selfMID,197,2,D_MID,255,0};
        J1708Send(Abort_msg,6,8);
        TP_Rx_Flag = false;
        return 0;
      }
    }
    else{
      //Malformed request. Abort.
      uint8_t Abort_msg[6] = {selfMID,197,2,D_MID,255,0};
      J1708Send(Abort_msg,6,8);
      TP_Rx_Flag = false;
      return 0;
    }
  }
  else{
    //Serial.print("RTS is busy but received request. [");Serial.print(selfMID);Serial.println("]");
    //Busy with another connection. Abort the request for now.
    uint8_t Abort_msg[6] = {selfMID,197,2,D_MID,255,0};
    J1708Send(Abort_msg,6,8);
    return 0;
  }
}

bool J1708::CTS_Handler(uint8_t TP_Data[]){
  //Serial.print("CTS Handler Started [");Serial.print(selfMID);Serial.println("]");
  uint8_t D_MID=TP_Data[1];
  if (TP_Tx_Flag && D_MID==TP_Session_MID){
    uint8_t TP_NSegments=TP_Data[6];
    uint8_t TP_StartSegment=TP_Data[7];
    uint8_t TP_NBytes=TP_Tx_NBytes; 
    int N=0;

    //Sanity Check - Is request actually logical relative to RTS
    if (TP_NSegments <= TP_Tx_NSegments && TP_StartSegment<=TP_Tx_NSegments){
      //Good to go
      for (int i=TP_StartSegment; i<TP_NSegments+TP_StartSegment; i++){
        //Do we have enough data to fill up TP_Default_Segment_Size?
        if (TP_NBytes>=TP_Default_Segment_Size){
          //Yes...
          N = TP_Default_Segment_Size;
          Q_Lengths[i-1] = N+5+1;
          Q_Matrix[i-1][0]=selfMID;
          Q_Matrix[i-1][1]=198;
          Q_Matrix[i-1][2]=N+2;
          Q_Matrix[i-1][3]=D_MID;
          Q_Matrix[i-1][4]=i;
          for(int j=0; j<N;j++){
            Q_Matrix[i-1][j+5] = TP_Tx_Buffer[j+(N*(i-1))];
            TP_NBytes--;
          }
        }
        else{
          //No, this is the last segment. It has less than the max TP_Default_Segment_Size amount of data. 
          N = TP_NBytes;
          Q_Lengths[i-1] = N+5+1;
          Q_Matrix[i-1][0]=selfMID;
          Q_Matrix[i-1][1]=198;
          Q_Matrix[i-1][2]=N+2;
          Q_Matrix[i-1][3]=D_MID;
          Q_Matrix[i-1][4]=i;
          for (int k=0; k<N; k++){
            Q_Matrix[i-1][k+5] = TP_Tx_Buffer[k+(TP_Default_Segment_Size*(i-1))];
            TP_NBytes--;
          }
        }
      }
      //Serial.print("CTS Handler Complete [");Serial.print(selfMID);Serial.println("]");
      Q_flag = true;
      return 1;
    }
    //Bad CTS request. Abort the request and session.
    uint8_t Abort_msg[6] = {selfMID,197,2,D_MID,255,0};
    J1708Send(Abort_msg,6,8);
    TP_Tx_Flag = false;
    TP_Tx_NBytes=0;
    TP_Tx_NSegments=0;
    Q_Counter=0;
    Q_flag=false;
    return 0;
  }
  else{
    //Serial.print("CTS handler received request but is busy or did not expext CTS. [");Serial.print(selfMID);Serial.println("]");
    //Not in an active connection or busy with another connection. Abort the request for now.
    uint8_t Abort_msg[6] = {selfMID,197,2,D_MID,255,0};
    J1708Send(Abort_msg,6,8);
    return 0;
  }
}

bool J1708::CDP_Handler(uint8_t TP_Data[]){
  // Serial.print("CDP Handler Started [");Serial.print(selfMID);Serial.println("]");
  uint8_t D_MID=TP_Data[1];
  if (TP_Rx_Flag && TP_Session_MID==D_MID){
    uint8_t TP_NBytes=TP_Data[3]; 
    uint8_t TP_SegmentNumber=TP_Data[5];
    uint8_t TP_Start=(TP_SegmentNumber-1)*TP_Default_Segment_Size;

    //Here is where aditional CTS messages could be sent if malformed
    //data is received.
    for (int i=0; i<(TP_NBytes-2); i++){
      TP_Rx_Buffer[TP_Start+i] = TP_Data[i+6];
    }
    if (TP_SegmentNumber==TP_Rx_NSegments){
      uint8_t EOM_message[6] = {selfMID,197,2,D_MID,3};
      J1708Send(EOM_message,6,8);
      //Reset all Variables and Flags
      TP_Rx_Flag = false;
      TP_Rx_NBytes=0;
      TP_Rx_NSegments=0;
      // Serial.print("CDP Handler Complete [");Serial.print(selfMID);Serial.println("]");
      return 1;
    }
    //Serial.print("CDP Handler is expecting more data Segments. [");Serial.print(selfMID);Serial.println("]");
    //Request the next batch of data segments or wait.
    return 0;
  }
  else{
    //CDP Received but not in an active session/busy with another one. Abort.
    TP_Rx_Flag = false;
    TP_Rx_NBytes=0;
    TP_Rx_NSegments=0;
    uint8_t Abort_msg[6] = {selfMID,197,2,D_MID,255,0};
    J1708Send(Abort_msg,6,8);
    return 0;
  }
}

void J1708::EOM_Handler(uint8_t TP_Data[]){
  //Serial.print("EOM Handler Started [");Serial.print(selfMID);Serial.println("]");
  uint8_t D_MID=TP_Data[1];
  if (TP_Tx_Flag && D_MID==TP_Session_MID){
    //Reset all TP_Tx variables and Flags. End Session.
    TP_Tx_Flag = false;
    TP_Tx_NBytes=0;
    TP_Tx_NSegments=0;
    Q_Counter=0;
    Q_flag=false;
  }
  else{
    //We received an EOM that is not related to our ongoing session.
    //Tell them to Abort. (not really necessary)
    uint8_t Abort_msg[6] = {selfMID,197,2,D_MID,255,0};
    J1708Send(Abort_msg,6,8);
  }
  //Serial.print("EOM Handler Complete [");Serial.print(selfMID);Serial.println("]");
}

void J1708::Abort_Handler(uint8_t TP_Data[]){
  //Serial.print("Abort Handler Started [");Serial.print(selfMID);Serial.println("]");
  uint8_t D_MID=TP_Data[1];
  if ((TP_Tx_Flag && D_MID==TP_Session_MID) || (TP_Rx_Flag && D_MID==TP_Session_MID)){
    //Reset all TP_Tx and TP_Rx variables and flags
    TP_Rx_Flag = false;
    TP_Rx_NBytes=0;
    TP_Rx_NSegments=0;
    TP_Tx_Flag = false;
    TP_Tx_NBytes=0;
    TP_Tx_NSegments=0;
    Q_Counter=0;
    Q_flag=false;
  }
  else{
    //We received an Abort that is not related to our ongoing session.
    //Ignore it.
  }
  //Serial.print("Abort Handler Complete [");Serial.print(selfMID);Serial.println("]");
}

bool J1708::J1708CheckChecksum(uint8_t J1708Message[],const uint8_t &FrameLength){
  uint8_t chk = 0;
  for (int i=0; i<(FrameLength-1);i++){
    chk+=J1708Message[i];
  }
  chk=((~chk<<24)>>24)+1;
  if (J1708Message[FrameLength]==chk){
    return 1;
  }
  else{
    return 0;
  }
}

bool J1708::J1708CheckACL(const uint8_t &mid){
  if (tx_transmitting){
    return false;
  }
  if (selfACL[mid]){
    if (mid==selfMID){
      SEC_ERR_Counter++;
      digitalWrite(SEC_ERR_LED,!SEC_ERR_LEDState);
      ERR_Counter++;
      ERR7_Counter++;
      ERR7_IDCounter[mid]++;
      if (ERR7_IDCounter[selfMID]<=ERR7_Limit){
        uint8_t msg[10] = {selfMID,255,255,250,4,1,selfMID,(uint8_t)((ERR7_IDCounter[selfMID]<<8)>>8),(uint8_t)(ERR7_IDCounter[selfMID]>>8),0};
        J1708Send(msg,10,8);
      }
      else if (ERR7_IDCounter[selfMID]==ERR7_Limit+1){
        ERR8_Tracker[selfMID] = true;
        ERR8_Counter++;
        ERR_Counter++;
        SEC_ERR_Counter++;
        digitalWrite(SEC_ERR_LED,!SEC_ERR_LEDState);
      }
      if (ERR8_Timer > ERR8_Interval && ERR8_Tracker[selfMID] == true){
        uint8_t msg[9] = {selfMID,255,255,250,3,2,selfMID,(uint8_t)ERR8_Counter,0};
        J1708Send(msg,9,3);
        ERR8_Timer = 0;
      }
    }
    return false;
  }
  return true;
}

void J1708::UpdateNetworkStatistics(){
  if (BusloadTimer>1000){
    // Two calculation methods see Thesis Chapter 5:
    //   1.) Absolute Max: Assuming buad of 9600 -> max 10bit characters in one second = 960.0
    //   2.) Protocol Max: Assuming buad of 9600, J1708 message overhead, 21-byte messages -> max 10bit characters in one second = 903.0
    busload = (float)TotalByteCount/903.0;
    
    for (int i=0;i<=255;i++){
      MIDShareTracker[i] = (float)MIDByteCount[i]/(float)TotalByteCount;
      MIDByteCount[i] = 0;
    }
    
    BusloadTimer = 0;
    TotalByteCount = 0;
  }
}

void J1708::J1708CheckNetwork(){
  if (ERR6_Timer > 500){
    if (busload>maxBusload){
      ERR6_Counter++;
      ERR_Counter++;
      ERR6_HighBusload = true;
      ERR6_ConsecutiveCounter++;
      if (ERR6_ConsecutiveCounter>ERR6_ConsecutiveMax){
        for (int i=0;i<=255;i++){
          if (MIDShareTracker[i]>maxMIDShare){
            // Flooding Caught
            if (selfHostPort==false){
              // ...on the shared network (ERR9)
              if (!ERR9_Tracker[i]){
                ERR9_Tracker[i]=true;
                ERR9_Counter++;
                ERR_Counter++;
                SEC_ERR_Counter++;
                digitalWrite(SEC_ERR_LED,!SEC_ERR_LEDState);
                J1708UpdateACL(i,true);
                uint8_t msg[8] = {selfMID,255,255,250,2,3,(uint8_t)i,0};
                J1708Send(msg,8,1);
                // Dual-side alert - Not necessary
                // if (J1708Object_Linked && Rx_Forwarding){
                //   _j1708Ref->J1708Send(msg,8,1);
                // }
              }
            }
            else{
              // ...on the host network (ERR10)
              if (!ERR10_Tracker[i]){
                ERR10_Tracker[i]=true;
                ERR10_Counter++;
                ERR_Counter++;
                SEC_ERR_Counter++;
                digitalWrite(SEC_ERR_LED,!SEC_ERR_LEDState);
                J1708UpdateACL(i,true);
                uint8_t msg[8] = {selfMID,255,255,250,2,4,(uint8_t)i,0};
                if (J1708Object_Linked && Rx_Forwarding){
                  _j1708Ref->J1708Send(msg,8,1);
                }
                // Dual-side alert - Not necessary
                // J1708Send(msg,8,1);
              }
            }
          }
        }
      }
    }
    else{
      ERR6_HighBusload = false;
      ERR6_ConsecutiveCounter = 0;
    }
    ERR6_Timer = 0;
  }
  if (TP_Rx_Flag || TP_Tx_Flag){
    if (TP_Session_Timer > 10000){
      //Abort the current session
      TP_Rx_Flag = false;
      TP_Rx_NBytes=0;
      TP_Rx_NSegments=0;
      TP_Tx_Flag = false;
      TP_Tx_NBytes=0;
      TP_Tx_NSegments=0;
      Q_Counter=0;
      Q_flag=false;
      uint8_t Abort_msg[6] = {selfMID,197,2,TP_Session_MID,255,0};
      J1708Send(Abort_msg,6,8);
    }
  }
  if (SECLEDOn){
    if (ERR8_Counter>0||ERR9_Counter>0||ERR10_Counter>0){
      if (SEC_ERR_Timer>1000){
        if (SEC_ERR_LEDState){
          digitalWrite(SEC_ERR_LED,HIGH);
          SEC_ERR_LEDState = false;
        }
        else{
          digitalWrite(SEC_ERR_LED,LOW);
          SEC_ERR_LEDState = true;
        }
        SEC_ERR_Timer = 0;
      }
    }
    else if (SEC_ERR_Counter>0){
      if (SEC_ERR_Timer>500){
        if (SEC_ERR_LEDState){
          digitalWrite(SEC_ERR_LED,LOW);
          SEC_ERR_LEDState = false;
        }
      }
    }
  }
}

void J1708::J1708ResetACL(bool mode){
  //Reset ACL
  for (int i=0; i<256;i++){
    selfACL[i]=mode;
  }
}

void J1708::J1708UpdateACL(const uint8_t &mid, bool set){
  selfACL[mid] = set;
}

int J1708::J1708Parse(){
  if (!Loop_flag){
    //Save the current message in the RxBuffer to a more stable 32 byte location.
    //This also frees the J1708RxBuffer to be used in loop() after J1708Parse() is called.
    //uint8_t Loopbuffer[32]={};
    memcpy(Loopbuffer, J1708RxBuffer, 21);
    //Security Message Handling
    //Performed as early as possible compared to normal handling
    if (Loopbuffer[2]==255 && Loopbuffer[3]==255 && Loopbuffer[4]==250){
      uint8_t security_check = Loopbuffer[6];
      //Spoof Alert
      if (security_check==1){
        ERR7_IDCounter[Loopbuffer[7]]++;
        SEC_ERR_Counter++;
        digitalWrite(SEC_ERR_LED,!SEC_ERR_LEDState);
        ERR_Counter++;
        ERR7_Counter++;
        return 0;
      }
      else if (security_check==2){
        if (!ERR8_Tracker[Loopbuffer[7]]){
          SEC_ERR_Counter++;
          digitalWrite(SEC_ERR_LED,!SEC_ERR_LEDState);
          ERR_Counter++;
          ERR8_Counter++;
          ERR8_Tracker[Loopbuffer[7]] = true;
        }
        J1708UpdateACL(Loopbuffer[7],true);
        return 0;
      }
      else if (security_check==3){
        if (!ERR9_Tracker[Loopbuffer[7]]){
          SEC_ERR_Counter++;
          digitalWrite(SEC_ERR_LED,!SEC_ERR_LEDState);
          ERR_Counter++;
          ERR9_Counter++;
        }
        J1708UpdateACL(Loopbuffer[7],true);
        return 0;
      }
      else if (security_check==4){
        if (!ERR10_Tracker[Loopbuffer[7]]){
          SEC_ERR_Counter++;
          digitalWrite(SEC_ERR_LED,!SEC_ERR_LEDState);
          ERR_Counter++;
          ERR10_Counter++;
        }
        J1708UpdateACL(Loopbuffer[7],true);
        return 0;
      }
      else {
        //Received a malformed Security message
        //Do nothing...
        return 0;
      }
    }
    //Normal Message Pre-Processing
    if (GatewaySpecificProcessing){
      switch(Loopbuffer[2]){ //PID
        case 128:
          //Serial.println("PID 128 Received!"); //debug
          if (selfMID==Loopbuffer[4]){
            //Component-Specific Request Parameter handler
          }
          return 0;
          break;
        case 197:
          //Serial.println("PID 197 Received!"); //debug
          if (Loopbuffer[4]==selfMID){
            switch(Loopbuffer[5]){
              case 1:
                //Serial.println("RTS Received!");//debug
                //Run RTS Handler
                return 1;
                break;
              case 2:
                //Serial.println("CTS Received!");//debug
                //Run CTS Handler
                return 2;
                break;
              case 3:
                //Serial.println("EOM Received!");//debug
                //Run EOM Handler
                return 3;
                break;
              case 255:
                //Serial.println("Abort Received!");//debug
                //Run Abort Handler
                return 4;
                break;
            }
          }
          return 0;
          break;
        case 198:
          //Serial.println("PID 198 Received!"); //debug
          if (Loopbuffer[4]==selfMID){
            //Serial.println("CDP Received!"); //debug
            //CDPHandler
            return 5;
            break;
          }
        default:
          break;
      }
      return 0;
    }
    else{
      return 0;
    }
    return 0;
  }
  return 0;
}

void J1708::J1708Listen(){
  if (J1708Rx(J1708RxBuffer)>0){
    if (J1708CheckACL(J1708RxBuffer[1])){
      if (J1708Object_Linked){
        if (Rx_Forwarding){
          _j1708Ref->J1708Send(J1708RxBuffer+1,J1708FrameLength,0);
          FWD_Counter++;
        }
      }
    }
    if (RxLEDOn){
      if (RxLEDState){
        RxLEDState = false;
        digitalWrite(RxLED,RxLEDState);
      }
      else{
        RxLEDState = true;
        digitalWrite(RxLED,RxLEDState);
      }
    }
    if (ShowTime){
      Serial.print("(");
      Serial.print(SerialTimer);
      Serial.print(")");
      Serial.print(" ");
    }
    if (ShowPort){
      Serial.print("SP");
      Serial.print(selfPN);
      Serial.print(" ");
    }
    if (ShowLength){
      Serial.print("[");
      Serial.print(J1708FrameLength);
      Serial.print("]");
      Serial.print(" ");
    }
    for (int i = 1; i < J1708FrameLength; i++){ //start at 1 to exclude 0x00 start value
      J1708Checksum += J1708RxBuffer[i];
      if (ShowRxData){
        sprintf(hexDisp,"%02X ",J1708RxBuffer[i]);
        Serial.print(hexDisp);
      }
    }
    if (ShowChecksum){
      uint8_t chk = 0;
      for (int i=1; i<(J1708FrameLength);i++){
        chk+=J1708RxBuffer[i];
      }
      chk=((~chk<<24)>>24)+1;
      Serial.print("C:");
      Serial.print(chk);
      Serial.print(" ");
    }
    if (ShowBusload){
      Serial.print("[");Serial.print(busload);Serial.print("] ");
    }
    if (ShowMIDShare){
      Serial.print("[");Serial.print(MIDShareTracker[J1708RxBuffer[1]]);Serial.print("] ");
    }
    if (!ShowTime && !ShowPort && !ShowLength && !ShowRxData && !ShowChecksum && !ShowBusload && !ShowMIDShare){
      //Nothing will be printed because all flags set false
    }
    else{
      Serial.println();
    }
    if(!tx_transmitting){
      if (J1708LoopTimer>P){
        if (fx!=0){
          switch(fx){
            case 1:
              RTS_Handler(Loopbuffer);
              break;
            case 2:
              CTS_Handler(Loopbuffer);
              break;
            case 3:
              EOM_Handler(Loopbuffer);
              break;
            case 4:
              Abort_Handler(Loopbuffer);
              break;
            case 5:
              CDP_Handler(Loopbuffer);
              break;
            default:
              break;
          }
          fx=0;
          P =C_Rate;
          Loop_flag = false;
        }
        else{
          P = N_Rate;
          fx = J1708Parse();
          if (fx>0){
            Loop_flag = true;
          }
          else{
            Loop_flag=false;
          }
        }
        J1708LoopTimer = 0;
      }
      else {
        if (fx>0){
          //Wait Until next scheduled send.
        }
        else{
          fx = J1708Parse();
          if (fx>0){
            Loop_flag = true;
          }
          else{
            Loop_flag=false;
          }
        }    
      }
    }
    else{
      tx_transmitting=false;
    }
  }
  else{
    //Check if a message is in the middle of being received (ie is the line idle?)
    if (!rx_busy){
      uint8_t priority = J1708TxQPriorities[N_TxQ];
      if (J1708TxTimer > (twelvebit+(onebit*priority*2)) + TxQueuePenalty*PenaltyTime){
        // Transport CTS Messages (queue #1)
        if (Q_flag){
          uint8_t Q_Message[Q_Lengths[Q_Counter]] = {};
          for (int i=0;i<Q_Lengths[Q_Counter];i++){
            Q_Message[i] = Q_Matrix[Q_Counter][i];
          }
          J1708Tx(Q_Message,Q_Lengths[Q_Counter],8);
          if (Q_Counter+1==TP_Tx_NSegments){
            Q_flag=false;
            Q_Counter = 0;
          }
          else{
            Q_Counter++;
          }
        }
        // Tx Queue Logic (queue #2)
        else if (N_TxQ_Total > 0){
          if (N_TxQ==TxQmax-1){
            // uint8_t J1708TxBuffer[J1708TxQLengths[N_TxQ]];
            // for (int i=0;i<J1708TxQLengths[N_TxQ];i++){
            //   J1708TxBuffer[i] = J1708TxQ[N_TxQ][i];
            // }
            //Serial.print("Before: N_TxQ:");Serial.print(N_TxQ);Serial.print(",N_TxQ_Total:");Serial.println(N_TxQ_Total);
            J1708Tx(J1708TxQ[N_TxQ],J1708TxQLengths[N_TxQ],J1708TxQPriorities[N_TxQ]);
            N_TxQ=0;
            N_TxQ_Total--;
            //Serial.print("After: N_TxQ:");Serial.print(N_TxQ);Serial.print(",N_TxQ_Total:");Serial.println(N_TxQ_Total);
          }
          else if (N_TxQ>=0){
            // uint8_t J1708TxBuffer[J1708TxQLengths[N_TxQ]];
            // for (int i=0;i<J1708TxQLengths[N_TxQ];i++){
            //   J1708TxBuffer[i] = J1708TxQ[N_TxQ][i];
            // }
            //Serial.print("Before: N_TxQ:");Serial.print(N_TxQ);Serial.print(",N_TxQ_Total:");Serial.println(N_TxQ_Total);
            J1708Tx(J1708TxQ[N_TxQ],J1708TxQLengths[N_TxQ],J1708TxQPriorities[N_TxQ]);
            N_TxQ++;
            N_TxQ_Total--;
            //Serial.print("After: N_TxQ:");Serial.print(N_TxQ);Serial.print(",N_TxQ_Total:");Serial.println(N_TxQ_Total);
          }
        }
      }
    }
    //Is there a relevant message in the loopbuffer that needs to be handled?
    if (fx!=0){
      switch(fx){
        case 1:
          RTS_Handler(Loopbuffer);
          break;
        case 2:
          CTS_Handler(Loopbuffer);
          break;
        case 3:
          EOM_Handler(Loopbuffer);
          break;
        case 4:
          Abort_Handler(Loopbuffer);
          break;
        case 5:
          CDP_Handler(Loopbuffer);
          break;
        default:
          break;
      }
      fx=0;
      P =C_Rate;
      Loop_flag = false;
    }
  }
  //Periodic Tasks
  UpdateNetworkStatistics();
  J1708CheckNetwork();
  if (ERR8_Timer > ERR8_Interval && ERR7_IDCounter[selfMID]>ERR7_Limit){
    uint8_t msg[9] = {selfMID,255,255,250,3,2,selfMID,(uint8_t)ERR8_Counter,0};
    J1708Send(msg,9,3);
    ERR8_Timer = 0;
  }
}

bool J1708::J1708TransportTx(uint8_t TP_Data[], const uint16_t &nBytes, const uint8_t &D_MID){
  if (!TP_Tx_Flag && !TP_Rx_Flag){
    if (nBytes>21 && nBytes<256){
      int whole_seg = nBytes/TP_Default_Segment_Size;
      int part_seg = nBytes%TP_Default_Segment_Size;
      int seg = whole_seg;
      if (part_seg>1){
        seg++;
      }
      TP_Tx_NBytes=nBytes;
      TP_Tx_NSegments=seg;
      TP_Session_MID = D_MID;
      //Load Data into buffer
      for (int i=0; i<nBytes; i++){
        TP_Tx_Buffer[i] = TP_Data[i];
      }
      uint8_t RTS_message[] = {selfMID,197,5,D_MID,1,(uint8_t)seg,(uint8_t)((nBytes<<8)>>8),(uint8_t)(nBytes>>8),0};
      J1708Send(RTS_message,9,8);
      TP_Tx_Flag = true;
      TP_Session_Timer = 0;
      return 1;
    }
    else{
      //Small enough message to send in normal frame.
      return 0;
    }
  }
  else{
    //Busy with another connection or data too large for TP_Tx_Buffer.
    return 0;
  }
}

void J1708::J1708Log(){
  // This function can replace J1708Listen. It will only print messages to Serial. No other interactions.
  if (J1708Rx(J1708RxBuffer)>0){ //Execute this if the number of recieved bytes is more than zero.
    if (RxLEDOn){
      if (RxLEDState){
        RxLEDState = false;
        digitalWrite(RxLED,RxLEDState);
      }
      else{
        RxLEDState = true;
        digitalWrite(RxLED,RxLEDState);
      }
    }
    if (ShowTime){
      Serial.print("(");
      Serial.print(SerialTimer);
      Serial.print(")");
      Serial.print(" ");
    }
    if (ShowPort){
      Serial.print("SP");
      Serial.print(selfPN);
      Serial.print(" ");
    }
    if (ShowLength){
      Serial.print("[");
      Serial.print(J1708FrameLength);
      Serial.print("]");
      Serial.print(" ");
    }
    for (int i = 1; i < J1708FrameLength; i++){ //start at 1 to exclude 0x00 start value
      J1708Checksum += J1708RxBuffer[i];
      if (ShowRxData){
        sprintf(hexDisp,"%02X ",J1708RxBuffer[i]);
        Serial.print(hexDisp);
      }
    }
    if (ShowChecksum){
      uint8_t chk = 0;
      for (int i=1; i<(J1708FrameLength);i++){
        chk+=J1708RxBuffer[i];
      }
      chk=((~chk<<24)>>24)+1;
      Serial.print("C:");
      Serial.print(chk);
      Serial.print(" ");
    }
    if (ShowBusload){
      Serial.print("[");Serial.print(busload);Serial.print("] ");
    }
    if (ShowMIDShare){
      Serial.print("[");Serial.print(MIDShareTracker[J1708RxBuffer[1]]);Serial.print("] ");
    }
    if (!ShowTime && !ShowPort && !ShowLength && !ShowRxData && !ShowChecksum && !ShowBusload && !ShowMIDShare){
      //Nothing will be printed because all flags set false
    }
    else{
      Serial.println();
    }
  }
}

void J1708::J1708Update(){
  if (selfMode==Gateway){
    J1708Listen();
  }
  else if (selfMode==Observer){
    J1708Log();
  }
}

bool J1708::J1708Settings(String &command){
  // Commands contain 4 fields: cmd spx | sbc | opt | val
  if (ShowCommand){
    Serial.println(command);
  }
  String temp;
  if (getValue(command,' ',0)=="j1708config"){
    temp = getValue(command,' ',2);
    if (temp=="-g"){
      if (getValue(command,' ',3)=="-h"){
        if (getValue(command,' ',4)=="0"){
          selfHostPort = false;
          return true;
        }
        else if (getValue(command,' ',4)=="1"){
          selfHostPort = true;
          return true;
        }
        return false;
      }
      else if (getValue(command,' ',3)=="-f"){
        if (getValue(command,' ',4)=="0"){
          Rx_Forwarding = false;
          return true;
        }
        else if (getValue(command,' ',4)=="1"){
          Rx_Forwarding = true;
          return true;
        }
        return false;
      }
      else if (getValue(command,' ',3)=="-a"){
        if (getValue(command,' ',4)>0){
          int temp = string2Hex(getValue(command,' ',4));
          if (temp>=0){
            J1708UpdateACL(temp,true);
            Serial.print("MID added to blocklist:");Serial.println(temp);
            return true;
          }
          else{
            return false;
          }
        }
      }
      else if (getValue(command,' ',3)=="-m"){
        if (getValue(command,' ',4)>0){
          int temp = string2Hex(getValue(command,' ',4));
          if (temp>=0){
            selfMID = (uint8_t)temp;
            Serial.print("Self_MID changed to ");Serial.println(temp);
            return true;
          }
          else{
            return false;
          }
        }
      }
      else if (getValue(command,' ',3)=="-M"){
        if (getValue(command,' ',4)>0){
          temp = getValue(command,' ',4);
          float target = (float)temp.toFloat();
          if (temp>=0){
            maxMIDShare = target;
            Serial.print("Max_MID changed to ");Serial.println(target);
            return true;
          }
          else{
            return false;
          }
        }
      }
      else if (getValue(command,' ',3)=="-b"){
        if (getValue(command,' ',4)>0){
          temp = getValue(command,' ',4);
          float target = (float)temp.toFloat();
          if (temp>=0){
            maxBusload = target;
            Serial.print("Max_Busload changed to ");Serial.println(target);
            return true;
          }
          else{
            return false;
          }
        }
      }
      else if (getValue(command,' ',3)=="-r"){
        if (getValue(command,' ',4)>0){
          int temp = string2Hex(getValue(command,' ',4));
          if (temp>=0){
            J1708UpdateACL(temp,false);
            Serial.print("MID removed from blocklist:");Serial.println(temp);
            return true;
          }
          else{
            return false;
          }
        }
      }
      if (getValue(command,' ',3)=="-p"){
        if (getValue(command,' ',4)=="0"){
          GatewaySpecificProcessing = false;
          return true;
        }
        else if (getValue(command,' ',4)=="1"){
          GatewaySpecificProcessing = true;
          return true;
        }
        return false;
      }
      return false;
    }
    else if (temp=="-h"){
      Serial.print("j1708config sp<port_no> <subcommand>\n  -g GATEWAY <option> <value>\n    -a <MID>      add MID to ACL\n    -b <float>    max allowable busload\n    -h <0|1>      designate port as 'host port'\n    -f <0|1>      forward rx data to linked port\n    -m <MID>      change the gateway MID (ACL settings preserved)\n    -M <float>    max allowable MID share of max busload\n    -p <0|1>      process gateway specific requests\n    -r <MID>      remove MID from ACL \n  -h HELP\n  -H HARDWARE <option> <value>\n    -r <0|1>      rx LED ON/OFF \n    -t <0|1>      tx LED ON/OFF \n    -s <0|1>      security LED ON/OFF \n  -r RESET <option>\n    -a            ACL allow all\n    -b            ACL block all\n    -c            message counters\n    -e            error counters\n    -t            message timer\n  -s SHOW <option> <value>\n    -a            all\n    -A <0|1>      show ACL\n    -b <0|1>      busload\n    -c <0|1>      checksum\n    -C <0|1>      command\n    -d            default\n    -e <0|1>      non-security errors\n    -l <0|1>      data length\n    -m <0|1>      busload by MID\n    -n            none\n    -p <0|1>      port\n    -r <0|1>      rx data\n    -s            statistics\n    -T <0|1>      time\n");
      return true;
    }
    else if (temp=="-H"){
      if (getValue(command,' ',3)=="-r"){
        if (getValue(command,' ',4)=="0"){
          RxLEDOn = false;
          return true;
        }
        else if (getValue(command,' ',4)=="1"){
          RxLEDOn = true;
          return true;
        }
        return false;
      }
      if (getValue(command,' ',3)=="-s"){
        if (getValue(command,' ',4)=="0"){
          SECLEDOn = false;
          return true;
        }
        else if (getValue(command,' ',4)=="1"){
          SECLEDOn = true;
          return true;
        }
        return false;
      }
      else if (getValue(command,' ',3)=="-t"){
        if (getValue(command,' ',4)=="0"){
          TxLEDOn = false;
          return true;
        }
        else if (getValue(command,' ',4)=="1"){
          TxLEDOn = true;
          return true;
        }
        return false;
      }
      return false;
    }
    else if (temp=="-r"){
      if (getValue(command,' ',3)=="-t"){
        //Reset Serial Timer
        SerialTimer = 0;
        return true;
      }
      else if (getValue(command,' ',3)=="-a"){
        //Reset ACL - Allow All
        J1708ResetACL(false);
        return true;
      }
      else if (getValue(command,' ',3)=="-b"){
        //Reset ACL - Block All
        J1708ResetACL(true);
        return true;
      }
      else if (getValue(command,' ',3)=="-c"){
        //Reset message counters
        RX_Counter = 0;
        TX_Counter = 0;
        FWD_Counter = 0;
        return true;
      }
      else if (getValue(command,' ',3)=="-e"){
        //Reset Error Count
        ERR_Counter = 0;
        ERR1_Counter = 0; // Checksum Error
        ERR2_Counter = 0; // Buffer Overflow Error
        ERR3_Counter = 0; // Transmit Buffer Overflow Error (i.e. Too many messages being sent)
        ERR4_Counter = 0; // Message Collision Error
        ERR5_Counter = 0; // Transmit Error (i.e. Disconnected from bus, hardware fault)
        ERR6_Counter = 0; // High Busload / Flood Error
        ERR7_Counter = 0; // Spoofed Message Error
        ERR8_Counter = 0; // Rogue Node Detected Error
        ERR9_Counter = 0; // Rogue Node Detected Error
        ERR10_Counter = 0; // Rogue Node Detected Error
        for (int i=0;i<256;i++){
          ERR7_IDCounter[i]=0;
          ERR8_Tracker[i]=false;
          ERR9_Tracker[i]=false;
          ERR10_Tracker[i]=false;
        }
        SEC_ERR_Counter = 0;
        ERR1_Checksum = false;
        ERR2_RxOverflow = false;
        ERR3_Tx_Overflow = false;
        ERR4_Collision = false;
        ERR5_DataNotSent = false;
        ERR6_HighBusload = false;
        return true;
      }
      return false;
    }
    else if (temp=="-s"){
      temp = (getValue(command,' ',3));
      if (temp=="-a"){
        ShowRxData = true;
        ShowTime = true;
        ShowPort = true;
        ShowChecksum = true;
        ShowLength = true;
        ShowErrors = true;
        ShowCommand = true;
        ShowBusload = true;
        ShowMIDShare = true;
        return true;
      }
      else if (temp=="-A"){
      Serial.println("ACCESS CONTROL LIST");
      Serial.println("MID:<0-Allowed, 1-Blocked>");
        if (getValue(command,' ',4)=="0"){
          for (int i=0;i<=255;i++){
            if (selfACL[i]==0){
              Serial.print(i);Serial.print(":");Serial.println(selfACL[i]);
            }
          }
          return true;
        }
        else if (getValue(command,' ',4)=="1"){
          for (int i=0;i<=255;i++){
            if (selfACL[i]==1){
              Serial.print(i);Serial.print(":");Serial.println(selfACL[i]);
            }
          }
          return true;
        }
        else{
          for (int i=0;i<=255;i++){
            Serial.print(i);Serial.print(":");Serial.println(selfACL[i]);
          }
          return true;
        }
      }
      else if (temp=="-b"){
        if (getValue(command,' ',4)=="0"){
          ShowBusload = false;
          return true;
        }
        else if (getValue(command,' ',4)=="1"){
          ShowBusload = true;
          return true;
        }
        return false;
      }
      else if (temp=="-c"){
        if (getValue(command,' ',4)=="0"){
          ShowChecksum = false;
          return true;
        }
        else if (getValue(command,' ',4)=="1"){
          ShowChecksum = true;
          return true;
        }
        return false;
      }
      else if (temp=="-C"){
        if (getValue(command,' ',4)=="0"){
          ShowCommand = false;
          return true;
        }
        else if (getValue(command,' ',4)=="1"){
          ShowCommand = true;
          return true;
        }
        return false;
      }
      if (temp=="-d"){
        ShowRxData = true;
        ShowTime = true;
        ShowPort = true;
        ShowChecksum = false;
        ShowLength = true;
        ShowErrors = true;
        ShowCommand = true;
        ShowBusload = false;
        ShowMIDShare = false;
        return true;
      }
      else if (temp=="-e"){
        if (getValue(command,' ',4)=="0"){
          ShowErrors = false;
          return true;
        }
        else if (getValue(command,' ',4)=="1"){
          ShowErrors = true;
          return true;
        }
        return false;
      }
      else if (temp=="-i"){
        Serial.println("SYSTEM INFORMATION");
        Serial.print("Mode:");Serial.print(selfMode);
          if (selfMode==0){
            Serial.println(" (Gateway)");
          }
          else if (selfMode==0){
            Serial.println(" (Rogue)");
          }
          else if (selfMode==0){
            Serial.println(" (Compromised)");
          }
          else if (selfMode==0){
            Serial.println(" (Observer)");
          }
          else{
            Serial.println();
          }
        Serial.print("Port:");Serial.println(selfPN);
        if (J1708Object_Linked){
          Serial.print("Linked_Port:");Serial.println(_j1708Ref->selfPN);
        }
        else{
          Serial.print("Linked_Port:");Serial.println("-");
        }
        Serial.print("Self_MID:");Serial.println(selfMID);
        Serial.print("Max_Busload:");Serial.println(maxBusload);
        Serial.print("Max_MID%:");Serial.println(maxMIDShare);
        Serial.print("TxBucketSize:");Serial.println(TxQmax);
        if (selfHostPort){
          Serial.print("Host_Port:");Serial.println("True");
        }
        else {
          Serial.print("Host_Port:");Serial.println("False");
        }
        if (GatewaySpecificProcessing){
          Serial.print("Gateway_Prc:");Serial.println("True");
        }
        else {
          Serial.print("Gateway_Prc:");Serial.println("False");
        }
        if (Rx_Forwarding){
          Serial.print("Msg_Forwarding:");Serial.println("True");
        }
        else {
          Serial.print("Msg_Forwarding:");Serial.println("False");
        }
        return true;
      }
      else if (temp=="-l"){
        if (getValue(command,' ',4)=="0"){
          ShowLength = false;
          return true;
        }
        else if (getValue(command,' ',4)=="1"){
          ShowLength = true;
          return true;
        }
        return false;
      }
      else if (temp=="-n"){
        ShowRxData = false;
        ShowTime = false;
        ShowPort = false;
        ShowChecksum = false;
        ShowLength = false;
        ShowErrors = false;
        ShowCommand = false;
        ShowBusload = false;
        ShowMIDShare = false;
        return true;
      }
      else if (temp=="-m"){
        if (getValue(command,' ',4)=="0"){
          ShowMIDShare = false;
          return true;
        }
        else if (getValue(command,' ',4)=="1"){
          ShowMIDShare = true;
          return true;
        }
        return false;
      }
      else if (temp=="-p"){
        if (getValue(command,' ',4)=="0"){
          ShowPort = false;
          return true;
        }
        else if (getValue(command,' ',4)=="1"){
          ShowPort = true;
          return true;
        }
        return false;
      }
      else if (temp=="-r"){
        if (getValue(command,' ',4)=="0"){
          ShowRxData = false;
          return true;
        }
        else if (getValue(command,' ',4)=="1"){
          ShowRxData = true;
          return true;
        }
        return false;
      }
      else if (temp=="-s"){
        Serial.println("SYSTEM STATISTICS");
        Serial.print("Bus_Load:");Serial.print(busload*100.0);Serial.println("%");
        Serial.print("Total_Error_Count:");Serial.println(ERR_Counter);
        Serial.print("  ERR1_Count:");Serial.println(ERR1_Counter);
        Serial.print("  ERR2_Count:");Serial.println(ERR2_Counter);
        Serial.print("  ERR3_Count:");Serial.println(ERR3_Counter);
        Serial.print("  ERR4_Count:");Serial.println(ERR4_Counter);
        Serial.print("  ERR5_Count:");Serial.println(ERR5_Counter);
        Serial.print("  ERR6_Count:");Serial.println(ERR6_Counter);
        Serial.print("  ERR7_Count:");Serial.println(ERR7_Counter);
        Serial.print("  ERR8_Count:");Serial.println(ERR8_Counter);
        Serial.print("  ERR9_Count:");Serial.println(ERR9_Counter);
        Serial.print("  ERR10_Count:");Serial.println(ERR10_Counter);
        Serial.print("Security_Alerts:");Serial.println(SEC_ERR_Counter);
        Serial.print("Total_Received_Messages:");Serial.println(RX_Counter);
        Serial.print("Total_Transmitted_Messages:");Serial.println(TX_Counter);
        Serial.print("Total_Forwarded_Messages:");Serial.println(FWD_Counter);
        Serial.print("Message_Timer_Micros:");Serial.println(SerialTimer);
        Serial.print("System_Timer_Millis:");Serial.println(millis());
        return true;
      }
      else if (temp=="-T"){
        if (getValue(command,' ',4)=="0"){
          ShowTime = false;
          return true;
        }
        else if (getValue(command,' ',4)=="1"){
          ShowTime = true;
          return true;
        }
        return false;
      }
      return false;
    }
    return false;
  }
  else if (getValue(command,' ',0)=="j1708send"){
    temp = getValue(command,' ',2);
    if (temp=="-h"){
      Serial.print("j1708send sp<port_no> <option> <param1> <param2>\n        SEND MESSAGE\n            <payload_size> <payload>            send data w/o checksum (automatically calculated and appended)\n            EXAMPLE:\n            j1708send sp3 4 DE.AD.be.ef         send a message to port three with MID 0xDE\n\n    -T  SEND TRANSPORT MESSAGE\n            <dst.MID> <payload_size> <payload>  Send a payload using J1587 transport protocol. Automatic RTS\n                                                is sent if possible. Automatic handling. Connection times\n                                                out after 10s. Meanwhile, no other RTSs can be sent. Max\n                                                payload size is 256-bytes.\n            EXAMPLE:\n            j1708send sp3 -T A1 30 01.02.03. ... .29.30 \n            \n    -h  HELP");
      return true;
    }
    else if (temp=="-T"){
      if (getValue(command,' ',3)>0){
        int temp = string2Hex(getValue(command,' ',3));
        if (temp>=0){
          uint8_t destinationAddr = (uint8_t)temp;
          String temp = getValue(command,' ',4);
          if (temp>0 && getValue(command,' ',5)>0){
            int payloadsize = (int)temp.toInt();
            String payload = getValue(command,' ',5);
            // Send a long message
            if (payloadsize>19 && payloadsize<256){
              uint8_t msg[payloadsize];
              uint8_t byte_i;
              for (int i=0;i<payloadsize;i++){
                if (getValue(payload,'.',i)>0){
                  byte_i = string2Hex(getValue(payload,'.',i));
                  if (byte_i>=0){
                    // This byte was a well formated HEX string
                    // Add it to buffer
                    msg[i] = byte_i;
                  }
                  else {
                    return false;
                  }
                }
                else{
                  return false;
                }
              }
              if (J1708TransportTx(msg,payloadsize,destinationAddr)){
                return true;
              }
              return false;
            }
            return false;
          }
          return false;
        }
        return false;
      }
      return false;
    }
    else if (temp>0 && getValue(command,' ',3)>0){
      int msgLen = (int)temp.toInt();
      String payload = getValue(command,' ',3);
      //Send standard Sized Message
      if (msgLen>0 && msgLen<21){
        uint8_t msg[msgLen+1];
        int byte_i;
        for (int i=0;i<msgLen;i++){
          if (getValue(payload,'.',i)>0){
            byte_i = string2Hex(getValue(payload,'.',i));
            if (byte_i>=0){
              // This byte was a well formated HEX string
              // Add it to buffer
              msg[i] = (uint8_t)byte_i;
            }
            else{
              return false;
            }
          }
          else {
            return false;
          }
        }
        J1708Send(msg,msgLen+1,8);
        return true;
      }
      return false;
    }
    return false;
  }
  return false;
}
