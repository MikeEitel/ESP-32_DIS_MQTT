// Modified by Mike Eitel to display on ILI9341 based board
// No rights reserved
//

//#define Rhy          // If defined ( Me .. MeIOT .. LU ..  Rhy ) use private network for testing, otherwise use IOT standard
//#define TEST        // Testmodus
#define LCDtypeN // Witch LCDtype of CYD  choose by additional letter  N .. R .. C

#if defined(ESP8266)
  #include <ESP8266WiFi.h>
#elif defined(ESP32)
  #include <WiFi.h>
#endif

#include <PubSubClient.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ILI9341.h>

#include <driver/adc.h>

#if defined(LCDtypeC) 
  #include <bb_captouch.h>  
#elif defined(LCDtypeR) 
  #include <XPT2046_Bitbang.h>
#endif

//#include <Fonts/Picopixel.h>

// This files contain the device definitions for different networks
#if defined(Me)                       
    #include <Me_credentials.h>      
#elif defined(MeIOT)
    #include <MeIOT_credentials.h>     
#elif defined(LU)
    #include <LU_credentials.h>     
#elif defined(Rhy)
    #include <Rhy_credentials.h> 
//    #include <Rhy_config.h>       
#else
    #include <credentials.h>         
#endif

// Define used pins of the lcd
#define CYD_RST  -1
#define CYD_DC    2
#define CYD_MISO 12
#define CYD_MOSI 13
#define CYD_SCLK 14
#define CYD_CS   15
#define CYD_BL   27               // The display backlight
#define CYD_LDR  34               // The ldr light sensor
            // diffrent from CYD source = 21
#define Bon HIGH                  // Backlight ON easier to read in code when non pwm use
#define Boff LOW                  // Backlight OFF easier to read in code when non pwm use

#if defined(LCDtypeC)             // These are for the capacitive touch type  
  #define CST820_SDA 33           // diffrent from CYD source = 
  #define CST820_SCL 32           // diffrent from CYD source = 
  #define CST820_RST 25           // diffrent from CYD source = 
  #define CST820_IRQ 21           // diffrent from CYD source = 
#elif defined(LCDtypeR)           // These are for the capacitive touch type
  #define XPT2046_IRQ  36
  #define XPT2046_MOSI  13        // diffrent from CYD source = 32
  #define XPT2046_MISO  12        // diffrent from CYD source = 39
  #define XPT2046_CLK  14         // diffrent from CYD source = 25
  #define XPT2046_CS  33
#endif

// Define onboard led
#define CYD_LED_RED 4
#define CYD_LED_GREEN 16
#define CYD_LED_BLUE 17
#define Lon LOW                   // Led ON easier to read in code
#define Loff HIGH                 // Led OFF easier to read in code

// Define additional colour variants
#define ILI9341_DARK      050505  // a very dark gray
#define ILI9341_DARK_BLUE 000005  // a very dark blue

#define mqMaxtext 55              // Maximal tranverable text via mqtt minus 1


// Declare global variables and constants
unsigned long currentMillis;                  // Actual timer 
unsigned long prevMQTTMillis = 2764472319;    // Stores last MQTT time value was published 2764472319->FASTER START
unsigned long prevMinMillis = 2764472319;     // Stores last minutes time value ->  2764472319->FASTER START
unsigned long prevTickerMillis = 2764472319;  // Stores last ticker time value ->  2764472319->FASTER START
String Sendme;                                // Used for clear text messages in MQTT
int receivedlenght;                           // How long is mqtt message
char lastreceived;                            // Stores the last received status
char receivedChar[mqMaxtext + 35];            //  = "";
bool received;                                // Actual received status
int watchdogW = 1;                            // Counter if there is no wifi connection
int watchdogM = 1;                            // Counter if there is no MQTT connection
int mqttstatus;                               // Helper to see whats going on
bool watchdog = true;                         // Signal via mqtt that device is still ok
bool statusreset = false;                     // Used to minimize error 0 sendouts
bool Ticker;                                  // Ticker for fx. blinking leds
int looped = 1;                               // Loop counter as debug helper
byte blk_set = 255;                            // Control of pwm dimmed backlight
byte blk_now = 1;                             // Helper to control dimmed backlight
byte blk_last = 0;                            // Helper to control dimmed backlight
int ldr;                                      // Value of read LDR

// Variables needed for mqtt commands
int mqX;                          // Received from mqtt start a X position                
int mqY;                          // Received from mqtt start a Y position
int mqlX;                         // Received from mqtt lenght off X area
int mqlY;                         // Received from mqtt lenght off Y area
int mqtX;                         // Received from mqtt X start of text
int mqtY;                         // Received from mqtt X start of text
int mqS;                          // Received from mqtt text size
char mqT[mqMaxtext];              // Received from mqtt text
int mqC;                          // Received from mqtt text colour
int mqB;                          // Received from mqtt background colour
int trend[3] = {25, 24, 61};      // cyd preset chars for rising/falling/equal

// Specific for Rhysauna easy overview app --------------------------------------------------------------
int posval[6][2];                 // Position of fields for a 4 upper and 2 lower fields
struct mqstruct {                 // Structure of a mqtt message describing a numbered predefined field
  int fc;                         // The foregound colour
  int bc;                         // The backgroud colour
  char text[54];                  // The text
};
mqstruct mq_val;                  // Get message as combination of colours and text
//  ------------------------------------------------------------------------------------------------------

uint16_t Colourtable[20];
int LEDsta = 1;                   // Commandstatus for all set by mqtt
int LEDsta_BL = 1;                // Commandstatus for backlight
int LEDsta_R = 0;                 // Commandstatus for red led
int LEDsta_G = 0;                 // Commandstatus for green led
int LEDsta_B = 0;                 // Commandstatus for blue led
bool LEDsta_used;                 // Status that LEDsta was set via mqtt

// Only needed when there is touch included     xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx
#if !defined(LCDtypeN)
  // Define stouch areas on screen
  const int XTmin = 18; // realistic values that come from touch
  const int XTmax = 288;
  const int YTmin = 16;
  const int YTmax = 222;

  float XTdiv = 320.0 / (XTmax - XTmin);    // Needed to adjust touch error in X
  float YTdiv = 240.0 / (YTmax - YTmin);    // Needed to adjust touch error in Y
  int X = 0;                // Last detected X position on touch
  int Y = 0;                // Last detected Y position on touch
  int field = 0;            // Actual detected screen field
  int lastfield = 0;        // Helper to avoid double activities
#endif

struct areastruct {       // The areas are defined as numbered area based on pixels
  int area;              // Unique nr. off the defined field, starting from 0 
  int Xstart;             // Start/reference point if the field X 
  int Xlen;               // Length of the field in X 
  int Xtext;              // Start of text X based on Xstart
  int Xtlen;              // Length of the texts background X
  int Ystart;             // Start/reference point if the field Y
  int Ylen;               // Length of the field in Y 
  int Ytext;              // Start of text Y based on Ystart
  int Ytlen;              // Length of the texts background Y
};
areastruct areas[12];

struct areastaticstruct { // Used to have a static "menue's text" on screen
  int field;              // This unique area number is NOT the same as in area 
  int area;               // This is based on areas.area[]. Can be multiple times in this struct                                
  int Xoff;               // X offset of background, based on areas.Xstart of areas.area[]
  int Xlen;               // Background length X, based on areas.Xstart of areas.area[]
  int Xtext;              // X offset of text, based on area.Xstart of areas.area[]
  int Yoff;               // Y offset of background, based on area.Ystart  of areas.area[]
  int Ylen;               // Background length Y, based on area.Ystart of areas.area[] 
  int Ytext;              // Y offset of text, based on area.Ystart of areas.area[]
  int size;               // Size of text 
  int fg;                 // Foreground colour
  int bg;                 // Background colour
  const char* text;       // The text
};
areastaticstruct ar_sta[40];   // A application depending structure of static texts

struct fieldvaluestruct { // Used to have a static "menue's text" on screen
  int field;              // This unique field number is NOT the same as in area 
  int area;               // This is a area.field[]. Can be multiple times in this struct                                
  int Xvoff;              // X Start of value based on area Xstart
  int Xvlen;              // X Length of value based on area Xstart
  int Xvbsta;             // Start of background based on Xstart
  int Xvblen;             // Length of background based on Xstart
  int Yvoff;              // Y Start of value based on area Xstart
  int Yvlen;              // Y Length of value based on area Xstart
  int Yvbsta;             // Start of background based on Ystart
  int Yvblen;             // Length of background based on Ystart
};
fieldvaluestruct fieldval[40];   // A application depending structure of static texts


int XY1tareas = 4;        // Possible touch areas on X axis for upper row Y1
int XY2tareas = 4;        // Possible touch areas on X axis for middle row Y2
int XY3tareas = 4;        // Possible touch areas on X axis in case there are 3 rows Y3
int XYtamax  = 17;        // Maximal number of areas
int Staticmax  = 12;      // Maximal number of static texts for areas
int Fieldmax  = 40;       // Maximal number of field for areas


// Only needed when there is touch included     xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx

