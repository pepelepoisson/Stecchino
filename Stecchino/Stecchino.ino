// Libraries required
#include "I2Cdev.h"
#include "MPU6050.h"
#include "Wire.h"
#include "RunningMedian.h"
#include "FastLED.h" // Librarie required for addressable LEDs
#include <avr/interrupt.h> // Librairies for sleep mode
#include <avr/power.h> // Librairies for sleep mode
#include <avr/sleep.h> // Librairies for sleep mode
#include <avr/io.h> // Librairies for sleep mode

#define NUM_LEDS 72  // How many leds in your strip?
#define HIGH_BRIGHTNESS 50  // Set LEDS brightness
#define LOW_BRIGHTNESS 10  // Set LEDS brightness
#define DATA_PIN 10
#define MOSFET_GATE 11
#define MPU_POWER_PIN 9
#define PushB1  2 // Pin for push button 1  
#define Button_1_On  (!digitalRead(PushB1))
#define FRAMES_PER_SECOND  120
#define ARRAY_SIZE(A) (sizeof(A) / sizeof((A)[0]))
#define SET_IDLE_MILLISECONDS 20000 // how many seconds at idle before moving to Fake_sleep?
#define SET_FAKE_SLEEP_MILLISECONDS 60000 // how many seconds at Fake_sleep before moving to Sleep?
#define SET_MAX_SPIRIT_LEVEL_MILLISECONDS 20000 // how many seconds at Fake_sleep before moving to Sleep?
#define NUM_LEDS_PER_SECONDS 2 // how many LEDs are turned on every second when playing?

// Variables used in CheckAccel() routine
MPU6050 accelgyro;
int16_t ax, ay, az;
int16_t gx, gy, gz;
String accel_status=String("unknown");
RunningMedian a_forwardRollingSample = RunningMedian(5);
RunningMedian a_sidewayRollingSample = RunningMedian(5);
RunningMedian a_verticalRollingSample = RunningMedian(5);
#define ACCELEROMETER_ORIENTATION 2     // 0, 1 or 2 to set the angle of the joystick
int a_forward_offset=0,a_sideway_offset=0,a_vertical_offset=0;
enum POSITION_BOUTONS { NONE, EN_HAUT,AU_DESSUS,EN_BAS,AU_DESSOUS, EN_AVANT, EN_ARRIERE, COUNT };  // Used to detect position of buttons relative to Stecchino and user
const char *position_boutons[COUNT] = { "None", "EN_HAUT", "AU_DESSUS", "EN_BAS", "AU_DESSOUS", "EN_AVANT", "EN_ARRIERE" };
uint8_t orientation_boutons = NONE;

unsigned long start_time=0, current_time=0, elapsed_time=0, record_time=0, previous_record_time=0;
int i=0;
float angle_2_horizon=0;

CRGB leds[NUM_LEDS];  // Define the array of leds

void setup() {

  Serial.begin(38400);
  while (!Serial);

  pinMode(PushB1,INPUT);
  digitalWrite(PushB1,HIGH);  // Configure built-in pullup resitor for push button 1

  pinMode(MOSFET_GATE,OUTPUT);
  digitalWrite(MOSFET_GATE,HIGH);


  pinMode(MPU_POWER_PIN,OUTPUT);
  digitalWrite(MPU_POWER_PIN,HIGH);

  delay(500);
  
  // LEDs strip
  FastLED.addLeds<WS2812B, DATA_PIN, GRB>(leds, NUM_LEDS);  
  FastLED.setBrightness(LOW_BRIGHTNESS);  
  
  // MPU
  Wire.begin();
  accelgyro.initialize();  
  Serial.println("Starting MPU... ");
  
  a_forward_offset=0;
  a_sideway_offset=0;
  a_vertical_offset=-100;

  //delay(2000);
  Serial.print("VCC=");
  Serial.print(readVcc());
  Serial.println("mV");
  
  start_time=millis();
}

// List of patterns to cycle through.  Each is defined as a separate function below.
typedef void (*SimplePatternList[])();
SimplePatternList gPatterns = {sinelon, juggle, bpm, rainbow, confetti };
char* SimplePatternNames[]={"sinelon", "juggle", "bpm", "rainbow", "confetti" };
uint8_t gCurrentPatternNumber = 1; // Index number of which pattern is current
uint8_t gHue = 0; // rotating "base color" used by many of the patterns
uint8_t gFrameCount = 0; // Inc by 1 for each Frame of Trasition, New/Changed connection(s) pattern
#define WAKE_UP_TRANSITION 1000 //duration of animation when system returns from sleep (ms)
#define START_PLAY_TRANSITION 500 //duration of animation when game starts (ms)
#define GAME_OVER_TRANSITION 1000 //duration of game over animation (ms)
#define SLEEP_TRANSITION 2000 //duration of animation to sleep (ms)

