#include <Arduino.h>
#include "ODriveCAN.h"

//PID
#include <PID_v1.h>

// PID1
double Pk1 = 2.5;   // 4.5
double Ik1 = 15;    // 12
double Dk1 = 0.07;  // 0.07

double Setpoint1, Input1, Output1, Output1a;    // PID variables
PID PID1(&Input1, &Output1, &Setpoint1, Pk1, Ik1 , Dk1, DIRECT);    // PID Setup

// CAN bus baudrate. Make sure this matches for every device on the bus
#define CAN_BAUDRATE 250000

// ODrive node_id for odrvives
#define ODRV0_NODE_ID 0
#define ODRV1_NODE_ID 1
#define ODRV2_NODE_ID 2
#define ODRV3_NODE_ID 3
#define ODRV4_NODE_ID 4

// Uncomment below the line that corresponds to your hardware.
// See also "Board-specific settings" to adapt the details for your hardware setup.

#define IS_TEENSY_BUILTIN // Teensy boards with built-in CAN interface (e.g. Teensy 4.1). See below to select which interface to use.
// #define IS_ARDUINO_BUILTIN // Arduino boards with built-in CAN interface (e.g. Arduino Uno R4 Minima)
// #define IS_MCP2515 // Any board with external MCP2515 based extension module. See below to configure the module.


/* Board-specific includes ---------------------------------------------------*/

#if defined(IS_TEENSY_BUILTIN) + defined(IS_ARDUINO_BUILTIN) + defined(IS_MCP2515) != 1
#warning "Select exactly one hardware option at the top of this file."

#if CAN_HOWMANY > 0 || CANFD_HOWMANY > 0
#define IS_ARDUINO_BUILTIN
#warning "guessing that this uses HardwareCAN"
#else
#error "cannot guess hardware version"
#endif

#endif

#ifdef IS_ARDUINO_BUILTIN
// See https://github.com/arduino/ArduinoCore-API/blob/master/api/HardwareCAN.h
// and https://github.com/arduino/ArduinoCore-renesas/tree/main/libraries/Arduino_CAN

#include <Arduino_CAN.h>
#include <ODriveHardwareCAN.hpp>
#endif // IS_ARDUINO_BUILTIN

#ifdef IS_MCP2515
// See https://github.com/sandeepmistry/arduino-CAN/
#include "MCP2515.h"
#include "ODriveMCPCAN.hpp"
#endif // IS_MCP2515

# ifdef IS_TEENSY_BUILTIN
// See https://github.com/tonton81/FlexCAN_T4
// clone https://github.com/tonton81/FlexCAN_T4.git into /src
#include <FlexCAN_T4.h>
#include "ODriveFlexCAN.hpp"
struct ODriveStatus; // hack to prevent teensy compile error
#endif // IS_TEENSY_BUILTIN

/* Board-specific settings ---------------------------------------------------*/

/* Teensy */

#ifdef IS_TEENSY_BUILTIN

FlexCAN_T4<CAN1, RX_SIZE_256, TX_SIZE_16> can_intf;

bool setupCan() {
  can_intf.begin();
  can_intf.setBaudRate(CAN_BAUDRATE);
  can_intf.setMaxMB(16);
  can_intf.enableFIFO();
  can_intf.enableFIFOInterrupt();
  can_intf.onReceive(onCanMessage);
  return true;
}

#endif // IS_TEENSY_BUILTIN

/* MCP2515-based extension modules -*/

#ifdef IS_MCP2515

MCP2515Class& can_intf = CAN;

// chip select pin used for the MCP2515
#define MCP2515_CS 10

// interrupt pin used for the MCP2515
// NOTE: not all Arduino pins are interruptable, check the documentation for your board!
#define MCP2515_INT 2

// freqeuncy of the crystal oscillator on the MCP2515 breakout board. 
// common values are: 16 MHz, 12 MHz, 8 MHz
#define MCP2515_CLK_HZ 8000000


static inline void receiveCallback(int packet_size) {
  if (packet_size > 8) {
    return; // not supported
  }
  CanMsg msg = {.id = (unsigned int)CAN.packetId(), .len = (uint8_t)packet_size};
  CAN.readBytes(msg.buffer, packet_size);
  onCanMessage(msg);
}

