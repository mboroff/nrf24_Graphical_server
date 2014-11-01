/*******************************************************************

        Server code for remote temperature and humidity project
        Code uses Radiohead library and Adafruit tft library
        Devices used nrf24L01, DS1301 RTC and Adafruit 2.8 touch screen
             wired for 8 bit to reduce any conflicts with SPI devices
        Where aplicable example code comments were left in place
*******************************************************************/

// nrf24_server.pde
// -*- mode: C++ -*-
// Example sketch showing how to create a simple messageing server
// with the RH_NRF24 class. RH_NRF24 class does not provide for addressing or
// reliability, so you should only use RH_NRF24  if you do not need the higher
// level messaging abilities.
// It is designed to work with the other example nrf24_client
// Tested on Uno with Sparkfun NRF25L01 module
// Tested on Anarduino Mini (http://www.anarduino.com/mini/) with RFM73 module
// Tested on Arduino Mega with Sparkfun WRL-00691 NRF25L01 module

#include <SPI.h>
#include <RH_NRF24.h>
#include <Adafruit_GFX.h>    // Core graphics library
#include <Adafruit_TFTLCD.h> // Hardware-specific library
#include <Wire.h>
#include "Adafruit_STMPE610.h"
#include <DS1307.h>

// The control pins for the LCD can be assigned to any digital or
// analog pins...but we'll use the analog pins as this allows us to
// double up the pins with the touch screen (see the TFT paint example).
#define LCD_CS A3 // Chip Select goes to Analog 3
#define LCD_CD A2 // Command/Data goes to Analog 2
#define LCD_WR A1 // LCD Write goes to Analog 1
#define LCD_RD A0 // LCD Read goes to Analog 0

#define LCD_RESET A4 // Can alternately just connect to Arduino's reset pin

// When using the BREAKOUT BOARD only, use these 8 data lines to the LCD:
// For the Arduino Uno, Duemilanove, Diecimila, etc.:
//   D0 connects to digital pin 8  (Notice these are
//   D1 connects to digital pin 9   NOT in order!)
//   D2 connects to digital pin 2
//   D3 connects to digital pin 3
//   D4 connects to digital pin 4
//   D5 connects to digital pin 5
//   D6 connects to digital pin 6
//   D7 connects to digital pin 7
// For the Arduino Mega, use digital pins 22 through 29
// (on the 2-row header at the end of the board).

// Assign human-readable names to some common 16-bit color values:
#define	BLACK   0x0000
#define	BLUE    0x001F
#define	RED     0xF800
#define	GREEN   0x07E0
#define CYAN    0x07FF
#define MAGENTA 0xF81F
#define YELLOW  0xFFE0
#define WHITE   0xFFFF

Adafruit_TFTLCD tft(LCD_CS, LCD_CD, LCD_WR, LCD_RD, LCD_RESET);

// Singleton instance of the radio driver
RH_NRF24 nrf24(8, 53);
// RH_NRF24 nrf24(8, 7); // use this to be electrically compatible with Mirf
// RH_NRF24 nrf24(8, 10);// For Leonardo, need explicit SS pin
// RH_NRF24 nrf24(8, 7); // For RFM73 on Anarduino Mini
// Init the DS1307
DS1307 rtc(20, 21);
#define YP A3  // must be an analog pin, use "An" notation!
#define XM A2  // must be an analog pin, use "An" notation!
#define YM 23   // can be a digital pin
#define XP 22   // can be a digital pin

#define TS_MINX 150
#define TS_MINY 120
#define TS_MAXX 3800
#define TS_MAXY 4000
#define MINPRESSURE 10
#define MAXPRESSURE 1000

// Option #2 - use hardware SPI, connect to hardware SPI port only!
// SDI to MOSI, SDO to MISO, and SCL to SPI CLOCK
// on Arduino Uno, that's 11, 12 and 13 respectively
// Then pick a CS pin, any pin is OK but we suggest #10 on an Uno
// tie MODE to 3.3V and POWER CYCLE the STMPE610 (there is no reset pin)
#define STMPE_CS  36
Adafruit_STMPE610 touch = Adafruit_STMPE610(STMPE_CS);

uint16_t px, py;
uint8_t pz;
uint16_t tsx, tsy;
uint8_t tsz;


/************************************************
            booleans
************************************************/
boolean pulse = false;
boolean nosig = false;
boolean signalLost = false;
boolean reVerse = false;