void LEDS_ON(int count, int record){
    for (int i = NUM_LEDS; i >=0; i--) {
      if (i<=NUM_LEDS-count){leds[i]=CRGB::Black;}
      //else {leds[i]=CRGB::Green;}
      else {leds[i]=CHSV(gHue, 255, 255);}
      if (i==NUM_LEDS-record){leds[i]=CRGB::Red;}
    }

  // send the 'leds' array out to the actual LED strip
  FastLED.show();  
  // insert a delay to keep the framerate modest
  FastLED.delay(1000/FRAMES_PER_SECOND);       
  // do some periodic updates
  EVERY_N_MILLISECONDS( 20 ) { gHue++; } // slowly cycle the "base color" through the rainbow
}

void LED(String pattern){
  if (pattern=="going_to_sleep"){
    digitalWrite(MOSFET_GATE,HIGH);
    FastLED.setBrightness(LOW_BRIGHTNESS); 
    for (int i = NUM_LEDS; i >=0; i--){
      leds[i]=CRGB::Blue;
    }
  }
  
  if (pattern=="idle"){
    digitalWrite(MOSFET_GATE,HIGH);
    FastLED.setBrightness(HIGH_BRIGHTNESS); 
    confetti();
    //rainbow();
  }
      
  if (pattern=="start_play"){
    digitalWrite(MOSFET_GATE,HIGH);
    FastLED.setBrightness(LOW_BRIGHTNESS); 
    for (int i = NUM_LEDS; i >=0; i--){
      leds[i]=CRGB::Green;
    }
  }
   
   if (pattern=="wahoo"){
    digitalWrite(MOSFET_GATE,HIGH);
    FastLED.setBrightness(HIGH_BRIGHTNESS); 
    // Call the pattern function once, updating the 'leds' array
    //sinelon();
    //juggle();
    //bpm();
    //rainbow();
    //rainbowWithGlitter();
    //confetti();
    //gPatterns[gCurrentPatternNumber]();
    redGlitter();
  } 
   
   if (pattern=="spirit_level"){
    digitalWrite(MOSFET_GATE,HIGH);
    FastLED.setBrightness(HIGH_BRIGHTNESS); 
    sinelon();
  } 
    
  if (pattern=="game_over"){
    digitalWrite(MOSFET_GATE,HIGH);
    FastLED.setBrightness(LOW_BRIGHTNESS); 
    for (int i = NUM_LEDS; i >=0; i--){
      leds[i]=CRGB::Red;
    }
  }
  
  if (pattern=="off"){
    for (int i = NUM_LEDS; i >=0; i--) {
      //leds[i]=CRGB::Black;
      leds[i].nscale8(230);
    }
    digitalWrite(MOSFET_GATE,LOW);
  }

  // send the 'leds' array out to the actual LED strip
  FastLED.show();  
  // insert a delay to keep the framerate modest
  FastLED.delay(1000/FRAMES_PER_SECOND);       
  // do some periodic updates
  EVERY_N_MILLISECONDS( 20 ) { gHue++; } // slowly cycle the "base color" through the rainbow
}

void SPIRIT_LEVEL_LED(float angle){
    digitalWrite(MOSFET_GATE,HIGH);
    FastLED.setBrightness(HIGH_BRIGHTNESS);
    int int_angle=int(angle);
    //int pos_led=map(int_angle,-90,90,1,NUM_LEDS);
    //int pos_led=map(int_angle,45,-45,1,NUM_LEDS);
    int pos_led=map(int_angle,-45,45,1,NUM_LEDS);
    int couleur_led=map(pos_led,0,NUM_LEDS,0,255);
    for (int i = NUM_LEDS; i >=0; i--){
      if (i==pos_led){leds[i]=CHSV(couleur_led, 255, 255);}
      //if (i==pos_led){leds[i]=CRGB::Blue;}
      else {leds[i]=CRGB::Black;}
    }
    // send the 'leds' array out to the actual LED strip
    FastLED.show();  
    // insert a delay to keep the framerate modest
    FastLED.delay(1000/FRAMES_PER_SECOND);  
    // do some periodic updates
  EVERY_N_MILLISECONDS( 20 ) { gHue++; } // slowly cycle the "base color" through the rainbow 
}

