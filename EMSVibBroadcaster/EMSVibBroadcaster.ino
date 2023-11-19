/***
  Command Configuration
  (STX)(Client's mac address),(Haptic Type),(Haptic Stimuli)(ETX)
  
  Haptic Type  
  V - vibration only
  E - EMS Only
  W - vibration + EMS
  
  Haptic Stimuli
  case V: (Vibration Amplitude; length 3)(Vibration Frequency; length 3;)(Vibration Duration; length 4)(Decaying Rate; length 3)
  case E: (EMS Frequency; length 2)(EMS Pulse Width; length 4)(EMS Duration; length 3)
  case M: (Vibration Amplitude; length 3)(Vibration Frequency; length 3;)(Vibration Duration; length 4)(Decaying Rate; length 3)(EMS Frequency; length 2)(EMS Pulse Width; length 4)(EMS Duration; length 3)

  For example, 
  E8:31:CD:A3:13:1C,V,0701200200057 -> Mac Address: E8:31:CD:A3:13:1C; Haptic Type: V; Vibration Amplitude 70 %, Frequency 120 Hz, Duration 200 ms, Decaying Rate 057 (1/s)
  E8:31:CD:A3:13:1C,E,120300010 -> Mac Address: E8:31:CD:A3:13:1C; Haptic Type: E; EMS Frequency: 120 Hz, Pulse Width: 300 us, Duration 100 ms
  E8:31:CD:A3:13:1C,W,0701200200057120300010-> Mac Address: E8:31:CD:A3:13:1C; Haptic Type: W; Vibration Amplitude 70 %, Frequency 120 Hz, Duration 200 ms, Decaying Rate 057 (1/s)
                                                                                            EMS Frequency: 120 Hz, Pulse Width: 300 us, Duration 100 ms
  Command should be send the HEX code.  
  ***DON'T SEND USING ARDUINO'S SERIAL MONITOR.***
  Instead of Arduino Serial Monitor, 'hterm' is recommended as a command delivery program.
***/

#include <driver/dac.h>
#include <AsyncTCP.h>
#include "painlessMesh.h"
#include "IPAddress.h"

#define   MESH_PREFIX     "HiHS2024"
#define   MESH_PASSWORD   "HiHS2024"
#define   MESH_PORT       6741

String msg = "";
int connectDevice = 0;
int macAddIdx = 0;
String macAddmsg = "";
int macAddCnt = 0;

Scheduler userScheduler;
painlessMesh  mesh;

// User stub
void sendMessage();

Task taskSendMessage( TASK_MILLISECOND * 10 , TASK_FOREVER, &sendMessage ); // start with ten milliseconds interval


// For Serial Communication from Unity to ESP32 (server)
#define STX 0x02
#define ETX 0x03

#define DEVICE_REQ  0x55
#define DEVICE_ACK  0x56
#define DEVICE_MAC  0x57

#define MSG_LEN_ERR -1
#define MSG_STX_ERR -2
#define MSG_ETX_ERR -3
#define MSG_DEV_OK  0
#define MSG_REQ_OK  1

#define MAXREQSZ  80

char reqBuf[MAXREQSZ];
int reqBufPos = 0;
byte DeviceAck[3] = {0x02, 0x56, 0x03};

void sendMsgToPC(String msg) {
  Serial.write(STX);
  Serial.print(msg); // Mac address  
  Serial.write(ETX);  
  macAddmsg = "";
}

int MessageProcess(String &msg) {  
  if(reqBufPos < 3) return (MSG_LEN_ERR);
  if(reqBuf[0] != STX) return (MSG_STX_ERR);
  if(reqBuf[reqBufPos-1] != ETX) return (MSG_ETX_ERR);

  if((reqBufPos == 3) && (reqBuf[1] == DEVICE_REQ)) {
    // Check the connection between Agent
    Serial.write(DeviceAck, 3);
    return (MSG_DEV_OK);
  }

  if((reqBufPos == 3) && (reqBuf[1] == DEVICE_MAC)) {
    sendMsgToPC(macAddmsg);    
    macAddmsg = "";
    return (MSG_DEV_OK);
  }
  
  msg = "";
  for (int i = 1; i < (reqBufPos - 1); i++) {
    msg.concat(reqBuf[i]);
  }

  return (MSG_REQ_OK);  
}

void MessageCommCheck() {
  String msg;
  
  if (!Serial.available()) return;
  char ch = Serial.read();
  switch (ch) {
    case STX:
      reqBufPos = 0;
      reqBuf[reqBufPos++] = ch;
      break;

    case ETX:
      reqBuf[reqBufPos++] = ch;
      if (MessageProcess(msg) == MSG_REQ_OK) {
        mesh.sendBroadcast(msg);
        taskSendMessage.setInterval(TASK_MILLISECOND * 1);
        //Serial.print(msg);
      }
      reqBufPos = 0;
      break;

    default:
      reqBuf[reqBufPos++] = ch;
      if (reqBufPos >= MAXREQSZ)
        reqBufPos = 0;
  }
}

void sendMessage() {
}

void receivedCallback( uint32_t from, String &msg ) {
  if(msg == "connect") {
      msg == "";
  } else if(macAddmsg.length() <= 18) {
    if(macAddmsg == "") {
      macAddmsg = msg;
    } else if(macAddmsg != msg) {
      macAddmsg += ",";
      macAddmsg += msg;  
    }        
  }  
  if(macAddmsg != "") {
    macAddCnt = 0;
  }
}

void newConnectionCallback(uint32_t nodeId) {
  mesh.sendBroadcast("check");
  Serial.println("Check Connect");
  macAddCnt = 0;
  taskSendMessage.setInterval(TASK_MILLISECOND * 1);
  Serial.println("Connected Haptic device: 8C:4B:14:71:B4:BC");
}

void changedConnectionCallback() {
  mesh.sendBroadcast("hi");
}

void nodeTimeAdjustedCallback(int32_t offset) {
}

void setup() { 
  Serial.begin(115200);
  Serial.setTimeout(10);
  
  mesh.setDebugMsgTypes( ERROR | STARTUP );

  mesh.init( MESH_PREFIX, MESH_PASSWORD, &userScheduler, MESH_PORT );
  mesh.onReceive(&receivedCallback);
  mesh.onNewConnection(&newConnectionCallback);
  mesh.onChangedConnections(&changedConnectionCallback);
  mesh.onNodeTimeAdjusted(&nodeTimeAdjustedCallback);
  
  userScheduler.addTask(taskSendMessage);
  taskSendMessage.enable();
}

void loop() {
  mesh.update();
  MessageCommCheck();
}
