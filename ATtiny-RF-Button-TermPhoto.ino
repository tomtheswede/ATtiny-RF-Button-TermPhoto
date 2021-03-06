/*  
 *   For sending 433MHz RF signals from battery operated ATtiny85
 *   Code by Thomas Friberg (https://github.com/tomtheswede)
 *   Updated 21/04/2017
 */

#include <avr/sleep.h> //For sleep commands set_sleep_mode(SLEEP_MODE_PWR_DOWN), sleep_enable() and sleep_mode();

//Device parameters
const byte devices=3;
const unsigned long devID[devices] = {523523,463737,7332222}; // 00001010111011011000100111101111 So the message can be picked up by the right receiver
const unsigned long devType[devices] = {1,2,11}; //Reads as "1" corresponding with BTN type
const byte devPin[devices] = {0,3,2};

//General variables
const byte pwrPin = 1; //Power for external elements pin. pin 6 for rfbutton v0.4 pin 6 is PB1 so use 1

//RF related
const byte sendPin = 4; //RF pin. pin 3 for rfbutton v0.4 which is pb4 so use 4 use. If debugging in breadboard, use 2
const byte typePreamble[4] = {120,121,122,123}; //01111000 for register, 01111001 for basic message type
byte msgLengths[4]={32,8,16,32};
byte msgBuffer[9]; //Max 9 bytes for transmission
int msgLength;

//Analog reading related


//Button related
long currentTime=millis();

//Interrupt variables
volatile const byte btnPin = devPin[0]; //rfButton v0.4 uses pin 5 which is PB0 so use 0 here.
volatile bool primer[5]={0,0,0,0,0};
volatile bool pressed=0;
volatile bool buttonState=0;
volatile unsigned long pressTime=0;
volatile const unsigned int reTriggerDelay=50; //minimum time in millis between button presses to remove bad contact
#define sbi(sfr, bit) (_SFR_BYTE(sfr) |= _BV(bit)) //OR - Turn on bit
#define cbi(sfr, bit) (_SFR_BYTE(sfr) &= ~_BV(bit)) //AND - Turn off bit

void setup() { //This code runs as soon as the device is powered on.
  pinMode(pwrPin,OUTPUT);
  digitalWrite(pwrPin,1);
  pinMode(sendPin,OUTPUT);
  //pinMode(devPin[1],INPUT);
  //pinMode(devPin[2],INPUT);

  delay(400);
  encodeMessage(1,devID[0],3); //Register button on first on

  pinMode(devPin[0],INPUT_PULLUP);
  digitalWrite(pwrPin,0); //Power down devices between button pushes
  //Set pin to start interrupts
  sbi(GIMSK,PCIE); //Turn on interrupt
  sbi(PCMSK,PCINT0); //set pin affected by interupt - PCINT0 corresponds to PB0 or pin 5
  sleepSet(); //Sleep when entering loop
}

void loop() {

  CheckButton();

}

void sleepSet() {
  //Power down
  cbi(ADCSRA,ADEN); //Turns off the ADCs
  set_sleep_mode(SLEEP_MODE_PWR_DOWN);
  sleep_enable();
  sleep_mode(); //Starts the sleep
  //Sleep is here. Code only runs onward if an intterupt is triggered.
  sbi(ADCSRA,ADEN); //Turns on the ADCs
}

//Button functions --------------------------------------------

ISR(PCINT0_vect) {
  // This is called when the interrupt occurs
  buttonState=!(digitalRead(btnPin));
  if (buttonState && (millis()-pressTime>reTriggerDelay)) { //if pressed in
    pressed=true;
    primer[0]=1;
    primer[1]=1;
    primer[2]=1;
    primer[3]=1;
    primer[4]=1;
    pressTime=millis();
    //encodeMessage(1,5);
  }
}

void CheckButton() {
  if (pressed && digitalRead(devPin[0])) {
    pressed = false;
    //digitalWrite(pwrPin,0); //Turn off ancillaries
    primer[0]=0;
    primer[1]=0;
    primer[2]=0;
    primer[3]=0;
    primer[4]=0;
    //sleepSet(); //Sleep when button is released
  }
  if (pressed) {
    digitalWrite(pwrPin,1); //Turn on ancillaries
    currentTime=millis();
    if (primer[0]) {
      encodeMessage(1,devID[0],1);
      primer[0]=0;
    }
    else if (primer[1] && (currentTime-pressTime>600)) {
      encodeMessage(1,devID[0],2);
      primer[1]=0;
    }
    else if (primer[2] && (currentTime-pressTime>1500)) {
      encodeMessage(2,devID[2],analogRead(devPin[2]));
      primer[2]=0;
      //encodeMessage(2,devID[1],4144);
    }
    else if (primer[3] && (currentTime-pressTime>3000)) {
      encodeMessage(1,devID[0],4);
      primer[3]=0;
    }
    else if (primer[4] && (currentTime-pressTime>6000)) {
      encodeMessage(0,devID[0],devType[0]);  //Register
      primer[4]=0;
    }
  }
}

//ADC check ------------------------------------------------------

void SendTemperature() {
  encodeMessage(2,devID[1],4144);
}


//RF Functions ------------------------------------------------------------------------

void pulse(bool logic) {
  if (logic) { //a one
    digitalWrite(sendPin,HIGH);
    delayMicroseconds(410);  //797us realtime - 720
    digitalWrite(sendPin,LOW);
    delayMicroseconds(10);   //416us realtime - 60
  }
  else { //a zero
    digitalWrite(sendPin,HIGH);
    delayMicroseconds(140);  //416us realtime -320
    digitalWrite(sendPin,LOW);
    delayMicroseconds(30);  //797us realtime - 470
  }
}

void encodeMessage(byte msgType,unsigned long dID, unsigned long msg) {
  int k;
  msgLength=msgLengths[msgType];
  k=0;
  //construct the message
  for (int i=0; i<8; i++) {  //6 bit preamble with 2 bit msg type - 8 bit total
    bitWrite(msgBuffer[k/8],7-(k%8),bitRead(typePreamble[msgType],7-i));
    k++;
  }
  for (int i=0; i<32; i++) {  //32 bit device ID
    bitWrite(msgBuffer[k/8],7-(k%8),bitRead(dID,31-i));
    k++;
  }
  for (int i=0; i<msgLength; i++) {  //72,48,56,72 bit message length
    bitWrite(msgBuffer[k/8],7-(k%8),bitRead(msg,msgLength-1-i));
    k++;
  }
  
  //Send the message
  pulse(1); //For calibration pre-read of reciever
  pulse(0); //For calibration pre-read of reciever
  delay(1);
  for (int rep=0; rep<5; rep++) {
    for (int i=0; i<msgLength+40; i++) {
      pulse(bitRead(msgBuffer[i/8],7-(i%8)));
    }
    pulse(0); //to end the message timing
    delay(1);
  }
  delay(1);
}