Adafruit_ILI9341 cyd = Adafruit_ILI9341(CYD_CS, CYD_DC, CYD_MOSI, CYD_SCLK, CYD_RST, CYD_MISO);
//#include <Fonts/FreeMonoBoldOblique12pt7b.h>

#if defined(LCDtypeC)
  BBCapTouch touch;
  TOUCHINFO ti;
  const char *szNames[] = {"Unknown", "FT6x36", "GT911", "CST820"};
#elif defined(LCDtypeR)
  XPT2046_Bitbang touchscreen(XPT2046_MOSI, XPT2046_MISO, XPT2046_CLK, XPT2046_CS,320,240);
#endif

//#include "screen_config.h"

// Setup the background classes  
WiFiClient   espClient;     // Get Wifi access
PubSubClient mqttclient;    // MQTT protokol handler

// AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA  APPLICATION SPECIFIC start AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA

// XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX  Nontouch based routines  XXXXXXXXXXXXXXXXXXXXXXXXXXXXX   

#define touch_3_2_0

void MakeScreenTable(){
  
  #if defined(touch_3_2_0)         // Possible touch areas on X axis for middle row Y2
    XY1tareas = 3;                 // Possible touch areas on X axis for upper row Y1
    XY2tareas = 2;                 // Possible touch areas on X axis for middle row Y2
    XY3tareas = 0;                 // Possible touch areas on X axis in case there are 3 rows Y3
    
    // Definition of the main areas
    int fieldValues[] =  {   1,   2,   3,   4,   5,  99};
    int XstartValues[] = {   1, 108, 215,   1, 161,  99};
    int XlenValues[] =   { 105, 105, 105, 158, 159,  99};
    int XtextValues[] =  {   5,   5,   5,   5,   5,  99};
    int XtlenValues[] =  {  98,  98,  98, 150, 150,  99};
    int YstartValues[] = {   1,   1,   1, 121, 121,  99};
    int YlenValues[] =   { 118, 118, 118, 118, 118,  99};
    int YtextValues[] =  {   5,   5,   5,   5,   5,  99};
    int YtlenValues[] =  {  25,  25,  25,  25,  25,  99};

    // Definition of the static / menue text in an area
    int SfieldValue[] = {   1,   2,   3,   4,   5,   6,   7,   8,   9,  10,  99};
    int SareaValue[] =  {   1,   1,   2,   2,   3,   3,   4,   4,   5,   5,  99};
    int SXoffValue[] =  {   3,   3,   3,   3,   3,   3,   3,   3,   3,   3,   1};
    int SXlenValue[] =  { 100, 100, 100, 100, 100, 100, 154, 154, 154, 154,   1};
    int SXtextValue[] = {  10,  10,  10,  10,  10,  10,  10,  10,  10,  10,   1};
    int SYoffValue[] =  {   3,  23,   3,  23,   3,  23,   3,  23,   3,  23,   1};
    int SYlenValue[] =  {  20,  20,  20,  20,  20,  20,  20,  20,  20,  20,   1};
    int SYtextValue[] = {   5,  23,   5,  23,   5,  23,   5,  23,   6,  24,   1};
    int SsizeValue[] =  {   2,   2,   2,   2,   2,   2,   2,   2,   2,   2,   1};
    int SfgValue[] =    {  12,  12,  12,  12,  12,  12,  12,  12,  12,  12,   1};
    int SbgValue[] =    {   6,   6,   6,   6,   6,   6,   6,   6,   6,   6,   1};
    const char* textValue[] = { " Sauna"," Jurte"," Arven"," Sauna","grosses","  Fass",
                                " Ruhe Jurte","  mit Ofen","kleines Fass","  mit Ofen","x"};

    // Definition of the dynamic written fields in the areas
    int FfieldValue[] =  {   1,   2,   3,   4,   5,   6,   7,   8,   9,  10,  11,  12,  13,  14,  99};
    int FareaValue[] =   {   1,   1,   2,   2,   5,   5,   3,   3,   4,   4,   4,   4,   5,   5,  99};
    int FXvbstaValue[] = {   2,  83,   2,  83,   2, 138,   2,  83,   2, 138,   2, 138,   2, 138,  99};
    int FXvblenValue[] = {  79,  18,  79,  18, 135,  18,  79,  18, 135,  18, 135,  18, 135,  18,  99};
    int FYvbstaValue[] = {  65,  65,  65,  65,  45,  45,  65,  65,  45,  45,  80,  80,  80,  80,  99};
    int FYvblenValue[] = {  37,  37,  37,  37,  34,  34,  37,  37,  34,  34,  34,  34,  34,  34,  99};
    int FXvoffValue[] =  {   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,  99};
    int FXvlenValue[] =  {  75,  17,  75,  17, 132,  17,  75,  17, 132,  17, 132,  17, 132,  17,  99};
    int FYvoffValue[] =  {   5,   5,   5,   5,   5,   5,   5,   5,   5,   5,   5,   5,   5,   5,  99};
    int FYvlenValue[] =  {  29,  29,  29,  29,  25,  25,  29,  29,  25,  25,  25,  25,  25,  25,  99};

  #elif defined(touch_4_2_0)   // The 4 * 2 
    XY1tareas = 4;                 // Possible touch areas on X axis for upper row Y1
    XY2tareas = 2;                 // Possible touch areas on X axis for middle row Y2
    XY3tareas = 0;                 // Possible touch areas on X axis in case there are 3 rows Y3
    
    // Definition of the main areas
    int fieldValues[] =  {   1,   2,   3,   4,   5,   6,  99};
    int XstartValues[] = {   1,  81, 161, 241,   1, 161,  99};
    int XlenValues[] =   {  78,  78,  78,  79, 158, 159,  99};
    int XtextValues[] =  {   5,   5,   5,   5,   5,   5,  99};
    int XtlenValues[] =  {  70,  70,  70,  70, 150, 150,  99};
    int YstartValues[] = {   1,   1,   1,   1, 121, 121,  99};
    int YlenValues[] =   { 118, 118, 118, 118, 118, 118,  99};
    int YtextValues[] =  {   5,   5,   5,   5,   5,   5,  99};
    int YtlenValues[] =  {  25,  25,  25,  25,  25,  25,  99};

    // Definition of the static / menue text in an area
    int SfieldValue[] = {   1,   2,   3,   4,   5,   6,   7,   8,   9,  10,  11,  12,  99};
    int SareaValue[] =  {   1,   1,   2,   2,   3,   3,   4,   4,   5,   5,   6,   6,  99};
    int SXoffValue[] =  {   3,   3,   3,   3,   3,   3,   3,   3,   3,   3,   3,   3,   1};
    int SXlenValue[] =  {  73,  73,  73,  73,  73,  73,  74,  74, 154, 154, 154, 154,   1};
    int SXtextValue[] = {  10,  10,  10,  10,   5,  17,   5,  17,  28,  33,  50,  10,   1};
    int SYoffValue[] =  {   3,  23,   3,  23,   3,  23,   3,  23,   3,  23,   3,  23,   1};
    int SYlenValue[] =  {  20,  20,  20,  20,  20,  20,  20,  20,  20,  20,  20,  20,   1};
    int SYtextValue[] = {   5,  23,   5,  23,   5,  23,   5,  23,   6,  24,   6,  24,   1};
    int SsizeValue[] =  {   2,   2,   2,   2,   2,   2,   2,   2,   2,   2,   2,   2,   1};
    int SfgValue[] =    {  12,  12,  12,  12,  12,  12,  12,  12,  12,  12,  12,  12,   1};
    int SbgValue[] =    {   6,   6,   6,   6,   6,   6,   6,   6,   6,   6,   6,   6,   1};
    const char* textValue[] = { "Sauna","Jurte","Arven","Sauna","klein.","Fass","gross.","Fass",
                                "Ruhejurte","mit Ofen","Ofen","kleines Fass","off"};

    // Definition of the dynamic written fields in the areas
    int FfieldValue[] =  {   1,   2,   3,   4,   5,   6,   7,   8,   9,  10,  11,  12,  13,  14,  99};
    int FareaValue[] =   {   1,   1,   2,   2,   3,   3,   4,   4,   5,   5,   5,   5,   6,   6,  99};
    int FXvbstaValue[] = {   2,  58,   2,  58,   2,  58,   2,  58,   1, 138,   1, 138,   1, 138,  99};
    int FXvblenValue[] = {  52,  18,  52,  18,  52,  18,  52,  18, 135,  18,  135, 18,  135, 18,  99};
    int FYvbstaValue[] = {  65,  65,  65,  65,  65,  65,  65,  65,  45,  45,  80,  80,  60,  60,  99};
    int FYvblenValue[] = {  37,  37,  37,  37,  37,  37,  37,  37,  32,  32,  32,  32,  36,  36,  99};
    int FXvoffValue[] =  {   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,  99};
    int FXvlenValue[] =  {  50,  17,  50,  17,  50,  17,  50,  17, 132,  17, 132,  17, 132,  17,  99};
    int FYvoffValue[] =  {   5,   5,   5,   5,   5,   5,   5,   5,   5,   5,   5,   5,   5,   5,  99};
    int FYvlenValue[] =  {  29,  29,  29,  29,  29,  29,  29,  29,  23,  23,  23,  23,  26,  26,  99};

  #elif defined(touch_3_3_4)   // The 3+3+4 
    XY1tareas = 3;                 // Possible touch areas on X axis for upper row Y1
    XY2tareas = 3;                 // Possible touch areas on X axis for middle row Y2
    XY3tareas = 4;                 // Possible touch areas on X axis in case there are 3 rows Y3

    // Definition of the main areas
    int fieldValues[] =  {   1,   2,   3,   4,   5,   6,   7,   8,   9,  10,  99};
    int XstartValues[] = {   1, 108, 215,   1, 108, 215,   1,  81, 161, 241,  99};
    int XlenValues[] =   { 105, 105, 105, 105, 105, 105,  78,  78,  78,  79,  99};
    int XtextValues[] =  {   5,   5,   5,   5,   5,   5,   5,   5,   5,   5,  99};
    int XtlenValues[] =  {  98,  98,  98,  98,  98,  98,  65,  65,  65,  65,  99};
    int YstartValues[] = {   1,   1,   1,  98,  98,  98, 194, 194, 194, 194,  99};
    int YlenValues[] =   {  95,  95,  95,  95,  95,  95,  46,  46,  46,  46,  99};
    int YtextValues[] =  {   5,   5,   5,   5,   5,   5,   5,   5,   5,   5,  99};
    int YtlenValues[] =  {  25,  25,  25,  25,  25,  25,  25,  25,  25,  25,  99};

    // Definition of the static / menue text in an area  
    int SfieldValue[] = {   1,   2,   3,   4,   5,   6,   7,   8,   9,  10,  11,  12,  13,  14,  15,  16,  99};
    int SareaValue[] =  {   1,   2,   3,   4,   5,   6,   7,   8,   9,  10,   1,   2,   3,   4,   5,   6,  99};
    int SXoffValue[] =  {   3,   3,   3,   3,   3,   3,   3,   3,   3,   3,   3,   3,   3,   3,   3,   3,   1};
    int SXlenValue[] =  { 100, 100, 100, 100, 100, 100,  73,  73,  73,  73, 100, 100, 100, 100, 100, 100,   1};
    int SXtextValue[] = {  10,  10,  10,  10,  10,  10,   6,  10,   4,  10,  10,  10,  10,  10,  10,  10,   1};
    int SYoffValue[] =  {   3,   3,   3,   3,   3,   3,   3,   3,   3,   3,   3,   3,   3,   3,   3,   3,   1};
    int SYlenValue[] =  {  20,  20,  20,  20,  20,  20,  20,  20,  20,  20,  20,  20,  20,  20,  20,  20,   1};
    int SYtextValue[] = {   3,   3,   3,   3,   3,   3,   3,   3,   3,   3,   3,   3,   3,   3,   3,   3,   1};
    int SsizeValue[] =  {   2,   2,   2,   2,   2,   2,   2,   2,   2,   2,   2,   2,   2,   2,   2,   2,   1};
    int SfgValue[] =    {  12,  12,  12,  12,  12,  12,  12,  12,  12,  12,  12,  12,  12,  12,  12,  12,   1};
    int SbgValue[] =    {   6,   6,   6,   6,   6,   6,   6,   6,   6,   6,   6,   6,   6,   6,   6,   6,   1};
    const char* textValue[] = { "Decke","Vorhang","Sofa","Decke","Vorhang","Sofa",
                                "Wohnzi","Kuech","Garten","Spare",
                                "AA","BB","CC","AA","BB","CC","off"};

    // Definition of the dynamic written fields in the areas
    int FfieldValue[] =  {   1,   2,   3,   4,   5,   6,   7,   8,   9,  10,  99};
    int FareaValue[] =   {   1,   2,   3,   4,   5,   6,   7,   8,   9,  10,  99};
    int FXvbstaValue[] = {   2,   2,   2,   2,   2,   2,   2,   2,   2,   2,  99};
    int FXvblenValue[] = { 100, 100, 100, 100, 100, 100,  70,  70,  70,  70,  99};
    int FYvbstaValue[] = {  45,  45,  45,  45,  45,  45,  21,  21,  21,  21,  99};
    int FYvblenValue[] = {  32,  32,  32,  32,  32,  32,  40,  11,  12,  13,  99};
    int FXvoffValue[] =  {   5,   5,   3,   3,   3,   3,   3,   3,   3,   3,  99};
    int FXvlenValue[] =  {  92,  97,  97,  97,  97,  97,  70,  70,  70,  70,  99};
    int FYvoffValue[] =  {   3,   3,   3,   3,   3,   3,   3,   3,   3,   3,  99};
    int FYvlenValue[] =  {  23,  23,  23,  23,  23,  23,  19,  19,  19,  19,  99};

  #elif defined(touch_4_4_4)   // The 3+3+4 
    // Max sensfull matrix of 4x4x4
    XY1tareas = 4;                 // Possible touch areas on X axis for upper row Y1
    XY2tareas = 4;                 // Possible touch areas on X axis for middle row Y2
    XY3tareas = 4;                 // Possible touch areas on X axis in case there are 3 rows Y3

    // Definition of the main areas
    int fieldValues[] =  {   1,   2,   3,   4,   5,   6,   7,   8,   9,  10,  11,  12,  99};
    int XstartValues[] = {   1,  81, 161, 241,   1,  81, 161, 241,   1,  81, 161, 241,  99};
    int XlenValues[] =   {  78,  78,  78,  79,  78,  78,  78,  79,  78,  78,  78,  79,  99};
    int XtextValues[] =  {   5,   5,   5,   5,   5,   5,   5,   5,   5,   5,   5,   5,  99};
    int XtlenValues[] =  {  65,  65,  65,  65,  65,  65,  65,  65,  65,  65,  65,  65,  99};
    int YstartValues[] = {   1,   1,   1,   1,  81,  81,  81,  81, 161, 161, 161, 161,  99};
    int YlenValues[] =   {  78,  78,  78,  78,  78,  78,  78,  78,  79,  79,  79,  79,  99};
    int YtextValues[] =  {   5,   5,   5,   5,   5,   5,   5,   5,   5,   5,   5,   5,  99};
    int YtlenValues[] =  {  25,  25,  25,  25,  25,  25,  25,  25,  25,  25,  25,  25,  99};

    // Definition of the static / menue text in an area
    int SfieldValue[] = {   1,   2,   3,   4,   5,   6,   7,   8,   9,  10,  11,  12,  99};
    int SareaValue[] =  {   1,   2,   3,   4,   5,   6,   7,   8,   9,  10,  11,  12,  99};
    int SXoffValue[] =  {   3,   3,   3,   3,   3,   3,   3,   3,   3,   3,   3,   3,   1};
    int SXlenValue[] =  {  73,  73,  73,  73,  73,  73,  73,  73,  73,  73,  73,  73,   1};
    int SXtextValue[] = {  10,  10,  10,  10,  10,  10,  10,  10,  10,  10,  10,  10,   1};
    int SYoffValue[] =  {   3,   3,   3,   3,   3,   3,   3,   3,   3,   3,   3,   3,   1};
    int SYlenValue[] =  {  20,  20,  20,  20,  20,  20,  20,  20,  20,  20,  20,  20,   1};
    int SYtextValue[] = {   5,   5,   5,   5,   5,   5,   5,   5,   5,   5,   5,   5,   1};
    int SsizeValue[] =  {   2,   2,   2,   2,   2,   2,   2,   2,   2,   2,   2,   2,   1};
    int SfgValue[] =    {  12,  12,  12,  12,  12,  12,  12,  12,  12,  12,  12,  12,   1};
    int SbgValue[] =    {   6,   6,   6,   6,   6,   6,   6,   6,   6,   6,   6,   6,   1};
    const char* textValue[] = {"A","B","C","D","E","F","G","H","I","J","K","L","off"};

    // Definition of the dynamic written fields in the areas
    int FfieldValue[] =  {   1,   2,   3,   4,   5,   6,   7,   8,   9,  10,  11,  12,  99};
    int FareaValue[] =   {   1,   2,   3,   4,   5,   6,   7,   8,   9,  10,  11,  12,  99};
    int FXvbstaValue[] = {   2,   2,   2,   2,   2,   2,   2,   2,   2,   2,   2,   2,  99};
    int FXvblenValue[] = {  73,  73,  73,  73,  73,  73,  73,  73,  73,  73,  73,  73,  99};
    int FYvbstaValue[] = {  35,  35,  35,  35,  35,  35,  35,  35,  35,  35,  35,  35,  99};
    int FYvblenValue[] = {  35,  35,  35,  35,  35,  35,  35,  35,  35,  35,  35,  35,  99};
    int FXvoffValue[] =  {   3,   3,   3,   3,   3,   3,   3,   3,   3,   3,   3,   3,  99};
    int FXvlenValue[] =  {  67,  67,  67,  67,  67,  67,  67,  67,  67,  67,  67,  67,  99};
    int FYvoffValue[] =  {   5,   5,   5,   5,   5,   5,   5,   5,   5,   5,   5,   5,  99};
    int FYvlenValue[] =  {  26,  26,  26,  26,  26,  26,  26,  26,  26,  26,  26,  26,  99};

  #elif defined(touch_2_2_0)
    XY1tareas = 2;                 // Possible touch areas on X axis for upper row Y1
    XY2tareas = 2;                 // Possible touch areas on X axis for middle row Y2
    XY3tareas = 0;                // Possible touch areas on X axis in case there are 3 rows Y3

    // Definition of the main areas
    int fieldValues[] =  {   1,   2,   3,   4,  99};
    int XstartValues[] = {   1, 161,   1, 161,  99};
    int XlenValues[] =   { 158, 158, 158, 158,  99};
    int XtextValues[] =  {  10,  10,  10,  10,  99};
    int XtlenValues[] =  { 140, 140, 140, 140,  99};
    int YstartValues[] = {   1,   1, 121, 121,  99};
    int YlenValues[] =   { 118, 118, 118, 118,  99};
    int YtextValues[] =  {  10,  10,  10,  10,  99};
    int YtlenValues[] =  { 100, 100, 100, 100,  99};

    // Definition of the static / menue text in an area
    int SfieldValue[] =  {   1,   2,   3,   4,  99};
    int SareaValue[] =   {   1,   2,   3,   4,  99};
    int SXoffValue[] =   {   3,   3,   3,   3,  99};
    int SXlenValue[] =   { 152, 152, 152, 152,  99};
    int SXtextValue[] =  {  10,  10,  10,  10,  99};
    int SYoffValue[] =   {   3,   3,   3,   3,  99};
    int SYlenValue[] =   { 112, 112, 112, 112,  99};
    int SYtextValue[] =  {  10,  10,  10,  10,  99};
    int SsizeValue[] =   {   2,   2,   2,   2,  99};
    int SfgValue[] =     {  12,  12,  12,  12,  99};
    int SbgValue[] =     {   6,   6,   6,   6,  99};
    const char* textValue[] = {"Area A","Area B","Area C","Area D","off"};

    // Definition of the dynamic written fields in the areas
    int FfieldValue[] =  {   1,   2,   3,   4,   5,   6,   7,   8,  99};
    int FareaValue[] =   {   1,   1,   2,   2,   3,   3,   4,   4,  99};
    int FXvbstaValue[] = {   3, 116,   3, 116,   3, 116,   3, 116,  99};
    int FXvblenValue[] = { 120,  39, 115,  39, 100,  39, 100,  39,  99};
    int FYvbstaValue[] = {  40,  40,  40,  40,  40,  40,  40,  40,  99};
    int FYvblenValue[] = {  72,  72,  72,  72,  72,  72,  72,  72,  99};
    int FXvoffValue[] =  {   5,   5,   5,   5,   5,   5,   5,   5,  99};
    int FXvlenValue[] =  { 108,  28, 108,  28, 108,  28, 108,  28,  99};
    int FYvoffValue[] =  {   5,   5,   5,   5,   5,   5,   5,   5,  99};
    int FYvlenValue[] =  {  60,  60,  60,  60,  60,  60,  60,  60,  99};

  #else  
    XY1tareas = 1;                 // Possible touch areas on X axis for upper row Y1
    XY2tareas = 0;                 // Possible touch areas on X axis for middle row Y2
    XY3tareas = 0;                // Possible touch areas on X axis in case there are 3 rows Y3

    // Definition of the main areas
    int fieldValues[] =  {   1,   2,  99};
    int XstartValues[] = {   1,   1,  99};
    int XlenValues[] =   { 318,   1,  99};
    int XtextValues[] =  {  10,   1,  99};
    int XtlenValues[] =  { 300,   1,  99};
    int YstartValues[] = {   1,   1,  99};
    int YlenValues[] =   { 238,   1,  99};
    int YtextValues[] =  {  10,   1,  99};
    int YtlenValues[] =  { 220,   1,  99};


    // Definition of the static / menue text in an area
    int SfieldValue[] = {   1,   2,  99};
    int SareaValue[] =  {   1,   2,  99};
    int SXoffValue[] =  {   3,   1,  99};
    int SXlenValue[] =  { 317,   1,  99};
    int SXtextValue[] = {  10,   1,  99};
    int SYoffValue[] =  {   3,   1,  99};
    int SYlenValue[] =  { 220,   1,  99};
    int SYtextValue[] = {   5,   1,  99};
    int SsizeValue[] =  {   2,   1,  99};
    int SfgValue[] =    {  12,   1,  99};
    int SbgValue[] =    {   6,   1,  99};
    const char* textValue[] = {"A","","off"};

      // Definition of the dynamic written fields in the areas
    int FfieldValue[] =  {   1,   2,  99};
    int FareaValue[] =   {   1,   1,  99};
    int FXvbstaValue[] = {   3, 270,  99};
    int FXvblenValue[] = { 262,  47,  99};
    int FYvbstaValue[] = {  40,  40,  99};
    int FYvblenValue[] = { 178, 178,  99};
    int FXvoffValue[] =  {   5,   5,  99};
    int FXvlenValue[] =  { 253,  38,  99};
    int FYvoffValue[] =  {   5,   5,  99};
    int FYvlenValue[] =  { 165, 166,  99};
  #endif

  XYtamax = XY1tareas + XY2tareas + XY3tareas;  // Maximal number of areas
    
  for (int i = 0; i < XYtamax; i++){
    areas[i].area = fieldValues[i];
    areas[i].Xstart = XstartValues[i];
    areas[i].Xlen = XlenValues[i];
    areas[i].Xtext = XtextValues[i];
    areas[i].Xtlen = XtlenValues[i];
    areas[i].Ystart = YstartValues[i];
    areas[i].Ylen = YlenValues[i];
    areas[i].Ytext = YtextValues[i];
    areas[i].Ytlen = YtlenValues[i];
  }

  for (int s = 0; s < 40 && SfieldValue[s] < 99; s++) {
    Staticmax = SfieldValue[s];
  }
  for (int s = 0; s < Staticmax && ar_sta[s + 1].field < 99; s++) {
    ar_sta[s].field = SfieldValue[s]; 
    ar_sta[s].area = SareaValue[s];  
    ar_sta[s].Xoff = SXoffValue[s];  
    ar_sta[s].Xlen = SXlenValue[s];
    ar_sta[s].Xtext = SXtextValue[s]; 
    ar_sta[s].Yoff = SYoffValue[s];    
    ar_sta[s].Ylen = SYlenValue[s];
    ar_sta[s].Ytext = SYtextValue[s];     
    ar_sta[s].size = SsizeValue[s];    
    ar_sta[s].fg = SfgValue[s];    
    ar_sta[s].bg = SbgValue[s];    
    ar_sta[s].text = textValue[s];
  }

  for (int f = 0; f < 40 && FfieldValue[f] < 99; f++) {
    Fieldmax = FfieldValue[f];
  }
  for (int f = 0; f < Fieldmax && fieldval[f + 1].field < 99; f++) {
    fieldval[f].field = FfieldValue[f]; 
    fieldval[f].area = FareaValue[f];  
    fieldval[f].Xvoff = FXvoffValue[f];
    fieldval[f].Xvlen = FXvlenValue[f];
    fieldval[f].Xvbsta = FXvbstaValue[f];
    fieldval[f].Xvblen = FXvblenValue[f];
    fieldval[f].Yvoff = FYvoffValue[f];
    fieldval[f].Yvlen = FYvlenValue[f];
    fieldval[f].Yvbsta = FYvbstaValue[f];
    fieldval[f].Yvblen = FYvblenValue[f];
  }
}

  void StartScreen()
  { // MQTT S
    /* Test that profes the full screen adressing is from 0,0 to 319,239
       cyd.fillRect(0, 0, 1, 1, tCol(17));
       cyd.fillRect(319, 0, 1, 1, tCol(17));
       cyd.fillRect(0, 239, 1, 1, tCol(17));
       cyd.fillRect(319, 239, 1, 1, tCol(17));
     */
    cyd.fillRect(0, 0, 320, 240, tCol(3));
    for (int s = 0; s < XYtamax; s++)
    {
      cyd.drawRect(areas[s].Xstart, areas[s].Ystart, areas[s].Xlen, areas[s].Ylen, tCol(6));
    }
  }

