/**
 * This code is a from a project of an interactive illuminated tree for a festival.
 * The arduino is connected to LEDs stripes on one hand, and to a circuit described in
 * https://www.instructables.com/id/Touche-for-Arduino-Advanced-touch-sensing/
 * on the other hand that allows to mimic the Disney Touché! sensing technology.
 *
 * This circuit was connected to an apple, coming out of the tree, such that
 * depending on the way someone was touching the apple, the LEDs reacted differently.
 * 
 * We used the code from https://github.com/Illutron/AdvancedTouchSensing (from the
 * previously cited instructable) for sensing that we customized (zero curve, averaging, etc.),
 * and code from the examples of the Fast LED library for animating the LEDs.
 *
 * Requires the FastLED library
 *
 * This code is under the WTFPL
 */

#define USE_ARDUINO_INTERRUPTS false

//We use the predefined animations from FastLED
#include <FastLED.h>

#define SET(x,y) (x |=(1<<y))       //-Bit set/clear macros
#define CLR(x,y) (x &= (~(1<<y)))   // |
#define CHK(x,y) (x & (1<<y))       // |
#define TOG(x,y) (x^=(1<<y))        //-+

/* Thresholds separating simple, double touch and full grab
 * /!\ There are highly dependant of the setup, need to be
 * recalibrated in the final situation
 */
#define LIM_TOUCH 60.0
#define LIM_DOUBLE 75.0


#define N 160  //How many frequencies
/*The orignial code sweeps 160 frequencies, but results take too much memory
 *when coupled with the LED code. We thus introduce a divisor such that we
 *only keep 160/DIV points, corresponding to the local average
 *of the original complete curve
 */
#define DIV 6
#define ARRAY_SIZE (N/DIV)
/*In our experience, the points were oscillating quite a bit,
 *so we average them in time over AVG sweeps
 */
#define AVG 20

#define NUM_LEDS 300
#define DATA_PIN 6

#define PULSE_WIRE 1
#define PULSE_THRESHOLD 560

CRGB leds[NUM_LEDS];

//Results of the sweep
float results[ARRAY_SIZE];
//The curve corresponding to a no touch situation, initialized at the begginning (Do not touch when starting the Arduino!)
float zeroCurve[ARRAY_SIZE];

//Counter for averaging over AVG sweeps
unsigned int averageCounter = 1;
//Mode corresponding the the situation: nothing, one finger,two finger or full grab touch
unsigned short int mode = 0;
//Time of the last update
unsigned long lastLEDUpdate = 0;
float LEDSpeed = 1.0f;
CRGBPalette16 currentPalette;
TBlendType    currentBlending;
unsigned long lastModeChange = 0;

void setup()
{
  //When debugging or calibrating
  //Serial.begin(9600);
  FastLED.addLeds<NEOPIXEL, DATA_PIN>(leds, NUM_LEDS);

  TCCR1A = 0b10000010;      //-Set up frequency generator
  TCCR1B = 0b00011001;      //-+
  ICR1 = 110;
  OCR1A = 55;

  pinMode(9, OUTPUT);       //-Signal generator pin
  pinMode(8, OUTPUT);       //-Sync (test) pin

  for (int i = 0; i < ARRAY_SIZE; i++) {
    results[i] = 0;
    zeroCurve[i] = 0;
  }

  delay(1);

  /* The program first computes a zero curve, that corresponds to the state
   * of the circuit when no touch is occurring, that will be then substracted
   * to the measured curve to only keep the variations.
   * Cf the comments of the same for loop in the loop() method for more details
   * on the actions performed here
   */
  for (unsigned int i = 0; i < AVG; i++) {
    for (unsigned int d = 0; d < N; d++)
    {
      int v = analogRead(0);
      CLR(TCCR1B, 0);
      TCNT1 = 0;
      ICR1 = d;
      OCR1A = d / 2;
      SET(TCCR1B, 0);

      zeroCurve[d / DIV] += ((float)v) / ((float)(AVG * DIV));
    }

    TOG(PORTB, 0);
  }
}

void updateLEDSIfNeeded() {
  static uint8_t startIndex = 0;
  unsigned long now = millis();
  if ((now - lastLEDUpdate) > (20.0f / LEDSpeed)) {
    startIndex = startIndex + 1;
    FillLEDsFromPaletteColors(startIndex);
    FastLED.show();
    lastLEDUpdate = now;
  }
}

void setLED(int index, int r, int g, int b, int brightness) {
  leds[index] = CRGB( r, g, b);
  leds[index].fadeLightBy(255 - brightness);
}

// This function sets up a palette of black and white stripes,
// using code.  Since the palette is effectively an array of
// sixteen CRGB colors, the various fill_* functions can be used
// to set them up.
void SetupBlackAndWhiteStripedPalette()
{
    // 'black out' all 16 palette entries...
    fill_solid( currentPalette, 16, CRGB::Black);
    // and set every fourth one to white.
    currentPalette[0] = CRGB::White;
    currentPalette[4] = CRGB::White;
    currentPalette[8] = CRGB::White;
    currentPalette[12] = CRGB::White;
    
}

// This function sets up a palette of purple and green stripes.
void SetupPurpleAndGreenPalette()
{
    CRGB purple = CHSV( HUE_PURPLE, 255, 255);
    CRGB green  = CHSV( HUE_GREEN, 255, 255);
    CRGB black  = CRGB::Black;
    
    currentPalette = CRGBPalette16(
                                   green,  green,  black,  black,
                                   purple, purple, black,  black,
                                   green,  green,  black,  black,
                                   purple, purple, black,  black );
}