void nextPattern()
{
  // add one to the current pattern number, and wrap around at the end
  gCurrentPatternNumber = (gCurrentPatternNumber + 1) % ARRAY_SIZE( gPatterns);
}

void rainbow() 
{
  // FastLED's built-in rainbow generator
  fill_rainbow( leds, NUM_LEDS, gHue, 7);
}

void confetti() 
{
  // random colored speckles that blink in and fade smoothly
  fadeToBlackBy( leds, NUM_LEDS, 10);
  int pos = random16(NUM_LEDS);
  leds[pos] += CHSV( gHue + random8(64), 200, 255);
}

void sinelon()
{
  // a colored dot sweeping back and forth, with fading trails
  fadeToBlackBy( leds, NUM_LEDS, 20);
  int pos = beatsin16(13,0,NUM_LEDS);
  leds[pos] += CHSV( gHue, 255, 192);
}

void bpm()
{
  // colored stripes pulsing at a defined Beats-Per-Minute (BPM)
  uint8_t BeatsPerMinute = 62;
  CRGBPalette16 palette = PartyColors_p;
  uint8_t beat = beatsin8( BeatsPerMinute, 64, 255);
  for( int i = 0; i < NUM_LEDS; i++) { //9948
    leds[i] = ColorFromPalette(palette, gHue+(i*2), beat-gHue+(i*10));
  }
}

void juggle() {
  // eight colored dots, weaving in and out of sync with each other
  fadeToBlackBy( leds, NUM_LEDS, 20);
  byte dothue = 0;
  for( int i = 0; i < 8; i++) {
    leds[beatsin16(i+7,0,NUM_LEDS)] |= CHSV(dothue, 200, 255);
    dothue += 32;
  }
}

void redGlitter() {
  gFrameCount += 1;
  if (gFrameCount % 4 == 1) { // Slow down frame rate
    for ( int i = 0; i < NUM_LEDS; i++) {
      leds[i] = CHSV(HUE_RED, 0, random8() < 60 ? random8() : random8(64));
    }
  }
}

void alloff() {
  for (int i = NUM_LEDS; i >=0; i--) {
    leds[i]=CRGB::Black;
    delay(10);
    FastLED.show();
  }
  //FastLED.show();
}

enum {Check_Battery,Wake_Up_Transition,Idle,Start_Play_Transition,Play,Wahoo,Game_Over_Transition, Spirit_Level,Magic_Wand,Sleep_Transition,Fake_Sleep} condition=Idle;

void loop() {
  angle_2_horizon=CheckAccel();
  switch (condition) {

  case Check_Battery:
    Serial.print("VCC=");
    Serial.print(readVcc());
    Serial.println("mV");
    condition=Idle;
    break;  
    
  case Wake_Up_Transition:
    break;
    
  case Idle:      
    if (millis()-start_time>SET_IDLE_MILLISECONDS){condition=Fake_Sleep;start_time=millis();}
    if (accel_status=="straight"){condition=Start_Play_Transition;start_time=millis();}
    if (orientation_boutons==EN_AVANT){condition=Spirit_Level;start_time=millis();}
    else {LED("idle");}
    break;
    
  case Start_Play_Transition:
    //if (millis()-start_time>START_PLAY_TRANSITION){condition=Play;start_time=millis();previous_record_time=record_time;}
    //else {LED("start_play");}
    alloff();
    condition=Play;
    start_time=millis();
    previous_record_time=record_time;
    break;
    
  case Play:
    elapsed_time=(millis()-start_time)/1000;
    if (elapsed_time>record_time){record_time=elapsed_time;}
    if (elapsed_time>previous_record_time && elapsed_time<=previous_record_time+1 && previous_record_time !=0){LED("wahoo");}
    else {LEDS_ON(NUM_LEDS_PER_SECONDS*int(elapsed_time),NUM_LEDS_PER_SECONDS*int(record_time));} 
    if (accel_status=="fallen"){condition=Game_Over_Transition;start_time=millis();}
    break;
    
  case Wahoo:
    break;
    
  case Game_Over_Transition:
    if (millis()-start_time>GAME_OVER_TRANSITION){condition=Idle;start_time=millis();}
    else {LED("game_over");}
    break;

  case Spirit_Level:
    if (orientation_boutons==AU_DESSOUS || orientation_boutons==AU_DESSUS){condition=Idle;start_time=millis();}
    if (millis()-start_time>SET_MAX_SPIRIT_LEVEL_MILLISECONDS){condition=Fake_Sleep;start_time=millis();}
    //if (accel_status=="straight"){condition=Start_Play_Transition;start_time=millis();}
    SPIRIT_LEVEL_LED(angle_2_horizon);
    //Serial.print("Angle to horizon:");
    //Serial.println(angle_2_horizon);
    break;
    
   case Magic_Wand:
    break;
    
   case Fake_Sleep:
    if (millis()-start_time>SET_FAKE_SLEEP_MILLISECONDS){condition=Sleep_Transition;start_time=millis();}
    if (abs(angle_2_horizon)>15){condition=Idle;start_time=millis();}
    else LED("off");
    break; 
  
  case Sleep_Transition:
    if (millis()-start_time>SLEEP_TRANSITION){sleepNow();}
    else LED("going_to_sleep");
    break;
  }
  FastLED.show();
}