bool setupCan() {
  // configure and initialize the CAN bus interface
  CAN.setPins(MCP2515_CS, MCP2515_INT);
  CAN.setClockFrequency(MCP2515_CLK_HZ);
  if (!CAN.begin(CAN_BAUDRATE)) {
    return false;
  }

  CAN.onReceive(receiveCallback);
  return true;
}

#endif // IS_MCP2515


/* Arduinos with built-in CAN */

#ifdef IS_ARDUINO_BUILTIN

HardwareCAN& can_intf = CAN;

bool setupCan() {
  return can_intf.begin((CanBitRate)CAN_BAUDRATE);
}

#endif

// Instantiate ODrive objects
ODriveCAN odrv0(wrap_can_intf(can_intf), ODRV0_NODE_ID); // Standard CAN message ID
ODriveCAN odrv1(wrap_can_intf(can_intf), ODRV1_NODE_ID); // Standard CAN message ID
ODriveCAN odrv2(wrap_can_intf(can_intf), ODRV2_NODE_ID); // Standard CAN message ID
ODriveCAN odrv3(wrap_can_intf(can_intf), ODRV3_NODE_ID); // Standard CAN message ID
ODriveCAN odrv4(wrap_can_intf(can_intf), ODRV4_NODE_ID); // Standard CAN message ID
ODriveCAN* odrives[] = {&odrv0, &odrv1, &odrv2, &odrv3, &odrv4}; // Make sure all ODriveCAN instances are accounted for here

struct ODriveUserData {
  Heartbeat_msg_t last_heartbeat;
  bool received_heartbeat = false;
  Get_Encoder_Estimates_msg_t last_feedback;
  bool received_feedback = false;
};

// Keep some application-specific user data for every ODrive.
ODriveUserData odrv0_user_data;
ODriveUserData odrv1_user_data;
ODriveUserData odrv2_user_data;
ODriveUserData odrv3_user_data;
ODriveUserData odrv4_user_data;

// Called every time a Heartbeat message arrives from the ODrive
void onHeartbeat(Heartbeat_msg_t& msg, void* user_data_ptr) {
  ODriveUserData* odrv_user_data_ptr = static_cast<ODriveUserData*>(user_data_ptr);
  odrv_user_data_ptr->last_heartbeat = msg;
  odrv_user_data_ptr->received_heartbeat = true;
}

// Called every time a feedback message arrives from the ODrive
void onFeedback(Get_Encoder_Estimates_msg_t& msg, void* user_data_ptr) {
  ODriveUserData* odrv_user_data_ptr = static_cast<ODriveUserData*>(user_data_ptr);
  odrv_user_data_ptr->last_feedback = msg;
  odrv_user_data_ptr->received_feedback = true;
}

// Called for every message that arrives on the CAN bus
void onCanMessage(const CanMsg& msg) {
  for (auto odrive: odrives) {
    onReceive(msg, *odrive);
  }

}

// Uncomment below the line that corresponds to your hardware.
// See also "Board-specific settings" to adapt the details for your hardware setup.

#define IS_TEENSY_BUILTIN // Teensy boards with built-in CAN interface (e.g. Teensy 4.1). See below to select which interface to use.
// #define IS_ARDUINO_BUILTIN // Arduino boards with built-in CAN interface (e.g. Arduino Uno R4 Minima)
// #define IS_MCP2515 // Any board with external MCP2515 based extension module. See below to configure the module.

#include <Wire.h>

// #include <FastGPIO.h>
// #define APA102_USE_FAST_GPIO

#include <APA102.h>
// Define which pins to use.
const uint8_t dataPin = 2;
const uint8_t clockPin = 3;

// ** DEFINE VARIABLES **

int LED0;
int LED1;
int LED2;
int LED3;
int LED4;
int LED5;
int LED6;

float roll;
float rollTrim;
float rollTrimmed;

float fowardVel;
float fowardVelFiltered;

float rotVel;
float rotVelFiltered;

float wheel1;
float wheel2;
float wheel3;
float wheel4;
float wheel5;

int pot1;   // ctrl panel top pot
float pot2;   // ctrl panel bottom pot
float pot3;   // twist grip RH
float pot3Filtered;
float pot3FilteredConst;
float pot4;   // twist grip LH
float pot4Filtered;
float pot4FilteredConst;
int pot5;
int pot6;
int pot7;
int sw1;    // ctrl panel switch
int sw2;    // handlebar switch RH
int sw3;    // handlebar switch LH