void  StaticText2Screen(int Sta, int Stb) {   // MQTT M
  if (Stb > Staticmax) { Stb = Staticmax; }
  for (int s = Sta; s < Stb ; s++) {
    int arr = ar_sta[s].area -1;
    int Xarr = areas[arr].Xstart;
    int Yarr = areas[arr].Ystart;    
    int Xra = Xarr + ar_sta[s].Xoff;
    int Yra = Yarr + ar_sta[s].Yoff;
    int XrSi = ar_sta[s].Xlen;
    int YrSi = ar_sta[s].Ylen;
    int XTa = Xarr + ar_sta[s].Xtext;
    int YTa = Yarr + ar_sta[s].Ytext;
    int S = ar_sta[s].size;
    u_int16_t Textcol = tCol(ar_sta[s].fg);
    u_int16_t Backcol = tCol(ar_sta[s].bg);
    const char* Stext = ar_sta[s].text;
    //                 Xra  Yra XrSi YrSi  XTa  YTa  S  Textcol   Backcol Text
    PrintArea2Screen(Xra, Yra, XrSi, YrSi, XTa, YTa, S, Textcol, Backcol, Stext );
    }
  }

// xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx APPLICATION SPECIFIC  end  xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx


// mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm mqtt connection  start mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
void setup_wifi() {
  delay(10);

  WiFi.config(staticIP, gateway, subnet);

  Serial.println("");
  Serial.print("Try connect to: ");
  Serial.println(wifi_ssid);
  Serial.print("With IP address: ");
  Serial.println(WiFi.localIP());

  cyd.setTextColor(ILI9341_WHITE); 
  cyd.setTextSize(1);
  cyd.setCursor(0,35);
  cyd.print("Try connect to: ");
  cyd.setTextColor(ILI9341_GREEN); 
  cyd.setTextSize(2);
  cyd.setCursor(120,30);
  cyd.println(wifi_ssid);

  cyd.setTextColor(ILI9341_WHITE); 
  cyd.setTextSize(1);
  cyd.setCursor(0,65);
  cyd.print("With IP address: ");
  cyd.setTextColor(ILI9341_GREEN); 
  cyd.setTextSize(2);  
  cyd.setCursor(120,60);
  cyd.println(WiFi.localIP());

  WiFi.begin(wifi_ssid, wifi_password);
  WiFi.setTxPower(WIFI_POWER_19_5dBm);

  WiFi.onEvent([](WiFiEvent_t event, WiFiEventInfo_t info) {handleWiFiEvent(event, info);});
  
  while ((WiFi.status()!= WL_CONNECTED) && (watchdogW <= WiFi_timeout)) {
    delay(250);
    Serial.print(".");
    Serial.print(watchdogW);

    #if defined(TEST)
      cyd.setTextColor(ILI9341_WHITE); 
      cyd.setTextSize(2);  
      cyd.setCursor(55,100);      
      cyd.println("TEST MODE Wifi try");
    #else
      cyd.setTextColor(ILI9341_WHITE); 
      cyd.setTextSize(2);  
      cyd.setCursor(120,100);      
      cyd.println(" Wifi try");
    #endif
    cyd.fillRect(150,130,60,22,ILI9341_DARKGREY);
    cyd.setTextColor(ILI9341_GREEN); 
    cyd.setTextSize(3);
    cyd.setCursor(155,130); 
    cyd.print(watchdogW);

    watchdogW++;
  }
  if (WiFi.status()!= WL_CONNECTED){
    Serial.println ("No connection ");
    Serial.println ("Restart");
    cyd.fillRect(60,130,200,50,ILI9341_YELLOW);
    cyd.setTextColor(ILI9341_RED); 
    cyd.setTextSize(4);
    cyd.setCursor(80,140); 
    cyd.print("RESTART");
    delay(5000);
    ESP.restart();
    }
  else {
    Serial.println(" Successfull connected Wifi");
    Serial.print("RSSI: ");   Serial.println(WiFi.RSSI());
    cyd.fillRect(0,95,320,25,ILI9341_GREEN);
    cyd.fillRect(60,130,200,50,ILI9341_BLACK);
    cyd.setTextColor(ILI9341_BLACK); 
    cyd.setTextSize(2);
    cyd.setCursor(5,100); 
    cyd.print("Successfull connected Wifi");
  }
}

