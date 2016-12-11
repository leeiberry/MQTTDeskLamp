// MQTT Desk Lamp - Using MQTT  and Homie Assistant
//  Includes motion detected , which can be turned off by MQTT
//  MQTT is used to setup defaults , These values maybe set by (via MQTT) ie  Home Assitant , Node-red etc 
//  
//  MQTT Platform provided by Homie Esp8266 v 1.5  Refer Homie Standards 
//
//  options 
//   1) Allow motion detector to turn on lamp , no involvement from MQTT (AUTOSELF) 
//   2) Motion detector inform MQTT that motion has been detect , not further action 
//   3) Turn Lamp on from MQTT 
//   4) Turn Lamp of from Button.


#include <Homie.h>
#include <TickerScheduler.h>
#include "FS.h"  // use 


const int PIN_RELAY = D5;
const int PIN_MOTION = D7;
const int PIN_BUTTON = D3;

int SetSec = 30;//  number of seconds between motion detection reseting (MQTT)
int curSec = 0;// current seconds until motion testing can occur after a motion detection
int AUTOSELF = true; // will the esp8266 turn on the lamp (TURE)  or control via MQTT client
int motionFlag = false;
int LIGHTON = false;

Bounce debouncer = Bounce(); // Bounce is built into Homie, so you can use it without including it first

//  timers 
TickerScheduler ts(2);    //  two timers


HomieNode lightNode("light", "switch");
HomieNode motionNode("motion", "detect");

// timer callback functions 
void setMotion () // motion timer - timer 0 
{ // reduce the counter by 10 secs
  curSec= curSec-10; 
  if (curSec >= 0) {Homie.setNodeProperty(motionNode, "ResetMin", String(curSec));} // Publish to MQTT time remaining
} 


bool lampOnHandler(String value) 
// Turn On Lamp via MQTT (via client) , also used for AUTOSELF option or via the manual button 
{
  if (value == "true") {
    digitalWrite(PIN_RELAY, LOW);
     if ( !LIGHTON) {
        Homie.setNodeProperty(lightNode, "on", "true",true); // publish the state of the light
        Serial.println("Light is on");
        LIGHTON = true;
     }
  } else if (value == "false") {
    digitalWrite(PIN_RELAY, HIGH);
    if ( LIGHTON) {
       Homie.setNodeProperty(lightNode, "on", "false",true);
        Serial.println("Light is off");
        LIGHTON = false;
        motionFlag = false; // reset motion 
        curSec=-1;
        Homie.setNodeProperty(motionNode, "Detected", "false",true); /// reset MQTT , could be a manual reset
    }
  } else {
    return false;
  }

  return true;
}

void ReadV()
{

  Serial.println("testing SPIFF");
  if (!SPIFFS.begin()) { Serial.println("SPIFFS failed");
  } 
  else {
     
      // this opens the file "f.txt" in read-mode
      File f = SPIFFS.open("/fa.txt", "r");

        Serial.println("testing SPIFF - open file");    
      if (!f) {
         Serial.println("File doesn't exist yet. Creating it");

        // open the file in write mode
        Serial.println("testing SPIFF - write file");
        File f = SPIFFS.open("/fa.txt", "w");
        if (!f) {
            Serial.println("file creation failed");
        }
        // now write two lines in key/value style with  end-of-line characters
        f.println("ssid=abc");
        f.println("password=123455secret");
    } else {
      // we could open the file
      Serial.println("testing SPIFF - REead file");
       while(f.available()) {
        //Lets read line by line from the file
        String line = f.readStringUntil('n');
        Serial.println(line);
    }

  }
  f.close();
}
Serial.println("testing SPIFF  DONE");

  SPIFFS.end();
  
}


bool motionOnHandler(String value) {
  //  need to test if numeric
  Serial.println("setting motionOnHandler....");
  SetSec = value.toInt();
  Serial.print("motionOnHandler....");Serial.println(SetSec);
  if (SetSec <= 10) {SetSec=20;}  // safety limits 
  if (SetSec >= 300) {SetSec=300;}// max limits
  Homie.setNodeProperty(motionNode, "MotionWait", String(SetSec), true); // confirm and Publish new time 
  return true;
}

bool autoselfOnHandler(String Value) {
  // set the autoself flag 
  if (Value == "on") {
    Homie.setNodeProperty(motionNode, "AutoSelf", "on",true);
    AUTOSELF = true;
  }
  else
  {
     Homie.setNodeProperty(motionNode, "AutoSelf", "off",true);
    AUTOSELF = false;    
    if  (!lampOnHandler("false")){ Serial.println("Light Turn off failed");} // force the light off if turning auto off.
  }

  Serial.println("AUTOSELF ...");
  
  return true;
  
}


void TestButton()
{
    // test if the the override button has been pushed 
  int  buttonValue = digitalRead(PIN_BUTTON);
  
  if (buttonValue == LOW) {    
   // turn light on or off 
   if (LIGHTON) { lampOnHandler("false");  }
   else {lampOnHandler("true");  }
  }
}


void TestMotion()
{
 // Take a motion reading  
  int motVal = digitalRead(PIN_MOTION);
  //low = no motion, high = motion
  if (motVal == LOW )
    {if (curSec <=0)  // allow to test reading 
      { // turn lamp off 
        curSec = 0;
        if (motionFlag){ /// only publish once 
           Homie.setNodeProperty(motionNode, "Detected", "false"); // Thete is no motion
        if (AUTOSELF ) {lampOnHandler("false");}
        }       
        motionFlag = false;  //
      }
      else 
        {  // wait for until counter is zero before testing 
           //Serial.println("Waiting ..." + String(curMin));
        }
    }
    else
    { // motion , turn the lamp on 
      if (motionFlag) // another motion has been detected witin the countdown preriod, reset the countdown timer 
          {if (curSec< SetSec -10) // notneed to reset , if only reset in the last 10 seconds
            { 
             curSec = SetSec; 
          }
        }
      else 
        {  //   a new motion detection after the reset time 
          curSec = SetSec;; // reset timer
          Homie.setNodeProperty(motionNode, "Detected", "true",true); // MOTION detected publish to mqtt
          if (AUTOSELF) {lampOnHandler("true");}
          motionFlag = true;
        }        
    }

  delay(1000);  
  
}



void ControlLoop(){
  // main control loop for Homie   
  TestButton();
  TestMotion();  
 
}


void setup() {
 
  ReadV();
  
  
  // setup relay pin
  pinMode(PIN_RELAY, OUTPUT);
  digitalWrite(PIN_RELAY, HIGH);
  // PIR motion pin
  pinMode(PIN_MOTION, INPUT);
  // Override button pin 
  pinMode(PIN_BUTTON, INPUT);

  Homie.setFirmware("awesome-lamp", "1.0.0");
  lightNode.subscribe("on", lampOnHandler);
  motionNode.subscribe("MotionWait", motionOnHandler);  // mins waiting time 
  motionNode.subscribe("AutoSelf", autoselfOnHandler);  //  set AutoSelf
  Homie.registerNode(lightNode);
  Homie.registerNode(motionNode);
  Homie.setLoopFunction(ControlLoop);
  
  Homie.setup();
  
  ts.add(0, 10000,setMotion,false);  // motion timer , count down in 10 sec's 
  delay(100);


ReadV(); 
}

void loop() {
  Homie.loop();
  //update timers
  ts.update();
  
}