int filterVar;
float leanToSteer;
float SteerToLean;

int clFlag;  // ODrive init flag

int EMflag = 0;  // Emergency stop flat - if it falls over

unsigned long currentMillis;
unsigned long previousMillis = 0;        // set up timers
long interval = 10;             // time constant for timer

// Create an object for writing to the LED strip.
APA102<dataPin, clockPin> ledStrip;

// Set the number of LEDs to control.
const uint16_t ledCount = 7;

#include "SparkFun_BNO08x_Arduino_Library.h"  // CTRL+Click here to get the library: http://librarymanager/All#SparkFun_BNO08x
BNO08x myIMU;

#define BNO08X_INT  17
#define BNO08X_RST  16
#define BNO08X_ADDR 0x4B  // SparkFun BNO08x Breakout (Qwiic) defaults to 0x4B

void setup() {

  pinMode(A10, INPUT);    // ctrl panel top pot
  pinMode(A11, INPUT);    // ctrl panel bottom pot
  pinMode(A12, INPUT);    // external pot blue wire twist grip
  pinMode(A13, INPUT);    // external pot white wire twist grip
  pinMode(A15, INPUT);    // pot
  pinMode(A16, INPUT);    // pot
  pinMode(A17, INPUT);    // pot

  pinMode(4, INPUT_PULLUP);   // ctrl panel switch - motor init
  pinMode(5, INPUT_PULLUP);   // external switch yellow wire - handlebar
  pinMode(6, INPUT_PULLUP);   // external switch green wire - handlebar
  
  Serial.begin(115200);
  Serial.println();
  Serial.println("BNO08x Read Example");

  PID1.SetMode(AUTOMATIC);              
  PID1.SetOutputLimits(-80, 80);
  PID1.SetSampleTime(20);

  Wire.begin();

  if (myIMU.begin(BNO08X_ADDR, Wire, BNO08X_INT, BNO08X_RST) == false) {
    Serial.println("BNO08x not detected at default I2C address. Check your jumpers and the hookup guide. Freezing...");
    ledStrip.startFrame();
    ledStrip.sendColor(LED0, LED0, LED0);
    ledStrip.sendColor(LED1, LED1, LED1);
    ledStrip.sendColor(LED2, LED2, LED2);  
    ledStrip.sendColor(50, LED3, LED3);       // error LED
    ledStrip.sendColor(LED4, LED4, LED4);   
    ledStrip.sendColor(LED5, LED5, LED5);   
    ledStrip.sendColor(LED6, LED6, LED6);  
    ledStrip.endFrame(7);
    while (1)
      ;
  }
  
  Serial.println("BNO08x found!");
  ledStrip.startFrame();
  ledStrip.sendColor(LED0, LED0, LED0);
  ledStrip.sendColor(LED1, LED1, LED1);
  ledStrip.sendColor(LED2, LED2, LED2);  
  ledStrip.sendColor(LED1, 50, LED3);       // no error LED
  ledStrip.sendColor(LED4, LED4, LED4);   
  ledStrip.sendColor(LED5, LED5, LED5);   
  ledStrip.sendColor(LED6, LED6, LED6);  
  ledStrip.endFrame(7);

  setReports();

  Serial.println("Reading events");
  delay(100);

  // Wait for up to 3 seconds for the serial port to be opened on the PC side.
  // If no PC connects, continue anyway.
  for (int i = 0; i < 30 && !Serial; ++i) {
    delay(100);
  }
  delay(200);

  Serial.println("Starting ODriveCAN");

  // Register callbacks for the heartbeat and encoder feedback messages
  odrv0.onFeedback(onFeedback, &odrv0_user_data);
  odrv0.onStatus(onHeartbeat, &odrv0_user_data);
  odrv1.onFeedback(onFeedback, &odrv1_user_data);
  odrv1.onStatus(onHeartbeat, &odrv1_user_data);
  odrv2.onFeedback(onFeedback, &odrv2_user_data);
  odrv2.onStatus(onHeartbeat, &odrv2_user_data);
  odrv3.onFeedback(onFeedback, &odrv3_user_data);
  odrv3.onStatus(onHeartbeat, &odrv3_user_data);
  odrv4.onFeedback(onFeedback, &odrv4_user_data);
  odrv4.onStatus(onHeartbeat, &odrv4_user_data);

  // Configure and initialize the CAN bus interface. This function depends on
  // your hardware and the CAN stack that you're using.
  if (!setupCan()) {
    Serial.println("CAN failed to initialize: reset required");
    while (true); // spin indefinitely
  }

    
}