void handleWiFiEvent(WiFiEvent_t event, WiFiEventInfo_t info) {
    // Handle Wi-Fi events
    if (event == SYSTEM_EVENT_STA_DISCONNECTED) {
        Serial.println("Wi-Fi disconnected");
        // Reconnect to Wi-Fi
        // WiFi.reconnect();
    }
}

void reconnect() {
  delay(100);
  // Loop until we're reconnected to MQTT server
  while (!mqttclient.connected() && (watchdogM <= mqtt_timeout)) {
    mqttclient.clearWriteError();       // Cleaning MQTT write buffer
    mqttclient.flush();                 // Cleaning MQTT data buffer
    mqttstatus = mqttclient.state();    // Decoding of MQTT status 
    Serial.println("");
    Serial.print("Attempting MQTT connection try Nr. ");
    cyd.setTextColor(ILI9341_WHITE); 
    cyd.setTextSize(1);
    cyd.setCursor(0,135);
    cyd.print("Attempting MQTT connection: ");
    cyd.fillRect(170,128,40,22,ILI9341_DARKGREY);
    cyd.setTextColor(ILI9341_GREEN); 
    cyd.setTextSize(2);  
    cyd.setCursor(170,130);
    cyd.println(watchdogM);
   
    Serial.println(watchdogM);
    
    const char *reason;
    switch (mqttstatus) {
      case -4 : {reason = "The server didn't respond within the keepalive time"; break;}
      case -3 : {reason = "The network connection was broken"; break;}
      case -2 : {reason = "The network connection failed"; break;}
      case -1 : {reason = "The client is disconnected cleanly"; break;}
      case  0 : {reason = "The client is connected"; break;}
      case  1 : {reason = "The server doesn't support the requested version of MQTT"; break;}
      case  2 : {reason = "The server rejected the client identifier"; break;}
      case  3 : {reason = "The server was unable to accept the connection"; break;}
      case  4 : {reason = "The username/password were rejected"; break;}
      case  5 : {reason = "The client was not authorized to connect"; break;}
      default: {   }    // Wrong               
    }
    Serial.println(reason);
    cyd.fillRect(0,148,320,30,ILI9341_YELLOW);
    cyd.setTextColor(ILI9341_BLACK); 
    cyd.setTextSize(1);  
    cyd.setCursor(0,150);
    cyd.println(reason);
 
    if (mqttclient.connect(iamclient, mqtt_user, mqtt_password)) {
      Serial.print("Connected as: ");
      Serial.println(iamclient);
      Serial.println("");
      cyd.fillRect(0,128,320,80,ILI9341_BLACK);
      cyd.setTextColor(ILI9341_WHITE); 
      cyd.setTextSize(1);  
      cyd.setCursor(0,150);
      cyd.println("Listening to MQTT:");
      cyd.setTextColor(ILI9341_GREEN); 
      cyd.setTextSize(2);  
      cyd.setCursor(20,170);
      cyd.println(in_topic);
        
      // Restore the Startupscreen
      delay(1500);
      StartScreen();

      // Send status after start
      #if defined(TEST)
        mqttclient.publish(out_error, "1" ,false);
      #else
        mqttclient.publish(out_error, "MQTT Connected" ,false);
        watchdogM = 1;
#endif
    }         
    Serial.println("Retry MQTT in 1 second");
    delay(800);
    watchdogM++;
    Serial.println("_");
  }

  if (watchdogM >= mqtt_timeout) {
    Serial.println("");
    Serial.println("NO MQTT available");
    Serial.println("RESTART");
    cyd.fillRect(0,125,320,105,ILI9341_YELLOW);
    cyd.setTextColor(ILI9341_RED); 
    cyd.setTextSize(4);
    cyd.setCursor(80,170); 
    cyd.print("RESTART");
    delay(5000);
    ESP.restart();
  }
}