/*********************************************
       counters
**********************************************/
int pulseCtr = 2;
/********************************************************
                time
********************************************************/
byte oldsec=0;
int mySecond;
int myMinute;
int myHour;
int mYweekDay;
int mYmonthDay;
int mYmonth;
int mYyear;
int prevHour;
int prevSec;
int prevDay;

const char   janStr[] = "Jan";
const char   febStr[] = "Feb";
const char   marStr[] = "Mar";
const char   aprStr[] = "Apr";
const char   mayStr[] = "May";
const char   junStr[] = "Jun";
const char   julStr[] = "Jul";
const char   augStr[] = "Aug";
const char   sepStr[] = "Sep";
const char   octStr[] = "Oct";
const char   novStr[] = "Nov";
const char   decStr[] = "Dec"; 

const char* const monthStrs[] = { janStr, febStr, marStr, aprStr, mayStr, junStr,
                                          julStr, augStr, sepStr, octStr, novStr, decStr}; 

/*************************************************************
                           Setup
**************************************************************/
void setup() 
{
  Serial.begin(9600);
  while (!Serial) 
    ; // wait for serial port to connect. Needed for Leonardo only
  if (!nrf24.init())
    Serial.println("init failed");
  // Defaults after init are 2.402 GHz (channel 2), 2Mbps, 0dBm
  if (!nrf24.setChannel(1))
    Serial.println("setChannel failed");
  if (!nrf24.setRF(RH_NRF24::DataRate2Mbps, RH_NRF24::TransmitPower0dBm))
    Serial.println("setRF failed");    
#ifdef USE_ADAFRUIT_SHIELD_PINOUT
  Serial.println(F("Using Adafruit 2.8\" TFT Arduino Shield Pinout"));
#else
  Serial.println(F("Using Adafruit 2.8\" TFT Breakout Board Pinout"));
#endif

  tft.reset();

  uint16_t identifier = tft.readID();

  if(identifier == 0x9325) {
    Serial.println(F("Found ILI9325 LCD driver"));
  } else if(identifier == 0x9328) {
    Serial.println(F("Found ILI9328 LCD driver"));
  } else if(identifier == 0x7575) {
    Serial.println(F("Found HX8347G LCD driver"));
  } else if(identifier == 0x9341) {
    Serial.println(F("Found ILI9341 LCD driver"));
  } else {
    Serial.print(F("Unknown LCD driver chip: "));
    Serial.println(identifier, HEX);
    Serial.println(F("If using the Adafruit 2.8\" TFT Arduino shield, the line:"));
    Serial.println(F("  #define USE_ADAFRUIT_SHIELD_PINOUT"));
    Serial.println(F("should appear in the library header (Adafruit_TFT.h)."));
    Serial.println(F("If using the breakout board, it should NOT be #defined!"));
    Serial.println(F("Also if using the breakout, double-check that all wiring"));
    Serial.println(F("matches the tutorial."));
    return;
  }

  tft.begin(identifier);
  drawMain();
  if (! touch.begin()) {
    Serial.println("STMPE not found!");
    while(1);
  }

  // Set the clock to run-mode
  rtc.halt(false);
  // The following lines can be commented out to use the values already stored in the DS1307
//  rtc.setDOW(FRIDAY);        // Set Day-of-Week to SUNDAY
//  rtc.setTime(21, 53, 0);     // Set the time to 22:47:00 (24hr format)
//  rtc.setDate(31, 10, 2014);   // Set the date to October 31th, 2014
  Serial.println(rtc.getDateStr());

}
/*****************************************************
                Loop
*****************************************************/
void loop()
{
  get_touch();     // check for touch

  if (tsz > MINPRESSURE) {    // if pressure go to set clock menu
      setMenu();
      tsz = 0;                  // avoid touch bounce
      drawMain();              // redrw the main screen
      prevDay = 99;            // for date print
      }

  for (int i = 0; i < 50; i++) {      // take as many attempts to get 
       getNrf24();                    // data packet without disrupting
                                      // clock display
  }
  getTimeAndDate();                          // get the time and date

  if (prevSec != mySecond) {         // do stuff only when new second
      prevSec = mySecond;
      if (nosig == false) {          // if signal restored clear the area
          if (signalLost == true) {  //  where data is displayed
              signalLost = false;
              tft.fillRect(118, 40, 100, 200, BLUE);
          }
      }
      
      pulseCtr++;                   // going tocheck if it is time to flash pulse dot
 //     Serial.println(pulseCtr);
      if (pulseCtr < 30) {         // greater than 30 seconds report signal lost
          if (pulse == false) {
              pulse = true;
              tft.fillCircle(20, 50, 3, RED);
              } else {
                      pulse = false;
                      tft.fillCircle(20, 50, 3, BLUE);
                      }
      } else {
        if (nosig == false) {
            nosig = true;
            signalLost = true;
            tft.fillRect(118, 40, 100, 150, BLUE);
            tft.fillCircle(20, 50, 3, BLUE);
            tft.setCursor(120, 50);
            tft.setTextColor(RED, BLUE);  // all text is RED over blue background                        
            tft.print("Signal");
            tft.setCursor(120, 70);
            tft.print("Lost");            
            tft.setTextColor(WHITE, BLUE);  // all text is white over blue background            
        }
      }
      
      if (prevDay != mYmonthDay) {    // if different day clear date area
          prevDay = mYmonthDay;
          tft.fillRect(12, 202, 86, 105, BLUE);
      }
      tft.setTextSize(4);
      tft.setCursor(20, 210);
  //    Serial.print("Month : "); Serial.println(mYmonth-1);
      tft.print(monthStrs[mYmonth-1]);               // print month abbreviation
      tft.setCursor(30, 250);
      tft.print(mYmonthDay);                         // print day of month
      tft.setTextSize(2);
      tft.setCursor(30, 290);
      tft.print(mYyear);                            // print the year
      tft.setTextSize(3);      
      if (prevHour != myHour) {                  //  on the hour change clear area
          prevHour = myHour;  
          tft.fillRect(108, 228, 120, 30, BLUE);
      }
      tft.setCursor(110, 230);                      // print the time
      if (myHour > 12) myHour = myHour - 12;
      if (myHour == 0) myHour = 12;      
      tft.print(myHour);
      tft.print(":");
      if (myMinute < 9) tft.print("0");
      tft.print(myMinute);
      tft.setTextSize(2); 
      if (mySecond < 10) tft.print("0"); 
      tft.print(mySecond);
  }
}
/*********************************************
      getTimeAndDate
*********************************************/
void getTimeAndDate(){
  // Send time
  //Serial.println(rtc.getTimeStr());
  String timeString = rtc.getTimeStr();           // get time string
  int searchIndex = 0;
  int foundIndex = 0;
  String foundValue = "";
  foundIndex = timeString.indexOf(':');          // search for colon
  foundValue = timeString.substring(searchIndex, foundIndex);
  char temphour[foundValue.length() + 1];  
  foundValue.toCharArray(temphour, sizeof(temphour));   // copy hour 
  myHour = atoi(temphour);                              // convert to integer
  searchIndex = foundIndex + 1;
  foundIndex = timeString.lastIndexOf(':');               // find next colon
  foundValue = timeString.substring(searchIndex, foundIndex);
  char tempminute[foundValue.length() + 1];                 // convert minute to integer
  foundValue.toCharArray(tempminute, sizeof(tempminute));
  myMinute = atoi(tempminute);
  searchIndex = foundIndex +1;
  foundIndex = timeString.length();                               
  foundValue = timeString.substring(searchIndex, foundIndex+1);  // get seconds
  char tempsecond[foundValue.length() + 1];                 
  foundValue.toCharArray(tempsecond, sizeof(tempsecond));
  mySecond = atoi(tempsecond);                                  // convert second to integer
//  Serial.print(myHour); Serial.print(":"); Serial.print(myMinute); Serial.print(":"); Serial.println(mySecond);
  // Wait one second before repeating :)
  String Dow = rtc.getDOWStr(FORMAT_SHORT);                    // get day of the week string
//  Serial.println(Dow);
  String myDate = rtc.getDateStr();                            // get date string
// Serial.println(myDate);
  foundValue = myDate.substring(0, 2);
  char tempMonthDay[3];
  foundValue.toCharArray(tempMonthDay, sizeof(tempMonthDay));
  mYmonthDay = atoi(tempMonthDay);                              // convert day of month to integer
  char tempMonth[3];
  foundValue  = myDate.substring(3, 5);
  foundValue.toCharArray(tempMonth,sizeof(tempMonth));                
  mYmonth = atoi(tempMonth);                                    // convert month tointeger
  foundValue  = myDate.substring(6, 10);
  char tempYear[5];
  foundValue.toCharArray(tempYear, sizeof(tempYear));
  mYyear = atoi(tempYear);                                       // convert year to integer
//  Serial.println(myDate);
  delay(1000);
}
/******************************************
         getNrf
******************************************/
void getNrf24()
{
  
  if (nrf24.available())                                           // check if buffer has data
  {
    // Should be a message for us now   
    uint8_t buf[RH_NRF24_MAX_MESSAGE_LEN];
    uint8_t len = sizeof(buf);

     if (nrf24.recv(buf, &len))                                    // get packet
    {
      pulseCtr = 0;
      nosig = false;
//      Serial.print("got request: ");
//      Serial.println((char*)buf);
      char buff[5] = "    ";
         if (buf[0] == 'P') {                                       // is it a probe packet
 //            Serial.print("Got a P - Len : "); Serial.println(len);
             if (buf[1] == 'C') {                                   // is it centegrade in probe packet
                 tft.setCursor(120, 50);
                 for (int i = 2; i < len; i++) {                     // skip first two char then print temp
                      buff[i-2] = buf[i];
                 }         
                 tft.print(buff);                                
                 tft.drawCircle(180, 50, 3, WHITE);
                 tft.setCursor(190, 50);
                 tft.print("C");
             }
             if (buf[1] == 'F') {                                 // is it fahrenheit in probe packet
                 tft.setCursor(120, 70);
                 for (int i = 2; i < len; i++) {                 // skip first two char then print temp
                      buff[i-2] = buf[i];             
                 }
                 tft.print(buff);
                 tft.drawCircle(180, 70, 3, WHITE);
                 tft.setCursor(190, 70);
                 tft.print("F");
             }
         }
         if (buf[0] == 'D') {                                    //   is it DHT22 packet
 //            Serial.print("Got a P - Len : "); Serial.println(len);
             if (buf[1] == 'C') {                                // is it centegrade
                 tft.setCursor(120, 110);
                 for (int i = 2; i < len; i++) {
                      buff[i-2] = buf[i];
                 }
                 tft.print(buff);
                 tft.drawCircle(180, 110, 3, WHITE);
                 tft.setCursor(190, 110);
                 tft.print("C");
             }
             if (buf[1] == 'F') {                                    // it it fahrenheit
                 tft.setCursor(120, 130);
                 for (int i = 2; i < len; i++) {
                      buff[i-2] = buf[i];
                 }
                 tft.print(buff);
                 tft.drawCircle(180, 130, 3, WHITE);
                 tft.setCursor(190, 130);
                 tft.print("F");
             }
             if (buf[1] == 'H') {                                   // is it humidty
                 tft.setCursor(120, 150);
                 for (int i = 2; i < len; i++) {
                      buff[i-2] = buf[i];
                 }
                 tft.print(buff);
                 tft.setCursor(174, 150);
                 tft.print("%");
                 tft.setCursor(190, 150);
                 tft.print("H");

             }

         }

    }
    else
    {
      Serial.println("recv failed");
    }
  }
}
/*******************************************
          drawMain   shell for main screen
*******************************************/
void drawMain()
{
  tft.setTextColor(WHITE, BLUE);  // all text is white over blue background
  tft.fillScreen(BLUE);           // clear the screen BLUE
  delay(500);
  tft.drawRect(10, 10, 220, 300, WHITE);
  tft.drawLine(10, 200, 100, 200, WHITE);
  tft.drawLine(100, 200, 100, 308, WHITE); 
  tft.setTextSize(2);
  tft.setCursor(30, 50);
  tft.print("Outside");
  tft.setCursor(30, 110);
  tft.print("Garage");

}  
/**********************************************
            get_touch  - generic touch test 
                         set global ts x, y and z
***********************************************/