float CheckAccel(){
  // Reads acceleration from MPU6050 to evaluate current condition.
  // Tunables: 
  // Output values: still, cruising, braking, fallen, unknown

  // Get accelerometer readings
  accelgyro.getMotion6(&ax, &ay, &az, &gx, &gy, &gz);

  float angle_2_horizon=0;

  // Offset accel readings
  int a_forward_offset = -2;
  int a_sideway_offset = 2;
  //int a_vertical_offset = 1;

  // Convert to expected orientation - includes unit conversion to "cents of g" for MPU range set to 2g
  int a_forward = (ACCELEROMETER_ORIENTATION == 0?ax:(ACCELEROMETER_ORIENTATION == 1?ay:az))/164;
  int a_sideway = (ACCELEROMETER_ORIENTATION == 0?ay:(ACCELEROMETER_ORIENTATION == 1?az:ax))/164;
  int a_vertical = (ACCELEROMETER_ORIENTATION == 0?az:(ACCELEROMETER_ORIENTATION == 1?ax:ay))/164;
  
  // Update rolling average for smoothing
  a_forwardRollingSample.add(a_forward);
  a_sidewayRollingSample.add(a_sideway);
  a_verticalRollingSample.add(a_vertical);

  // Get median
  int a_forwardRollingSampleMedian=a_forwardRollingSample.getMedian()-a_forward_offset;
  int a_sidewayRollingSampleMedian=a_sidewayRollingSample.getMedian()-a_sideway_offset;
  //int a_verticalRollingSampleMedian=a_verticalRollingSample.getMedian()-a_vertical_offset+100;
  int a_verticalRollingSampleMedian=a_verticalRollingSample.getMedian()-a_vertical_offset-100; 
  
  // for debugging
  //Serial.print("a_forward:");
  //Serial.print(a_forwardRollingSampleMedian);
  //Serial.print(" a_sideway:");
  //Serial.print(a_sidewayRollingSampleMedian);
  //Serial.print(" a_vertical:");
  //Serial.println(a_verticalRollingSampleMedian);
  
    
  // Evaluate current condition based on smoothed accelarations
  accel_status="unknown";
  if (abs(a_sidewayRollingSampleMedian)>abs(a_verticalRollingSampleMedian)||abs(a_forwardRollingSampleMedian)>abs(a_verticalRollingSampleMedian)){accel_status="fallen";}
  if (abs(a_sidewayRollingSampleMedian)<abs(a_verticalRollingSampleMedian)&&abs(a_forwardRollingSampleMedian)<abs(a_verticalRollingSampleMedian)){accel_status="straight";}
  //else {accel_status="unknown";}

  if (a_verticalRollingSampleMedian >= 80 && abs(a_forwardRollingSampleMedian) <= 25 && abs(a_sidewayRollingSampleMedian) <= 25 && orientation_boutons != EN_HAUT) {
    // coté 1 en haut
    orientation_boutons = EN_HAUT;
  } else if (a_forwardRollingSampleMedian >= 80 && abs(a_verticalRollingSampleMedian) <= 25 && abs(a_sidewayRollingSampleMedian) <= 25 && orientation_boutons != AU_DESSUS) {
    // coté 2 en haut
    orientation_boutons = AU_DESSUS;
  } else if (a_verticalRollingSampleMedian <= -80 && abs(a_forwardRollingSampleMedian) <= 25 && abs(a_sidewayRollingSampleMedian) <= 25 && orientation_boutons != EN_BAS) {
    // coté 3 en haut
    orientation_boutons = EN_BAS;
  } else if (a_forwardRollingSampleMedian <= -80 && abs(a_verticalRollingSampleMedian) <= 25 && abs(a_sidewayRollingSampleMedian) <= 25 && orientation_boutons != AU_DESSOUS) {
    // coté 4 en haut
    orientation_boutons = AU_DESSOUS;
  } else if (a_sidewayRollingSampleMedian >= 80 && abs(a_verticalRollingSampleMedian) <= 25 && abs(a_forwardRollingSampleMedian) <= 25 && orientation_boutons != EN_AVANT) {
    // coté LEDs en haut
    orientation_boutons = EN_AVANT;
  } else if (a_sidewayRollingSampleMedian <= -80 && abs(a_verticalRollingSampleMedian) <= 25 && abs(a_forwardRollingSampleMedian) <= 25 && orientation_boutons != EN_ARRIERE) {
    // coté batteries en haut
    orientation_boutons = EN_ARRIERE;
  }  else {
    // orientation = FACE_NONE;
  }
  
 //Serial.print(" - ");
 //Serial.print(position_boutons[orientation_boutons]);
 //Serial.print(" - ");
 //Serial.println(accel_status);

 angle_2_horizon=atan2(float(a_verticalRollingSampleMedian),float(max(abs(a_sidewayRollingSampleMedian),abs(a_forwardRollingSampleMedian))))*180/PI;
 //Serial.print(" ");
 //Serial.println(angle_2_horizon);
 return angle_2_horizon;
 }