void callback(char* topic, uint8_t* payload, unsigned int length) {
  receivedlenght = length;
  Serial.println("");
  Serial.print("Message for [");
  Serial.print(topic);
  Serial.print("] arrived = L:(");
  Serial.print(length);
  Serial.print(")->");
  for (unsigned int i = 0; i < length; i++) {
    receivedChar[i] = payload[i];
    Serial.print(receivedChar[i]); //     Received from mqtt text
  }
  Serial.println();
  // Here is a decision point to analyse command structure
  #if defined(TEST)
      mqttclient.publish(out_error, "3" ,false);
    #else
      mqttclient.publish(out_error, "Command received" ,false);
      //statusreset = true;
      //delay(1000);
  #endif

  switch (receivedChar[0]) {          // Detecition what command is detected 
    case '?' : {                       // request parameters
      Sendme = "RSSI: ";
      Sendme = Sendme + (WiFi.RSSI());
      mqttclient.publish(out_param, (String(Sendme).c_str()), false);
      Sendme = "Light: ";
      Sendme = Sendme + (analogRead(CYD_LDR));
      mqttclient.publish(out_ligth, (String(Sendme).c_str()), false);
      /*
      #if ! defined(LCDtypeN)
        showTouchTable();
      #endif*/
      break;
    }
    case 'C' : { cyd.fillRect(0,0,319,239,tCol(0)); break;}                                                   // Clear screen dark
    case 'S' : { StartScreen(); StaticText2Screen(0,Fieldmax); break;}                                        // Start screen

    case 'U' : { cyd.fillRect(0,0,320,240,tCol(9)); break;}                                                   // Clear screen light grey
    case 'V' : { StartScreen(); break;}                                                                       // Only the frame
    case 'W' : { StaticText2Screen(0,Fieldmax); ShowArea(); break;}                                           // Static menue text to screen
    case 'Z' : { StartScreen(); StaticText2Screen(0,Fieldmax); ShowTouch();  delay(1500); ShowArea(); break;} // Helper for AREA concept
    
    case 'I' : {int V; V = x2i(receivedChar,1,2); blk_set = V; break;}       // Set intensity of display
    case 'L' : {int V; V = x2i(receivedChar,1,2); LEDsta = V; LEDsta_used = HIGH; break; }                                                                       // All led togeather
    case 'R' : {int V; V = x2i(receivedChar,1,2); LEDsta_R = V; break;}     // Red led
    case 'G' : {int V; V = x2i(receivedChar,1,2); LEDsta_G = V; break;}     // Green led
    case 'B' : {int V; V = x2i(receivedChar,1,2); LEDsta_B = V; break;}     // Blue led

    case 'P' : { // P 
      int V; 
      V = x2i(receivedChar,1,2);             // Received from mqtt witch variable is send
      mqC = x2i(receivedChar,3,4);           // Received from mqtt text colour
      mqB = x2i(receivedChar,5,6);           // Received from mqtt background colour 
      for (unsigned int i = 0; i < (receivedlenght - 7); i++) {mqT[i] = receivedChar[i + 7];} // Received from mqtt text
      mqT[receivedlenght - 7] = '\0';
      PrintText2Screen(posval[V][0], posval[V][1], 5, tCol(mqC), tCol(mqB), mqT);
      break;
    }
    case 'T': {
      mqX = x2i(receivedChar,1,3);            // Received from mqtt start a X position                
      mqY = x2i(receivedChar,4,6);            // Received from mqtt start a Y position
      mqS = x2i(receivedChar,7,8);            // Received from mqtt text size
      mqC = x2i(receivedChar,9,10);           // Received from mqtt text colour
      mqB = x2i(receivedChar,11,12);          // Received from mqtt background colour 
      for (unsigned int i = 0; i < (receivedlenght - 13); i++) {mqT[i] = receivedChar[i + 13];} // Received from mqtt text
      mqT[receivedlenght - 13] = '\0';
      PrintText2Screen(mqX, mqY, mqS, tCol(mqC), tCol(mqB), mqT);
      break;
    }

    case 'M' : { // P 
      int Va;   int Vb; 
      Va = x2i(receivedChar,1,2);           // Received from mqtt witch variable is send
      Vb = x2i(receivedChar,3,4);           // Received from mqtt text colour
      StaticText2Screen(Va,Vb);
      break;
    }

    case 'F': { // F 01 02 06 07 text
      int V = x2i(receivedChar,1,2) -1;     // Received from mqtt witch field is written to
      mqS = x2i(receivedChar,3,4);          // Received from mqtt text size
      mqC = x2i(receivedChar,5,6);          // Received from mqtt text colour
      mqB = x2i(receivedChar,7,8);          // Received from mqtt background colour 
      for (unsigned int i = 0; i < (receivedlenght - 9); i++) {mqT[i] = receivedChar[i + 9];} // Received from mqtt text
      mqT[receivedlenght - 9] = '\0';
     
      int fff = fieldval[V].area - 1;
      int iarX = areas[fff].Xstart;         int iarY = areas[fff].Ystart;
      int iXb = iarX + fieldval[V].Xvbsta;  int iYb = iarY + fieldval[V].Yvbsta;
      int iXbl = fieldval[V].Xvblen;        int iYbl = fieldval[V].Yvblen;
      int iXv = iXb + fieldval[V].Xvoff;    int iYv = iYb + fieldval[V].Yvoff;
      int iXvl = fieldval[V].Xvlen;         int iYvl = fieldval[V].Yvlen;
      int fg = tCol(mqC); int bg = tCol(mqB);
      //cyd.drawRoundRect(iXb, iYb, iXbl, iYbl, 5, fg);
      //cyd.drawRect(iXb, iYb, iXbl, iYbl, fg); // tCol(6));
      PrintInArea2Screen(iXb, iYb, iXbl, iYbl, iXv, iYv, mqS, tCol(mqC), tCol(mqB), mqT);
      break;
    }


    case 'A': {
      mqX = x2i(receivedChar,1,3);            // Received from mqtt start a X position                
      mqY = x2i(receivedChar,4,6);            // Received from mqtt start a Y position
      mqlX = x2i(receivedChar,7,9);           // Received from mqtt lenght off X area
      mqlY = x2i(receivedChar,10,12);         // Received from mqtt lenght off Y area
      mqtX = x2i(receivedChar,13,15);         // Received from mqtt X start of text
      mqtY = x2i(receivedChar,16,18);         // Received from mqtt X start of text
      mqS = x2i(receivedChar,19,20);          // Received from mqtt text size
      mqC = x2i(receivedChar,21,22);          // Received from mqtt text colour
      mqB = x2i(receivedChar,23,24);          // Received from mqtt background colour 
      for (unsigned int i = 0; i < (receivedlenght - 25); i++) {mqT[i] = receivedChar[i + 25];} // Received from mqtt text
      mqT[receivedlenght - 25] = '\0';
      PrintInArea2Screen(mqX, mqY, mqlX, mqlY,mqtX, mqtY, mqS, tCol(mqC), tCol(mqB), mqT);
      break;
    }
    case 'X' : { mqttclient.publish(out_error, "Restart" ,false);          // REMOTE RESTART
                 delay(1000); ESP.restart(); break; }
    
    default: {                                                             // Wrong command
      #if defined(TEST)
        mqttclient.publish(out_error, "-1" ,false);
      #else
        mqttclient.publish(out_error, "No valid command" ,false);
        statusreset = true;
        delay(300);
      #endif
      break;
    } 
  }
  // DIRTY TRICK to read all mqtt's fast if they are stacked
  // This overwrites the normal mqtt request timing by simulating an "older" timestamp
  prevMQTTMillis = currentMillis - (interval - 1); //250 );   // But at least 250ms before retrigger
}
// mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm mqtt connection    end mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm


// tttttttttttttttttttttttttttttttttttttttttttttt touch related start tttttttttttttttttttttttttttttttttttttttttttttt
#if !defined(LCDtypeN)

void printTouch2Serial() {
  Serial.println();
  Serial.print("Touched X/Y: ");
  Serial.print(X);
   Serial.print("/");
  Serial.println(Y);
}

void findTouchPos() {
//  printTouch2Serial();
//  printTouch2Screen();
  
  int Xh; int Yh;   int fh;
  int fhh = -1;  // Initialize with an invalid value

  // Loop through each quadrant
  for (int i = 0; i < XYtamax; i++) {
    // Calculate quadrant boundaries
    Xh = areas[i].Xstart + areas[i].Xlen;
    Yh = areas[i].Ystart + areas[i].Ylen;
    fh = areas[i].area;
    // Check if touch position is within current quadrant
    if (X >= areas[i].Xstart && X <= Xh && Y >= areas[i].Ystart && Y <= Yh) {
      // Update fhh if current quadrant has lower fh value
      if (fhh == -1 || fh < fhh) {
        fhh = fh;
      }
    }
  }
  if (fhh != -1) {
    Serial.println("Touched quadrant with field value: " + String(fhh));
    field = fhh;
    mqttclient.publish(out_button, (String(field).c_str()),false);
  } else {
    Serial.println("No valid quadrant touched.");
  }
}

void showTouchTable() {
  for (int i = 0; i < XYtamax; i++){
    char ftext[3];
  itoa(areas[i].area, ftext, 10);
  Serial.print(areas[i].area);
  // Write number to positioned field
  PrintField2Screen(areas[i].area, 2, 18, 2, ftext);

  }
}

#endif

// tttttttttttttttttttttttttttttttttttttttttttttt touch related end tttttttttttttttttttttttttttttttttttttttttttttt


// pppppppppppppppppppppppppppppppppppppppppppppp print to screen start pppppppppppppppppppppppppppppppppppppppppppp

void PrintField2Screen(int thefield, int size, uint16_t colour, uint16_t bkcolour, char *text){ 
  int fxx = areas[thefield - 1].Xstart;
  int fyy = areas[thefield - 1].Ystart;
  int fxlen = areas[thefield - 1].Xlen;
  int fylen = areas[thefield - 1].Ylen; 

  int fxtext = areas[thefield - 1].Xstart + areas[thefield - 1].Xtext;
  int fytext = areas[thefield - 1].Ystart + areas[thefield - 1].Ytext; 

  cyd.fillRect(fxx,fyy,fxlen,fylen,bkcolour);
  cyd.setTextSize(size);
  cyd.setTextColor(colour); 
  cyd.setCursor(fxtext,fytext);
  cyd.print(text);
}

void PrintText2Screen(int Xpos, int Ypos, int size, uint16_t colour, uint16_t bkcolour, char *text){
  int lenght = strlen(text);
  cyd.fillRect(Xpos-1,Ypos-1,(lenght *size *6),size * 8,bkcolour);
  cyd.setTextSize(size);
  cyd.setTextColor(colour); 
  cyd.setCursor(Xpos,Ypos);
  cyd.print(text);
}

