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
#define SET_IDLE_MILLISECONDS 20000 // how many seconds at idle before moving to sleep?

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


unsigned long start_time=0, current_time=0, elapsed_milliseconds=0;
int i=0;

CRGB leds[NUM_LEDS];  // Define the array of leds

void setup() {

  Serial.begin(9600);
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
  }
      
  if (pattern=="start_play"){
    digitalWrite(MOSFET_GATE,HIGH);
    FastLED.setBrightness(LOW_BRIGHTNESS); 
    for (int i = NUM_LEDS; i >=0; i--){
      leds[i]=CRGB::Green;
    }
  }
  
   if (pattern=="play"){
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

enum {Check_Battery,Wake_Up_Transition,Idle,Start_Play_Transition,Play,Wahoo,Game_Over_Transition, Sleep_Transition} condition=Idle;

void loop() {
  CheckAccel();
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
    if (millis()-start_time>SET_IDLE_MILLISECONDS){condition=Sleep_Transition;start_time=millis();}
    if (accel_status=="straight"){condition=Start_Play_Transition;start_time=millis();}
    else {LED("idle");}
    break;
    
  case Start_Play_Transition:
    if (millis()-start_time>START_PLAY_TRANSITION){condition=Play;start_time=millis();}
    else {LED("start_play");}
    break;
    
  case Play:
    LED("play");
    if (accel_status=="fallen"){condition=Game_Over_Transition;start_time=millis();}
    break;
    
  case Wahoo:
    break;
    
  case Game_Over_Transition:
    if (millis()-start_time>GAME_OVER_TRANSITION){condition=Idle;start_time=millis();}
    else {LED("game_over");}
    break;
    
  case Sleep_Transition:
    if (millis()-start_time>SLEEP_TRANSITION){sleepNow();}
    else LED("going_to_sleep");
    break;
  }
  FastLED.show();
}

void CheckAccel(){
  // Reads acceleration from MPU6050 to evaluate current condition.
  // Tunables: 
  // Output values: still, cruising, braking, fallen, unknown

  // Get accelerometer readings
  accelgyro.getMotion6(&ax, &ay, &az, &gx, &gy, &gz);

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
  /*Serial.print("a_forward:");
  Serial.print(a_forwardRollingSampleMedian);
  Serial.print(" a_sideway:");
  Serial.print(a_sidewayRollingSampleMedian);
  Serial.print(" a_vertical:");
  Serial.println(a_verticalRollingSampleMedian);
  */
  
  // Evaluate current condition based on smoothed accelarations
  accel_status="unknown";
  if (abs(a_forwardRollingSampleMedian)>20){accel_status="braking";}
  if (abs(a_sidewayRollingSampleMedian)>abs(a_verticalRollingSampleMedian)||abs(a_forwardRollingSampleMedian)>abs(a_verticalRollingSampleMedian)){accel_status="fallen";}
  if (abs(a_sidewayRollingSampleMedian)<abs(a_verticalRollingSampleMedian)&&abs(a_forwardRollingSampleMedian)<abs(a_verticalRollingSampleMedian)){accel_status="straight";}
  //else {accel_status="unknown";}
  //Serial.println(accel_status);
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

