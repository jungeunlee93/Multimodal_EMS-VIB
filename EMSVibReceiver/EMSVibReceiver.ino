/***
  Command Configuration
  (Client's mac address),(Haptic Type),(Haptic Stimuli)
  
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

Scheduler userScheduler; // to control your personal task
painlessMesh  mesh;

uint32_t broadcaster = 0;

hw_timer_t * timer = NULL;
hw_timer_t * timer1 = NULL;

String inString, cmd;

short DAC64bits[128] = {128, 134, 140, 146, 152, 158, 165, 170,
                        176, 182, 188, 193, 198, 203, 208, 213,
                        218, 222, 226, 230, 234, 237, 240, 243,
                        245, 248, 250, 251, 253, 254, 254, 255,
                        255, 255, 254, 254, 253, 251, 250, 248,
                        245, 243, 240, 237, 234, 230, 226, 222,
                        218, 213, 208, 203, 198, 193, 188, 182,
                        176, 170, 165, 158, 152, 146, 140, 134,
                        128, 121, 115, 109, 103, 97, 90, 85,
                        79, 73, 67, 62, 57, 52, 47, 42,
                        37, 33, 29, 25, 21, 18, 15, 12,
                        10, 7, 5, 4, 2, 1, 1, 0,
                        0, 0, 1, 1, 2, 4, 5, 7,
                        10, 12, 15, 18, 21, 25, 29, 33,
                        37, 42, 47, 52, 57, 62, 67, 73,
                        79, 85, 90, 97, 103, 109, 115, 121
                       };

//for EMS
const uint16_t timerTicks = 32768; //32.768 kHz
const double microsToTicks = timerTicks / 1000000.0;
uint16_t signalToggle = 655; // 50Hz
uint16_t pulseToggle = 65; // 2ms pulse
uint16_t curTimerState = 0;
bool startEMS = false;
int cntEMS;
int pinEMS = 12;
int frequencyEMS; // EMS frequency. value: 20 ~ 120 [Hz]
int pulsewidthEMS; // EMS pulse width. value: 100 ~ 400 [us]
int durationEMS;
double onePeriodEMS = 0.0; 
//int amplitude; // EMS amplitude.

//for LRA
bool startLRA = false;
int arrNum;
int strength = 10;
float durationLRA;
double onePeriodLRA = 0.0;
int frequencyLRA;
int sampleNum;
int cntLRA;
double decayingRateLRA;

bool isMultimodal = false;
//for Dual Core
TaskHandle_t Task1;

void resetParameters() {
  inString = "";
  frequencyEMS = 100;
  pulsewidthEMS = 200;
  cntLRA = 0;
  arrNum = 0;
  cntEMS = 0;
  sampleNum = 128;
  decayingRateLRA = 0.0;
}

void serialParser(String inString) {
  if(inString.substring(0,1) =="E") { // only EMS
    frequencyEMS = inString.substring(2,4).toInt() * 10;
    signalToggle = timerTicks / frequencyEMS;
    onePeriodEMS = 10.0 / frequencyEMS;
    pulsewidthEMS = inString.substring(4,8).toInt();
    pulseToggle = pulsewidthEMS * microsToTicks;
    durationEMS = inString.substring(8,11).toInt()/10;
  } else if(inString.substring(0,1) =="V") { // only LRA
    strength = inString.substring(2,5).toInt();
    frequencyLRA = inString.substring(5,8).toInt();
    onePeriodLRA = 10.0 / frequencyLRA;
    durationLRA = inString.substring(8,12).toInt() / 100;
    decayingRateLRA = inString.substring(12,15).toDouble();
  } else if(inString.substring(0,1) =="W") { // EMS + LRA
    strength = inString.substring(2,5).toInt();
    frequencyLRA = inString.substring(5,8).toInt();
    onePeriodLRA = 10.0 / frequencyLRA;
    durationLRA = inString.substring(8,12).toInt() / 100;
    decayingRateLRA = inString.substring(12,15).toDouble();
    
    frequencyEMS = inString.substring(15,17).toInt() * 10;
    signalToggle = timerTicks / frequencyEMS;
    onePeriodEMS = 10.0 / frequencyEMS;
    pulsewidthEMS = inString.substring(17,21).toInt();
    pulseToggle = pulsewidthEMS * microsToTicks;
    durationEMS = inString.substring(21,23).toInt()/10;
  }
}

void IRAM_ATTR setEMS(){
  
  if(curTimerState == 0) {
    digitalWrite(pinEMS, HIGH);  
  } else if(curTimerState == pulseToggle) {
    digitalWrite(pinEMS, LOW);
  }
  
  curTimerState = (curTimerState + 1) % (signalToggle + 1);
  
  if(curTimerState == 0 ) {
    cntEMS = cntEMS + 1;
    if(onePeriodEMS * cntEMS >= durationEMS) {      
      cntEMS = 0;  
      timerAlarmDisable(timer);
      startEMS = true;      
    }
  }
}

void IRAM_ATTR setLRA() {
  dac_output_voltage(DAC_CHANNEL_1, exp(-decayingRateLRA/1000*(128*cntLRA+arrNum)/sampleNum)*(DAC64bits[arrNum]*strength/100 + 128 - 128 * strength / 100));
  arrNum = (arrNum + 1) & (sampleNum - 1);
  if(arrNum == 0) {
    cntLRA = cntLRA + 1;    
    if(onePeriodLRA * cntLRA >= durationLRA){
      durationLRA = 0;
      cntLRA = 0;
      timerAlarmDisable(timer1);
      startLRA = true;
    }
  }
}

void sendMessage();

Task taskSendMessage( TASK_SECOND * 1 , TASK_FOREVER, &sendMessage );

void sendMessage() {
  String msg =  WiFi.macAddress();
  mesh.sendSingle(broadcaster, msg);
  taskSendMessage.setInterval( TASK_MILLISECOND * 50 );
}

void receivedCallback( uint32_t from, String &msg) {
  
  if(msg.substring(0,17) == WiFi.macAddress()) {
    inString = msg.substring(18);
  } else if(msg == "check") {
    inString = "V,015130050000";
    broadcaster = from;
    mesh.sendSingle(broadcaster, "connect");
  } else if(msg == "hi") {
    broadcaster = from;
  } 
}

void newConnectionCallback(uint32_t nodeId) {
  ;
}

void changedConnectionCallback() {
  ;
}

void nodeTimeAdjustedCallback(int32_t offset) {
  ;
}

void Task1code( void * pvParameters ){
  // For WiFi communication
  for(;;){
    mesh.update();
    vTaskDelay(10);
    }
}

void setup() {
  mesh.setDebugMsgTypes( ERROR | STARTUP );
  Serial.begin(115200);
  Serial.println(WiFi.macAddress());
  mesh.init( MESH_PREFIX, MESH_PASSWORD, &userScheduler, MESH_PORT );
  mesh.onReceive(&receivedCallback);
  mesh.onNewConnection(&newConnectionCallback);
  mesh.onChangedConnections(&changedConnectionCallback);
  mesh.onNodeTimeAdjusted(&nodeTimeAdjustedCallback);
  
  userScheduler.addTask( taskSendMessage );
  taskSendMessage.enable();

  pinMode(pinEMS, OUTPUT);
  
  dac_output_voltage(DAC_CHANNEL_1, 0);
  dac_output_enable(DAC_CHANNEL_1);
  resetParameters();

  xTaskCreatePinnedToCore(Task1code, "Task1", 10000, NULL, 1, &Task1, 0);
                    
  // timer for EMS
  timer = timerBegin(0, 2440, true); // 80 MHz / 2440 = 32.768 KHz
  timerAttachInterrupt(timer, &setEMS, true);
  timerAlarmWrite(timer, 1, true); // Interrupt Frequency: 32.768 KHz
  // timer for LRA
  timer1 = timerBegin(1, 80, true); // 80 MHz / 80 = 1 MHz
  timerAttachInterrupt(timer1, &setLRA, true);
  timerAlarmWrite(timer1, 156, true); // Interrupt (Vibration) Frequency
  // 1000000 / (sampleNumber * frequencyLRA)
  // 78: 100Hz, 156: 50Hz
  
  inString = "V,015130050000"; //E101010 V3130200
}

void loop() {
  if(inString != ""){
    startEMS = false;
    startLRA = false;

    if(inString.substring(0,1) =="E") {
      serialParser(inString);
      if(!startEMS) {
        timerAlarmEnable(timer);
      }
    }
    else if(inString.substring(0,1) == "V") {   
      serialParser(inString);
      timerAlarmWrite(timer1, 1000000 / (128 * frequencyLRA) , true);
      if(!startLRA){
        timerAlarmEnable(timer1);
      }
    }
    else if(inString =="@") {
      timerAlarmDisable(timer);
      timerAlarmDisable(timer1);
    }
    else if(inString.substring(0,1) == "W") {
      serialParser(inString);
      if(!startEMS && !startLRA) {
        timerAlarmEnable(timer);
        timerAlarmEnable(timer1);   
      }
    }
    inString = "";
  }
  if(startEMS && cntEMS == 0) {
    timerAlarmDisable(timer);
    digitalWrite(pinEMS, LOW);
  }
  if(startLRA && cntLRA == 0){
    timerAlarmDisable(timer1);
  }
}