void PrintInArea2Screen(int Xpos, int Ypos, int Xlen, int Ylen,int Xtpos, int Ytpos,int size, uint16_t colour, uint16_t bkcolour, const char *text) {
//  int lenght = strlen(text);
  cyd.fillRect(Xpos,Ypos,Xlen,Ylen,bkcolour);
  cyd.setTextSize(size);
  cyd.setTextColor(colour); 
  cyd.setCursor(Xtpos,Ytpos);
  cyd.print(text);
}

void PrintArea2Screen(int Xpos, int Ypos, int Xlen, int Ylen,int Xtpos, int Ytpos,int size, uint16_t colour, uint16_t bkcolour, const char *text) {
  int lenght = strlen(text);
  cyd.fillRect(Xpos,Ypos,Xlen,Ylen,bkcolour);
  cyd.setTextSize(size);
  cyd.setTextColor(colour); 
  cyd.setCursor(Xtpos,Ytpos);
  cyd.print(text);
}

void PrintValInAr2Screen(int Xpos, int Ypos, int Xlen, int Ylen,int Xtpos, int Ytpos,int size, uint16_t colour, uint16_t bkcolour, const char *text) {
  int lenght = strlen(text);
  cyd.fillRect(Xpos,Ypos,Xlen,Ylen,bkcolour);
  cyd.setTextSize(size);
  cyd.setTextColor(colour); 
  cyd.setCursor(Xtpos,Ytpos);
  cyd.print(text);
}

void ShowArea(){ 
  for (int j = 0; j < Fieldmax; j++){  // Write the text
    int fff = fieldval[j].area - 1;
    int iarX = areas[fff].Xstart;         int iarY = areas[fff].Ystart;
    int iXb = iarX + fieldval[j].Xvbsta;  int iYb = iarY + fieldval[j].Yvbsta;
    int iXbl = fieldval[j].Xvblen;        int iYbl = fieldval[j].Yvblen;
    int iXv = iXb + fieldval[j].Xvoff;    int iYv = iYb + fieldval[j].Yvoff;
    int iXvl = fieldval[j].Xvlen;         int iYvl = fieldval[j].Yvlen;
    int fg = tCol(5); int bg = tCol(8);; int dg = tCol(17);
    int is = 1;
    char itx[3]; itoa(j+1, itx, 10);
    cyd.drawRoundRect(iXb, iYb, iXbl, iYbl, 5, dg);
    //cyd.drawRect(iXb, iYb, iXbl, iYbl, fg); // tCol(6));
    PrintInArea2Screen(iXv, iYv, iXvl, iYvl, iXv, iYv, is, fg, bg, itx);
  }
}

void ShowTouch(){                      // MQTT H
  for (int i = 0; i < (XYtamax); i++){  // Draw rectangle to show the fields that contain variables
    int iX = areas[i].Xstart; int iY = areas[i].Ystart;
    int iXl = areas[i].Xlen;  int iYl = areas[i].Ylen;
    int iXt = areas[i].Xstart + areas[i].Xtext; int iYt = areas[i].Ystart + areas[i].Ytext;
    int fg = tCol(10);  int bg = tCol(18);
    int is = 1;
    char itx[10]; 
    strcpy(itx, "Touch ");
    char numStr[3];
    itoa(i + 1, numStr, 10);
    strcat(itx, numStr);
    cyd.drawRect(iX, iY,iXl, iYl, fg); 
    PrintInArea2Screen(iX, iY, iXl, iYl, iXt, iYt, is, fg, bg, itx);
  }
} 

#if !defined(LCDtypeN)
void printTouch2Screen(){
  cyd.fillRect(270,230,40,20,ILI9341_BLACK);
  cyd.setTextSize(1);
  cyd.setTextColor(ILI9341_CYAN); 
  cyd.setCursor(270,230);
  cyd.print(X);
  cyd.print("/");
  cyd.println(Y);
}
#endif
// pppppppppppppppppppppppppppppppppppppppppppppp print to screen  end  pppppppppppppppppppppppppppppppppppppppppppp


// HHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHH helpers start HHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHH

// Helper function for substring to hex conversion 
int x2i(char *s, int from_a, int to_b) {// String, starting position, endposition in string
  int x = 0;
  for(int a = from_a;  a <= to_b ;a++) {
 //   char c = *s;
    char c = s[a];
    if (c >= '0' && c <= '9') {   // Just the numbers
      x *= 16;
      x += c - '0'; 
    }
    else if (c >= 'A' && c <= 'F') {  // When using big letters
      x *= 16;
      x += (c - 'A') + 10; 
    }
    else if (c >= 'a' && c <= 'f') {  // When using low letters
      x *= 16;
      x += (c - 'a') + 10; 
    }
    else break;
    //s++;
  }
  return x;
}

uint16_t tCol(int colconst) {          // Transform easy mqtt colour number to ILI9341 unint_16 colour
  uint16_t result = 40;
  result = Colourtable[colconst];
  return result;
}  

void LedControl(){ 
  if (LEDsta_used) {
    LEDsta_BL = (LEDsta & 0b11);         // Extracting 2 bits starting from the rightmost side (Bit0 and Bit1)
    LEDsta_R = ((LEDsta >> 2) & 0b11);   // Shifting 2 bits to the right and then extracting the next 2 bits
    LEDsta_G = ((LEDsta >> 4) & 0b11);   // Shifting 4 bits to the right and then extracting the next 2 bits
    LEDsta_B = ((LEDsta >> 6) & 0b11);   // Shifting 6 bits to the right and then extracting the next 2 bits
    LEDsta_used = LOW;
  }
  ldr = (analogRead(CYD_LDR));
  blk_now = blk_set;

  switch(LEDsta_BL){
    /*    // Used not dimmed
    case 0:{digitalWrite(CYD_BL, Boff);break;}
    case 1:{digitalWrite(CYD_BL, Bon);break;}
    case 2:{digitalWrite(CYD_BL, Ticker);break;}
    case 3:{digitalWrite(CYD_BL, !Ticker);break;}
    */   // And here for dimmed mode
    case 0:{blk_now = 0 ;break;}    
    case 1:{blk_now = blk_set;break;}
    case 2: {
      if (ldr >= 0 && ldr <= 10) {blk_now = 255;}
      else if (ldr >= 11 && ldr <= 100) {blk_now = 64;}      
      else if (ldr >= 101 && ldr <= 500) {blk_now = 16;}
      else {blk_now = 04;}
      break;}
    case 3: {
      if (!Ticker) {blk_now = blk_set;}
      else { blk_now = 0;};break;}
    default: {break;}
  }
  if (blk_now != blk_last){analogWrite(CYD_BL,blk_now); blk_last = blk_now;}
  switch (LEDsta_R)
    {                                  
    case 0:{digitalWrite(CYD_LED_RED, Loff);break;}
    case 1:{digitalWrite(CYD_LED_RED, Lon);break;}
    case 2:{digitalWrite(CYD_LED_RED, Ticker);break;}
    case 3:{digitalWrite(CYD_LED_RED, !Ticker);break;}
    default:{break;}
  }
  switch(LEDsta_G){                                  
    case 0:{digitalWrite(CYD_LED_GREEN, Loff);break;}
    case 1:{digitalWrite(CYD_LED_GREEN, Lon);break;}
    case 2:{digitalWrite(CYD_LED_GREEN, Ticker);break;}
    case 3:{digitalWrite(CYD_LED_GREEN, !Ticker);break;}
    default:{break;}
  }
  switch(LEDsta_B){                                  
    case 0:{digitalWrite(CYD_LED_BLUE, Loff);break;}
    case 1:{digitalWrite(CYD_LED_BLUE, Lon);break;}
    case 2:{digitalWrite(CYD_LED_BLUE, Ticker);break;}
    case 3:{digitalWrite(CYD_LED_BLUE, !Ticker);break;}
  default:{break;}
  }
}