void get_touch() 
{

// clear the coordinates
tsx = 0;
tsy = 0;
tsz = 0;
px = 0;
py = 0;
pz = 0;
//Serial.println("Get touch");
 // while (pz > 0)
 if (touch.touched()) {
   
    // read px & py & pz;
    while (! touch.bufferEmpty()) {
      touch.readData(&px, &py, &pz);
      while(touch.bufferSize()>0){          // flush buffer
            touch.readData(&px, &py, &pz);
            }
/*
      Serial.print(F("Before constrain ->(")); 
      Serial.print(px); Serial.print(F(", ")); 
      Serial.print(py); Serial.print(F(", ")); 
      Serial.print(pz);
      Serial.println(F(")"));
*/
              delay(50);

    touch.writeRegister8(STMPE_INT_STA, 0xFF); // reset all ints
    }

      if (pz > MINPRESSURE) {
          px = map(px, TS_MINX, TS_MAXX, 0, tft.width());
          py = map(py, TS_MINY, TS_MAXY, 0, tft.height());
/* 
          Serial.print(F("->(")); 
          Serial.print(px); Serial.print(F(", ")); 
          Serial.print(py); Serial.print(F(", ")); 
          Serial.print(pz);
          Serial.println(F(")"));
*/
          if (pz > tsz) {
             tsx = px;   // get max
             tsy = py;
             tsz = pz;
          }
           while(touch.bufferSize()>0){
            touch.getPoint();  
             }
           px = 0;
           py = 0;
           pz = 0;
      }
  }  
}


 
/**********************************************
            setMenu0
***********************************************/
void setMenu()
{
  tsx = 0;
  tsy = 0;
  tsz = 0;
  px = 0;
  py = 0;
  pz = 0;

  tft.fillScreen(BLUE);           // clear the screen BLUE
  tft.setTextSize(3);   
  tft.setCursor(40, 10);
  tft.print("Set Clock");           // display title
  tft.setCursor(40, 120);
  tft.print("Hour");                  // draw selection and buttons
  tft.fillRect(40, 60, 50, 50, WHITE);
  tft.fillRect(60, 70, 10, 30, BLACK);
  tft.fillRect(50, 80, 30, 10, BLACK);
  tft.fillRect(40, 150, 50, 50, WHITE);
  tft.fillRect(50, 170, 30, 10, BLACK);

  tft.setCursor(150, 120);
  tft.print("Min");
  tft.fillRect(150, 60, 50, 50, WHITE);
  tft.fillRect(170, 70, 10, 30, BLACK);
  tft.fillRect(160, 80, 30, 10, BLACK);
  tft.fillRect(150, 150, 50, 50, WHITE);
  tft.fillRect(160, 170, 30, 10, BLACK);

  tft.fillRect(40, 260, 160, 50, WHITE);             // draw return button
  tft.setCursor(60, 270);
  tft.setTextColor(BLACK, WHITE);  // all text is white over blue background  
  tft.print("Return");
  tft.setTextColor(WHITE, BLUE);  // all text is white over blue background  
  
  
  while (1) {                                     // stay in this loop until the user selects the return to options 
       get_touch();                               // by pressing at the botton of display 
       if (tsz > MINPRESSURE) {
           Serial.print("Set menu X = "); Serial.print(tsx);
           Serial.print("\tSet menu Y = "); Serial.println(tsy);
           Serial.print("\tSet menu Pressure = "); Serial.println(tsz);
           if (tsx > 40 && tsx < 200 && tsy >260 && tsy < 310) {
               return;
               }
           if (tsx > 40 && tsx < 100 && tsy > 60 && tsy < 110) {
               bumpHour();
               }
           if (tsx > 150 && tsx < 210 && tsy > 60 && tsy < 110) {
               bumpMinute();
               }
           if (tsx > 40 && tsx < 100 && tsy > 150 && tsy < 200) {
               reVerse = true;
               bumpHour();
               reVerse = false;
               }
           if (tsx > 150 && tsx < 210 && tsy > 150 && tsy < 200) {
               reVerse = true;
               bumpMinute();
               reVerse = false;
               }

           }

      getTimeAndDate();                         // get the time and date

      if (prevSec != mySecond) {                 // print the time once a ssecond
          prevSec = mySecond;
      tft.setCursor(50, 230);
      if (myHour > 12) myHour = myHour - 12;
      if (myHour == 0) myHour = 12;
      tft.print(myHour);
      tft.print(":");
      if (myMinute < 9) tft.print("0");
      tft.print(myMinute);
      tft.print(":");
      if (mySecond < 10) tft.print("0"); 
          tft.print(mySecond);
      }
  }// end of menu while

}

/********************************************************************
          bumpHour
********************************************************************/
void bumpHour() {                                    // add or subtract one hour
  if (reVerse == false) {
      myHour++;
  } else {
 
    myHour--;
  }
  if (myHour > 23 ){                           // fix up hour if necessary
     myHour = 0;
  }
  if (myHour < 0 ){
     myHour = 23;
  }
  rtc.setTime(myHour, myMinute, mySecond);
  tft.fillRect(48, 228, 150, 30, BLUE);
  
}

/*********************************************************************
               bumpMinute
*********************************************************************/
void bumpMinute() {
  if (reVerse == false) {                            // add or subtract one minute
      myMinute++;
  } else {
    myMinute--;
  }
  if (myMinute > 59){                                 // fix minute if necessary
      myMinute = 0;
  }
  if (myMinute < 0){
      myMinute = 59;
  }

  mySecond = 0;
  rtc.setTime(myHour, myMinute, mySecond); 
  tft.fillRect(48, 228, 150, 30, BLUE);
}
