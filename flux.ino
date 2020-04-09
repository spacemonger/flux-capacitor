// +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
/*   Flux Capacitor
     Author:  Frank Li
     Date:    -4-2020
     Version: 0.1
*/
// +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_ADS1015.h>
#include <ADS1015_async.h>

#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 32 // OLED display height, in pixels
#define LOGO_HEIGHT   16
#define LOGO_WIDTH    16

// Declaration for an SSD1306 display connected to I2C (SDA, SCL pins)
#define OLED_RESET     4 // Reset pin # (or -1 if sharing Arduino reset pin)
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

Adafruit_ADS1015 adsV(0x48);     /* Use thi for the 12-bit version */
Adafruit_ADS1015 adsI(0x49);

short cRated = 48;
float ocv = 12.7;
float vLimit = 10.5;
//float temperature0;
short SOH = 1;
float SOHError = 0;
float SOCA = 1;
float SOCB = SOCA;
float DODA = 0;
float DODB = DODA;
float voltageA = 0;
float currentA = 0;
float voltageB = 0;
float currentB = 0;
unsigned long firstSample;
unsigned long secondSample;
const unsigned long period = 10;
float nd = 0.80;//Coefficent of discharge assumption with peukert's law, should be tested in matlab and real-life
float nc = 0.80;//Coefficient of charge assumption, should be tested in matlab and real-life

void setup() {
  Serial.begin(115200);

  adsI.setGain(GAIN_SIXTEEN);
  adsI.begin();
  adsV.begin();

  // SSD1306_SWITCHCAPVCC = generate display voltage from 3.3V internally
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { // Address 0x3C for 128x32
    Serial.println(F("SSD1306 allocation failed"));
    for(;;); // Don't proceed, loop forever
  }
  // Show initial display buffer contents on the screen --
  // the library initializes this with an Adafruit splash screen.
  display.display();
  delay(1000); // Pause for 1 second
  // Clear the buffer
  display.clearDisplay();

  
  voltageA = getVoltage();
  currentA = getCurrent();
  SOCA = getOCV(voltageA);

  SOHError = (ocv - voltageA)/(ocv - vLimit);
  SOH = (short) (1 - (((cRated * SOHError)/cRated)*100));
  DODA = SOH - SOCA;
  
  firstSample = millis();
  
}

void loop() {

  /*
  voltage = ((adsV.readADC_SingleEnded(0)*3)/1000.0);
  current = ((((adsI.readADC_Differential_0_1())*0.125)/0.1));
  Serial.print("Differential: "); Serial.print(adsI.readADC_Differential_0_1()); Serial.print("("); Serial.print(adsI.readADC_Differential_0_1()*0.125F); Serial.println("mV)");
  Serial.print("Differential: "); Serial.print(adsI.readADC_Differential_0_1()); Serial.print("("); Serial.print(((adsI.readADC_Differential_0_1()*0.125F)*1000.0F/0.75F)); Serial.println("mA)");
  delay(500);
  */
  secondSample = millis();
  
  if (secondSample - firstSample >= period)  //test whether the period has elapsed
  {
   voltageB = getVoltage();
   currentB = getCurrent();
   
        if(currentB > 0){
            printValues(voltageB, currentB, discharging(voltageB, currentA, currentB), SOH);
        }
        else if(currentB == 0){
            printValues(voltageB, currentB, SOCA, SOH);
        }
        else if(currentB < 0){
            printValues(voltageB, currentB, charging(voltageB, currentA, currentB), SOH);
        }
        else{
            printValues(voltageB, currentB, SOCA, SOH);
        }
        
    firstSample = secondSample;  //IMPORTANT to save the start time of the current LED brightness
  }


}

void printValues(float v, float i, float c, float h) {
  display.clearDisplay();
  display.setTextSize(2);             // Normal 1:1 pixel scale
  display.setCursor(0,0);
  display.setTextColor(SSD1306_WHITE);        // Draw white text
  display.print("V:"); display.println(v);
  display.println(" SOH:"); display.println(h);
  display.print("I:"); display.println(i);
  display.println(" SOC:"); display.println(c);
  display.display();
}

float getVoltage() {//units: V
    return ((adsV.readADC_SingleEnded(0)*3)/1000.0);
}

float getCurrent() { //units: A
    return ((((adsI.readADC_Differential_0_1())*0.125)/0.1)/1000.0);
}

/*
float getTemperature() {
    return TADCgetTemperature;
}
*/

float discharging(float V, float Ia, float Ib){

    if(V > vLimit) {

        DODB = DODA + nd*(-(Ia+Ib)/(cRated*SOHError));
        SOCB = SOH - DODB;
        DODA = DODB;
        SOCA = SOCB;
        return SOCB;
    }
    else {
        SOH = DODA;
        return 0;
    }
    
}

float charging(float V, float Ia, float Ib){

    if(V == ocv && Ib == 0) {
      
        SOH = SOCA;
        return 1;
    }
    else {
        DODB = DODA + nc*(-(Ia+Ib)/(cRated*SOHError));
        SOCB = SOH - DODB;
        DODA = DODB;
        SOCA = SOCB;
        return SOCB;
    }
}

float getOCV(float V){

        if(V <= 12.7 && V>=12.5)
            return ((V-10.7)/2);
        else if(V < 12.5 && V>=12.42)
            return ((V-11.78)/0.8);
        else if(V < 12.42 && V>=12.32)
            return ((V-11.62)/1);
        else if(V < 12.32 && V>=12.2)
            return ((V-11.48)/1.2);
        else if(V < 12.2 && V>=12.06)
            return ((V-11.36)/1.4);
        else if(V < 12.06 && V>=11.9)
            return ((V-11.26)/1.6);
        else if(V < 11.9 && V>=11.75)
            return ((V-11.3)/1.5);
        else if(V < 11.75 && V>=11.58)
            return ((V-11.24)/1.7);
        else if(V < 11.58 && V>=11.31)
            return ((V-11.4)/2.7);
        else if(V < 11.31 && V>=10.5)
            return ((V-10.5)/8.1);
        else
            return ((V-10.5)/8.1);
}