void MakeColourTable() {              // Make mqtt colour definition easier
  Colourtable[0] = ILI9341_BLACK;           // 0x0000  //   0,   0,   0
  Colourtable[1] = ILI9341_BLUE;            // 0x001F  //   0,   0, 255
  Colourtable[2] = ILI9341_CYAN;            // 0x07FF  //   0, 255, 255
  Colourtable[3] = ILI9341_DARK;            // 050505  //   0,   5,   5 a very dark gray
  Colourtable[4] = ILI9341_DARKCYAN;        // 0x03EF  //   0, 125, 123
  Colourtable[5] = ILI9341_DARKGREEN;       // 0x03E0  //   0, 125,   0
  Colourtable[6] = ILI9341_DARKGREY;        // 0x7BEF  // 123, 125, 123
  Colourtable[7] = ILI9341_GREEN;           // 0x07E0  //   0, 255,   0
  Colourtable[8] = ILI9341_GREENYELLOW;     // 0xAFE5  // 173, 255,  41
  Colourtable[9] = ILI9341_LIGHTGREY;       // 0xC618  // 198, 195, 198
  Colourtable[10] = ILI9341_MAGENTA;        // 0xF81F  // 255,   0, 255
  Colourtable[11] = ILI9341_MAROON;         // 0x7800  // 123,   0,   0
  Colourtable[12] = ILI9341_NAVY;           // 0x000F  //   0,   0, 123
  Colourtable[13] = ILI9341_OLIVE;          // 0x7BE0  // 123, 125,   0
  Colourtable[14] = ILI9341_ORANGE;         // 0xF8 0xF// 255, 165,   0
  Colourtable[15] = ILI9341_PINK;           // 0xFC18  // 255, 130, 198
  Colourtable[16] = ILI9341_PURPLE;         // 0x780F  // 123,   0, 123
  Colourtable[17] = ILI9341_RED;            // 0xF800  // 255,   0,   0
  Colourtable[18] = ILI9341_WHITE;          // 0xFFFF  // 255, 255, 255
  Colourtable[19] = ILI9341_YELLOW;         // 0xFFE0  // 255, 255,   0
  Colourtable[20] = ILI9341_DARK_BLUE;      // 0xFFE0  //   0,   5,   5 a very dark blue
}
// HHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHH helpers  end  HHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHH


void WitchField2Screen(int thefield){
  int fxx;  
  int fyy;
  char ftext[3];
  itoa(thefield, ftext, 10);
  fxx = areas[thefield - 1].Xstart;
  fyy = areas[thefield - 1].Ystart;
  Serial.print(thefield);
  Serial.print(">>");
  Serial.print(fxx);
  Serial.print("/");
  Serial.print(fyy); 
  // Write number to positioned field
  PrintField2Screen(thefield, 2, 18, 2, ftext);
}

void What2DoInField(int field){
  switch (field) {
    case 1: { WitchField2Screen(field); break;}
    case 2: { WitchField2Screen(field); break;}
    case 3: { WitchField2Screen(field); break;}
    case 4: { WitchField2Screen(field); break;}
    case 5: { WitchField2Screen(field); break;}
    case 6: { WitchField2Screen(field); break;}
    case 7: { WitchField2Screen(field); break;}
    case 8: { WitchField2Screen(field); break;}
    case 9: { WitchField2Screen(field); break;}
    case 10: { WitchField2Screen(field); break;} 
    case 11: { WitchField2Screen(field); break;}
    case 12: { WitchField2Screen(field); break;}
    default: { Serial.println(" Invalid field given"); break;}
  }
}


// XXXXXXXXXXXXXXXXXXXXXXXX PROGRAM  START XXXXXXXXXXXXXXXXXXXXXXX

void setup() {
  // Initialize onboard LEDs with "active low" -->  HIGH == off, LOW == on
  adc_attenuation_t attenuation = ADC_11db;
  analogSetAttenuation(attenuation);
  pinMode(CYD_LDR, INPUT);        // Measure suround light.

  pinMode(CYD_LED_BLUE, OUTPUT);
  pinMode(CYD_LED_GREEN, OUTPUT);
  pinMode(CYD_LED_RED, OUTPUT);
  digitalWrite(CYD_LED_RED, Loff);    
  digitalWrite(CYD_LED_GREEN, Loff);
  digitalWrite(CYD_LED_BLUE, Loff);

  // Initialize debug output and wifi and preset mqtt
  Serial.begin(115200);

  // Start screen
   // cyd.setFont(&Picopixel);
  pinMode(CYD_BL, OUTPUT);
  //digitalWrite(CYD_BL,Bon);           // As Adafruit library does not set backlight on
  analogWrite(CYD_BL,blk_set);
 
  cyd.begin();                        // Display with LED in  -lower-left-  corner
  cyd.setRotation(1);                
  cyd.fillScreen(ILI9341_BLACK); 
  cyd.setCursor(0, 0);
  Serial.println("");
  Serial.println("");
  Serial.print("I am: ");
  Serial.print(iamclient);
  #if defined(LCDtypeC)
    Serial.println(" a CYD ILI9341 with CST820 touch.");
  #elif defined(LCDtypeR)
    Serial.println(" a CYD ILI9341 with XPT2046 touch.");
  #elif defined(LCDtypeN)
    Serial.println(" a CYD ILI9341 with no touch.");
  #endif
  Serial.println("");

  cyd.setTextColor(ILI9341_WHITE);  
  cyd.setTextSize(1);
  cyd.setCursor(0, 0);
  cyd.println("I am a CYD ILI9341");
  #if defined(LCDtypeC)
    cyd.println("with CST820 touch:");
  #elif defined(LCDtypeR)
    cyd.println("with XPT2046 touch:");
   #elif defined(LCDtypeN)
    cyd.println("with no touch:");
  #endif
  cyd.setTextColor(ILI9341_GREEN); 
  cyd.setTextSize(2); 
  cyd.setCursor(120, 0);
  cyd.print(iamclient);

  setup_wifi();                     // Start the wifi connection
  delay(100);
  mqttclient.setClient(espClient);
  mqttclient.setServer(mqtt_server, mqtt_port);
  mqttclient.setCallback(callback);
  mqttclient.setKeepAlive(15);      // MQTT_KEEPALIVE : keepAlive interval in seconds. Override setKeepAlive()
  mqttclient.setSocketTimeout(15);  // MQTT_SOCKET_TIMEOUT: socket timeout interval in Seconds. Override setSocketTimeout()
  void reconnect();                 // Start the mqtt connection
  mqttclient.subscribe(in_topic);   // Listen to the mqtt inputs
 
  // Prepare static screen
  MakeColourTable();
  MakeScreenTable();

  // Start touch
  #if !defined(LCDtypeN) 
//    MakeTouchTable();
  //  MakeAreaTable();
#if defined(LCDtypeC)
      touch.init(CST820_SDA, CST820_SCL, CST820_RST, CST820_IRQ);	// sda, scl, rst, irq
      int iType = touch.sensorType();
      Serial.printf("Sensor type = %s\n", szNames[iType]);
    #elif defined(LCDtypeR)
      touchscreen.begin();
    #endif
  #endif
}


//  This is the Main loop
void loop() {                             
  unsigned long currentMillis = millis();
  if (currentMillis - prevTickerMillis >= 500) {
    prevTickerMillis = currentMillis;
    Ticker = !Ticker;
  }

  // Every X number of seconds (interval = x seconds) it publishes a new MQTT message
  if (currentMillis - prevMQTTMillis >= interval) {
    // Save the last time a new reading was published
    prevMQTTMillis = currentMillis;

    if (WiFi.status() == WL_CONNECTED) {
      Serial.print("+");
      }
    else {
      WiFi.begin(wifi_ssid, wifi_password);
      Serial.println("Try Wifi reconnect "); 
    }
//     mqttclient.loop();          // Request if there is a message
   
    if (!mqttclient.connected()) {
      reconnect();
      mqttclient.subscribe(in_topic);
      Serial.print("Go in loop with topic: " );
      Serial.println(in_topic);
      #if defined(TEST)
        mqttclient.publish(out_error, "2" ,false);
      #else
        mqttclient.publish(out_error, "Reconnected" ,false);
      #endif
      statusreset = true;
    }
    mqttclient.loop();          // Request if there is a message
    
    watchdog = !watchdog;       // Create toogeling watchdog signal
    mqttclient.publish(out_watchdog, String(watchdog).c_str() ,false);  // Only use when in doubts of loop

    if (statusreset){
      statusreset = false;
      // delay(1000);
      #if defined(TEST)
        mqttclient.publish(out_error, "0" ,false);
      #else
        mqttclient.publish(out_error, "Normal" ,false);
      #endif
    }
  }

  #if ! defined(LCDtypeN) 
    #if defined(LCDtypeR)                        // Version resistive
      touchscreen.getTouch();     // Dirty trick to get rid off false signals after mqtt receive
      delay(100);
      TouchPoint touch = touchscreen.getTouch();

      Serial.print(touch.xRaw); Serial.print("-");
      Serial.print(touch.yRaw); Serial.print("-");
      Serial.print(touch.zRaw); Serial.println("");
      if (touch.zRaw != 0) {
        digitalWrite(CYD_LED_BLUE, Lon);
        float helper;
        X = touch.x;
        Y = touch.y;

        helper = (X - XTmin) * XTdiv * 1.0;
        X = helper;
        helper = (Y - YTmin) * YTdiv * 1.0;
        Y = helper;

        findTouchPos();
        delay(250);
        // What2DoInField(field);
      }
    #endif 

    #if defined(LCDtypeC)                        // Version capacitive
      if (touch.getSamples(&ti)) {          // only when touch event happened
       // digitalWrite(CYD_LED_BLUE, Lon);
        Y = ti.x[0];
        X = ti.y[0];

        findTouchPos();
        // What2DoInField(field);
      }
      else { 
        digitalWrite(CYD_LED_BLUE, Loff);  
        }
    #endif  
  #endif

  LedControl();

  #if defined(TEST)
  // mqttclient.publish(out_topic, String(looped).c_str() ,false); // Only use that when in doubts of loop
    looped++;
  #endif
}