// Here is where you define the sensor outputs you want to receive
void setReports(void) {
  Serial.println("Setting desired reports");
  if (myIMU.enableRotationVector() == true) {
    Serial.println(F("Rotation vector enabled"));
    Serial.println(F("Output in form roll, pitch, yaw"));
  } else {
    Serial.println("Could not enable rotation vector");
  }
}

// motion filter to filter motions

float filter(float prevValue, float currentValue, int filter) {  
  float lengthFiltered =  (prevValue + (currentValue * filter)) / (filter + 1);  
  return lengthFiltered;  
}

void loop() {

      // ** READ IMU DATA **
      if (myIMU.wasReset()) {
        Serial.println("sensor was reset ");
        setReports();
      }    
      // Has a new event come in on the Sensor Hub Bus?
      if (myIMU.getSensorEvent() == true) {    
        // is it the correct sensor data we want?
        if (myIMU.getSensorEventID() == SENSOR_REPORTID_ROTATION_VECTOR) {     
            roll = (myIMU.getRoll()) * 180.0 / PI; // Convert roll to degrees
        }
      }
      // ** END OF READ IMU DATA **

      // ** CAN / ODRIVE STUFF **
      pumpEvents(can_intf); // This is required on some platforms to handle incoming feedback CAN messages


      // ** TIMED LOOP AT 10MS FOR EVERTHING ELSE ** 
      currentMillis = millis();
          if (currentMillis - previousMillis >= 20) {
          previousMillis = currentMillis;

          // ** READ SWITCHES **
          pot1 = analogRead(A10);       // top pot - roll trim
          pot2 = analogRead(A11);       // bottom pot - gain
          pot3 = analogRead(A13);       // LEFT twist
          pot4 = analogRead(A12);       // RIGHT twist
          pot5 = analogRead(A16);       // panel left
          pot6 = analogRead(A17);       // panel centre
          pot7 = analogRead(A15);       // panel right
          sw1 = digitalRead(4);         // ctrl panel switch - motor init
          sw2 = digitalRead(6);         // left handlebar switch
          sw3 = digitalRead(5);         // right handlebar switch

          if (sw1 == 0) {          // Init Odrives

              // Check for Odrives


              Serial.println("Waiting for ODrive 3...");
              while (!odrv3_user_data.received_heartbeat) {
                pumpEvents(can_intf);
                delay(100);
              }
              Serial.println("found ODrive 3");
              
              Serial.println("Waiting for ODrive 0...");
              while (!odrv0_user_data.received_heartbeat) {
                pumpEvents(can_intf);
                delay(100);
              }
              Serial.println("found ODrive 0");  


              Serial.println("Waiting for ODrive 1...");
              while (!odrv1_user_data.received_heartbeat) {
                pumpEvents(can_intf);
                delay(100);
              }
              Serial.println("found ODrive 1");
             
              Serial.println("Waiting for ODrive 2...");
              while (!odrv2_user_data.received_heartbeat) {
                pumpEvents(can_intf);
                delay(100);
              }
              Serial.println("found ODrive 2");   

              Serial.println("Waiting for ODrive 4...");
              while (!odrv4_user_data.received_heartbeat) {
                pumpEvents(can_intf);
                delay(100);
              }
              Serial.println("found ODrive 4"); 
              
              odrv0.setControllerMode(CONTROL_MODE_VELOCITY_CONTROL, INPUT_MODE_PASSTHROUGH);
              odrv1.setControllerMode(CONTROL_MODE_VELOCITY_CONTROL, INPUT_MODE_PASSTHROUGH);
              odrv2.setControllerMode(CONTROL_MODE_VELOCITY_CONTROL, INPUT_MODE_PASSTHROUGH);
              odrv3.setControllerMode(CONTROL_MODE_VELOCITY_CONTROL, INPUT_MODE_PASSTHROUGH);
              odrv4.setControllerMode(CONTROL_MODE_VELOCITY_CONTROL, INPUT_MODE_PASSTHROUGH);
          
              for (int i = 0; i < 15; ++i) {
                delay(10);
                pumpEvents(can_intf);
              }


              Serial.println("Enabling closed loop control 3...");
              while (odrv3_user_data.last_heartbeat.Axis_State != ODriveAxisState::AXIS_STATE_CLOSED_LOOP_CONTROL) {
                odrv3.clearErrors();
                delay(1);
                odrv3.setState(ODriveAxisState::AXIS_STATE_CLOSED_LOOP_CONTROL);
              } 
            
              Serial.println("Enabling closed loop control 0...");
                while (odrv0_user_data.last_heartbeat.Axis_State != ODriveAxisState::AXIS_STATE_CLOSED_LOOP_CONTROL) {
                odrv0.clearErrors();
                delay(1);
                odrv0.setState(ODriveAxisState::AXIS_STATE_CLOSED_LOOP_CONTROL);
              }

              Serial.println("Enabling closed loop control 1...");
              while (odrv1_user_data.last_heartbeat.Axis_State != ODriveAxisState::AXIS_STATE_CLOSED_LOOP_CONTROL) {
                odrv1.clearErrors();
                delay(1);
                odrv1.setState(ODriveAxisState::AXIS_STATE_CLOSED_LOOP_CONTROL);
              }
            
              Serial.println("Enabling closed loop control 2...");
              while (odrv2_user_data.last_heartbeat.Axis_State != ODriveAxisState::AXIS_STATE_CLOSED_LOOP_CONTROL) {
                odrv2.clearErrors();
                delay(1);
                odrv2.setState(ODriveAxisState::AXIS_STATE_CLOSED_LOOP_CONTROL);
              }

              Serial.println("Enabling closed loop control 4...");
              while (odrv4_user_data.last_heartbeat.Axis_State != ODriveAxisState::AXIS_STATE_CLOSED_LOOP_CONTROL) {
                odrv4.clearErrors();
                delay(1);
                odrv4.setState(ODriveAxisState::AXIS_STATE_CLOSED_LOOP_CONTROL);
              }
         
              
              clFlag = 1;          // reset flag
          }

          else if (sw1 == 1) {      // make sure we only power on ODrives once per button press
            clFlag = 0;
          }

          rollTrim = (float) (pot1 - 512)/100;
          rollTrimmed = ((roll*-1) + rollTrim);

   
          // display IMU data on LEDs
      
          if (rollTrimmed > -1.5 && rollTrimmed < 1.5) {
            LED0 = 0;
            LED1 = 0;
            LED2 = 0;
            LED3 = 50;
            LED4 = 0;
            LED5 = 0;
            LED6 = 0;      
          }
          else if (rollTrimmed > -3 && rollTrimmed < -1.5) {
            LED0 = 0;
            LED1 = 0;
            LED2 = 50;
            LED3 = 0;
            LED4 = 0;
            LED5 = 0;
            LED6 = 0;     
          }
          else if (rollTrimmed < 3 && rollTrimmed > 1.5) {
            LED0 = 0;
            LED1 = 0;
            LED2 = 0;
            LED3 = 0;
            LED4 = 50;
            LED5 = 0;
            LED6 = 0;      
          }
          else if (rollTrimmed > -4.5 && rollTrimmed < -3) {
            LED0 = 0;
            LED1 = 50;
            LED2 = 0;
            LED3 = 0;
            LED4 = 0;
            LED5 = 0;
            LED6 = 0;      
          }
          else if (rollTrimmed < -4.5) {
            LED0 = 50;
            LED1 = 0;
            LED2 = 0;
            LED3 = 0;
            LED4 = 0;
            LED5 = 0;
            LED6 = 0;      
          }
          else if (rollTrimmed < 4.5 && rollTrimmed > 3) {
            LED0 = 0;
            LED1 = 0;
            LED2 = 0;
            LED3 = 0;
            LED4 = 0;
            LED5 = 50;
            LED6 = 0;      
          }
          else if (rollTrimmed > 4.5) {
            LED0 = 0;
            LED1 = 0;
            LED2 = 0;
            LED3 = 0;
            LED4 = 0;
            LED5 = 0;
            LED6 = 50;      
          }
          else {
            LED0 = 0;
            LED1 = 0;
            LED2 = 0;
            LED3 = 0;
            LED4 = 0;
            LED5 = 0;
            LED6 = 0;
          }
        
          ledStrip.startFrame();                          // Write to LEDs
          ledStrip.sendColor(LED0, LED0, LED0);
          ledStrip.sendColor(LED1, LED1, LED1);
          ledStrip.sendColor(LED2, LED2, LED2);  
          ledStrip.sendColor(LED3, LED3, LED3);   
          ledStrip.sendColor(LED4, LED4, LED4);   
          ledStrip.sendColor(LED5, LED5, LED5);   
          ledStrip.sendColor(LED6, LED6, LED6);   
          ledStrip.endFrame(7); 

          Input1 = (rollTrimmed) + SteerToLean;                     // use ctrl panel pot for IMU trim
          Setpoint1 = 0;
          PID1.Compute();

          pot2 = pot2 / 1000;                       // ctrl panel pot for manual gain control         
          pot2 = constrain(pot2,0,1);
          Output1a = Output1*pot2;

          // LEFT HAND TWIST
          pot3 = pot3 - 539;          
          
          if (pot3 < 2 && pot3 > 0) {
            pot3 = 0;
          }
          else if (pot3 > 3) {
            pot3 = pot3 - 3;
          }        
          if (pot3 > -3 && pot3 < 0) {
            pot3 = 0;
          }
          else if (pot3 < -3) {
            pot3 = pot3 + 3;
          }        
          if (pot3 > 0) {
            pot3 = pot3 * 1.6;    // scaler on one side
          }        
          
          // RIGHT HAND TWIST       
          pot4 = pot4 - 517;
          if (pot4 < 2 && pot4 > 0) {
            pot4 = 0;
          }
          else if (pot4 > 2) {
            pot4 = pot4 - 2;
          }        
          if (pot4 > -2 && pot4 < 0) {
            pot4 = 0;
          }
          else if (pot4 < -2) {
            pot4 = pot4 + 2;
          }        
          if (pot4 > 0) {
            pot4 = pot4 * 1.25;    // scaler on one side
          }

          filterVar = map(pot7,0,1023,0,60);  // variable motion filter value

          leanToSteer = (pot5/100) * rollTrimmed;
          leanToSteer = constrain(leanToSteer,-10,10);
          
          // post scaler & apply filter
          pot3 = (pot3 / 2) - leanToSteer;
          pot4 = pot4 / 1.5;
          pot3Filtered = filter(pot3, pot3Filtered, filterVar);   // yaw input
          pot4Filtered = filter(pot4, pot4Filtered, filterVar);   // translating back/forward input
          pot3FilteredConst = constrain(pot3Filtered,-20,20);

          SteerToLean = ((pot3Filtered * abs(pot4Filtered/30))) * pot6/1023;

          // calc actual wheel velocities
          wheel1 = (Output1a) + (pot4Filtered) + (pot3FilteredConst * -1);
          wheel2 = (Output1a) + (pot4Filtered * -1) + (pot3FilteredConst * -1);
          wheel3 = (Output1a * -1) + (pot4Filtered) + (pot3FilteredConst * -1);
          wheel4 = (Output1a * -1) + (pot4Filtered * -1) + (pot3FilteredConst * -1); 
          wheel5 = pot4Filtered * 1.5;   //scaling for geometry and there is no reduction on this drive

          if (rollTrimmed < -12 || rollTrimmed > 12) {      // E-stop condition if we tip over too far
            EMflag = 1;
          }

          // ** DRIVE WHEELS UNDER NORMAL CIRUMSTANCES **
          if (EMflag == 0) {                                                              
              odrv1.setVelocity(wheel1);    // back left
              odrv2.setVelocity(wheel2);    // back right
              odrv3.setVelocity(wheel3);    // front left
              odrv0.setVelocity(wheel4);    // front right
              odrv4.setVelocity(wheel5);    // back wheel

          }

          // ** STOP WHEELS UNTIL EVERYTHING IS RESET **
          else if (EMflag == 1) {
              odrv2.setVelocity(0);
              odrv1.setVelocity(0);
              odrv3.setVelocity(0);
              odrv0.setVelocity(0);      
              odrv4.setVelocity(0);       
          }

     
  } // ** END OF TIMED LOOP **
  
} // end of main loop