void sleepNow(void)
{
    // Set pin 2 as interrupt and attach handler:
    attachInterrupt(0, pinInterrupt, LOW);
    delay(100);
    //
    set_sleep_mode(SLEEP_MODE_PWR_DOWN);
    //
    // Set sleep enable (SE) bit:
    sleep_enable();
    //
    // Put the device to sleep:
    digitalWrite(MOSFET_GATE,LOW);   // turn LEDs off to indicate sleep
    digitalWrite(MPU_POWER_PIN,LOW);   // turn MPU off
    sleep_mode();
    //
    // Upon waking up, sketch continues from this point.
    sleep_disable();
    digitalWrite(MOSFET_GATE,HIGH);   // turn LED on to indicate awake 
    digitalWrite(MPU_POWER_PIN,HIGH);   // turn MPU off
    delay(100);

    // clear running median buffer
    a_forwardRollingSample.clear();
    a_sidewayRollingSample.clear();
    a_verticalRollingSample.clear();
    
    // retore MPU connection
    Wire.begin();
    accelgyro.initialize();  
    Serial.println("Restarting MPU... ");
    
    condition=Check_Battery;
    start_time=millis();
}

void pinInterrupt(void)
{
    detachInterrupt(0);
}

long readVcc() {
  // Read 1.1V reference against AVcc
  // set the reference to Vcc and the measurement to the internal 1.1V reference
  #if defined(__AVR_ATmega32U4__) || defined(__AVR_ATmega1280__) || defined(__AVR_ATmega2560__)
    ADMUX = _BV(REFS0) | _BV(MUX4) | _BV(MUX3) | _BV(MUX2) | _BV(MUX1);
  #elif defined (__AVR_ATtiny24__) || defined(__AVR_ATtiny44__) || defined(__AVR_ATtiny84__)
    ADMUX = _BV(MUX5) | _BV(MUX0);
  #elif defined (__AVR_ATtiny25__) || defined(__AVR_ATtiny45__) || defined(__AVR_ATtiny85__)
    ADMUX = _BV(MUX3) | _BV(MUX2);
  #else
    ADMUX = _BV(REFS0) | _BV(MUX3) | _BV(MUX2) | _BV(MUX1);
  #endif  

  delay(2); // Wait for Vref to settle
  ADCSRA |= _BV(ADSC); // Start conversion
  while (bit_is_set(ADCSRA,ADSC)); // measuring

  uint8_t low  = ADCL; // must read ADCL first - it then locks ADCH  
  uint8_t high = ADCH; // unlocks both

  long result = (high<<8) | low;

  result = 1125300L / result; // Calculate Vcc (in mV); 1125300 = 1.1*1023*1000
  return result; // Vcc in millivolts
}