// This function fills the palette with totally random colors.
void SetupTotallyRandomPalette()
{
    for( int i = 0; i < 16; i++) {
        currentPalette[i] = CHSV( random8(), 255, random8());
    }
}

void FillLEDsFromPaletteColors( uint8_t colorIndex)
{
  uint8_t brightness = 255;

  for ( int i = 0; i < NUM_LEDS; i++) {
    if (mode == 0) {
      currentPalette = RainbowColors_p;
      currentBlending = LINEARBLEND;
    } else if (mode == 1) {
      currentPalette = RainbowStripeColors_p;
      currentBlending = NOBLEND;
    } else if (mode == 2) {
      SetupBlackAndWhiteStripedPalette();
      currentBlending = NOBLEND;
    } else if (mode == 3) {
      SetupTotallyRandomPalette();
      currentBlending = NOBLEND;

    }
    leds[i] = ColorFromPalette(currentPalette, colorIndex, 255, LINEARBLEND);
    colorIndex += 3;
  }
}

void setRed() {
  for (int i = 0; i < NUM_LEDS; i++) {
    leds[i] = CRGB(255, 0, 0);
  }
  FastLED.show();
}

void setGreen() {
  for (int i = 0; i < NUM_LEDS; i++) {
    leds[i] = CRGB(0, 255, 0);
  }
  FastLED.show();
}

void setBlue() {
  for (int i = 0; i < NUM_LEDS; i++) {
    leds[i] = CRGB(0, 0, 255);
  }
  FastLED.show();
}

void setWhite() {
  for (int i = 0; i < NUM_LEDS; i++) {
    leds[i] = CRGB(255, 255, 255);
  }
  FastLED.show();
}

void loop()
{
  updateLEDSIfNeeded();
  
  /* Code corresponding to the Touché! sensing
   * It sweeps 160 frequencies, generating a square signal
   * for each thanks to the PWM, that is then smoothed by the circuit
   * connected to the Arduino. Finally, we read the response
   */
  
  for (unsigned int d = 0; d < N; d++)
  {
    int v = analogRead(0);  //-Read response signal
    CLR(TCCR1B, 0);         //-Stop generator
    TCNT1 = 0;              //-Reload new frequency
    ICR1 = d;               // |
    OCR1A = d / 2;          //-+
    SET(TCCR1B, 0);         //-Restart generator

    //We average both over DIV (in frequency) and AVG (in time)
    results[d / DIV] += (float)(v) / ((float)(AVG * DIV)); //Filter results
  }

  TOG(PORTB, 0);

  averageCounter++;
  if (averageCounter == AVG) {
     /* This code computes the first 3 maximums and minimums,
      * together with the points where they were reached.
      * In the final setup we actually only use the first maximum
      * to discriminate between different touch
      */
    float maxs[3];
    int argMaxs[3];
    float mins[3];
    int argMins[3];

    for (int i = 0; i < 3; i++) {
      updateLEDSIfNeeded();
      //May need to replace these values with more extreme ones, depending on the setup
      maxs[i] = -100.0;
      argMaxs[i] = 0;
      mins[i] = 100.0;
      argMins[i] = 0;
    }

    for (int i = 0; i < ARRAY_SIZE; i++) {
      updateLEDSIfNeeded();
      //We only keep the differences to the zero curve
      results[i] = (float)(results[i] - zeroCurve[i]);

      if (results[i] < mins[0]) {
        mins[0] = results[i];
        argMins[0] = i;
      }
      else if (results[i] < mins[1]) {
        mins[1] = results[i];
        argMins[1] = i;
      }
      else if (results[i] < mins[2]) {
        mins[2] = results[i];
        argMins[2] = i;
      }

      if (results[i] > maxs[0]) {
        maxs[0] = results[i];
        argMaxs[0] = i;
      }
      else if (results[i] > maxs[1]) {
        maxs[1] = results[i];
        argMaxs[1] = i;
      }
      else if (results[i] > maxs[2]) {
        maxs[2] = results[i];
        argMaxs[2] = i;
      }
    }

      //Debugging or calibrating through serial port
//    Serial.println('M');
//    for(int i=0; i<3; i++) {
//        Serial.print(maxs[i]);
//        Serial.print(' ');
//        Serial.print(argMaxs[i]);
//        Serial.print(' ');
//     }
//
//     Serial.println();
//     Serial.println('m');
//     for(int i=0; i<3; i++) {
//        Serial.print(mins[i]);
//        Serial.print(' ');
//        Serial.print(argMins[i]);
//        Serial.print(' ');
//     }
//
//     Serial.println();
//     Serial.println();

    unsigned long now = millis();
    
    int newMode;
    
    /* newMode corresponds to the new current situation, that is :
     * 0 if there is no touch
     * 1 if there is a one finger touch
     * 2 if there is a two finger touch
     * 3 if there is a full grab
     */
    if (maxs[2] > 0 && maxs[0] < LIM_TOUCH) {
      newMode = 1;
    }
    else if (maxs[0] > LIM_TOUCH && maxs[0] < LIM_DOUBLE) {
      newMode = 2;
    }
    else if(maxs[0] > LIM_DOUBLE) {
      newMode = 3; 
    }
    else {
      newMode = 0;
    }
    
//    Serial.println(now);
//    Serial.println(newMode);
    //If the mode has switched less that 5 seconds ago, we don't switch again right away
    if (newMode != mode && now - lastModeChange > 5000) {
      lastModeChange = now;
      mode = newMode;
    }
//    Serial.println();
//    Serial.print('o');
//    Serial.println(mode);
//
//    Serial.println();
//    Serial.println();

    averageCounter = 1;

    for (int i = 0; i < ARRAY_SIZE; i++)
      results[i] = 0;
  }
}
