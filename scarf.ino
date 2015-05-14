#include <Adafruit_NeoPixel.h>
#include <avr/power.h>
#include <avr/pgmspace.h>

#define PIN 9
#define S_DEFAULT 0
#define S_BLOB_MEET_UP 1
#define S_COLOR_BLINK 2
#define S_COLOR_BLINK_S_RED_UP 3
#define S_COLOR_BLINK_S_RED_DOWN 4
#define S_STROBE 5
#define S_AA_BLOB 6
#define S_SIMPLE_FIRE 7
#define S_SIMPLE_FIRE_S_GENERATE 8
#define S_SIMPLE_FIRE_S_TRANSITION 9

#define SET_PIXEL(x, c) { pixels[x*3] = c & 0xff; pixels[x*3+1] = (c >> 8) & 0xff; pixels[x*3+2] = (c >> 16) & 0xff; }

#define S_MAX_STATE 9
uint16_t STATE = S_SIMPLE_FIRE;

// Parameter 1 = number of pixels in strip
// Parameter 2 = Arduino pin number (most are valid)
// Parameter 3 = pixel type flags, add together as needed:
//   NEO_KHZ800  800 KHz bitstream (most NeoPixel products w/WS2812 LEDs)
//   NEO_KHZ400  400 KHz (classic 'v1' (not v2) FLORA pixels, WS2811 drivers)
//   NEO_GRB     Pixels are wired for GRB bitstream (most NeoPixel products)
//   NEO_RGB     Pixels are wired for RGB bitstream (v1 FLORA pixels, not v2)
Adafruit_NeoPixel strip = Adafruit_NeoPixel(60, PIN, NEO_GRB + NEO_KHZ800);
int numPixels = strip.numPixels();
//int STRIP_SIZE = strip.numPixels();
#define STRIP_SIZE 60

uint8_t *pixels;

// GLOBAL VARIABLES
uint16_t FRAME_NUM = 0, LAST_FRAME = 100, FRAME_DELAY = 0;

uint16_t RAINBOW_STEP = 0, LAST_RAINBOW_STEP = 256, RAINBOW_SIZE = 128, RAINBOW_PERIOD = 17;

uint32_t S_COLOR_BLINK_START_COLOR, S_COLOR_BLINK_END_COLOR;
int S_COLOR_BLINK_CURRENT_STEP = 0, S_COLOR_BLINK_NUM_STEPS = 0, S_COLOR_BLINK_SUB_STATE = S_COLOR_BLINK_S_RED_DOWN, COLOR_BLINK_PERIOD = 3;

uint32_t STROBE_COLOR;
bool STROBE_ON = true;
int STROBE_PERIOD = 7;

int AA_BLOB_POS = 0, AA_BLOB_MAX_POS = 600, AA_BLOB_PERIOD = 100;

int S_SIMPLE_FIRE_SUB_STATE = S_SIMPLE_FIRE_S_GENERATE;
uint32_t SIMPLE_FIRE_CURRENT[STRIP_SIZE], SIMPLE_FIRE_NEW[STRIP_SIZE];
int SIMPLE_FIRE_CURRENT_STEP = 0, SIMPLE_FIRE_MAX_STEP = 0;
int SIMPLE_FIRE_R = 255, SIMPLE_FIRE_G = 150, SIMPLE_FIRE_B = 0, SIMPLE_FIRE_PERIOD = 3;

bool DETECT_MEET_UP = true;

#define NUM_BLOBS 3
int BLOB_POS[NUM_BLOBS] = {0, 20, 40};
int BLOB_SIZE[NUM_BLOBS] = {3, 5, 2}, BLOB_PERIOD[NUM_BLOBS] = {11, 9, 4};
bool IS_BLOB_ON[NUM_BLOBS] = {true, false, false};
bool IS_BLOB_RISING[NUM_BLOBS] = {true, false, false};

int BLOB_LEFT[NUM_BLOBS], BLOB_RIGHT[NUM_BLOBS];

uint8_t NUM_FADING_BLOBS = NUM_BLOBS;
int FADING_BLOB_POS[NUM_BLOBS] = {30};
int FADING_BLOB_SIZE[NUM_BLOBS] = {5}, FADING_BLOB_PERIOD[] = {13};
int FADING_BLOB_SATURATION[NUM_BLOBS] = {0};
int FADING_BLOB_SATURATION_SPEED[NUM_BLOBS] = {0};
bool IS_FADING_BLOB_ON[NUM_BLOBS] = {false};
bool IS_FADING_BLOB_RISING[NUM_BLOBS] = {false};

// IMPORTANT: To reduce NeoPixel burnout risk, add 1000 uF capacitor across
// pixel power leads, add 300 - 500 Ohm resistor on first pixel's data input
// and minimize distance between Arduino and first pixel.  Avoid connecting
// on a live circuit...if you must, connect GND first.

void setup() {
  // This is for Trinket 5V 16MHz, you can remove these three lines if you are not using a Trinket
#if defined (__AVR_ATtiny85__)
  if (F_CPU == 16000000) clock_prescale_set(clock_div_1);
#endif
  // End of trinket special code
  
  Serial.begin(9600);
  pinMode(3, INPUT);
  pinMode(2, INPUT);

  strip.begin();
  strip.setBrightness(255);
  pixels = strip.getPixels();
  strip.show(); // Initialize all pixels to 'off'

  for (uint8_t num_blob = 0; num_blob < NUM_BLOBS; num_blob++) {
    IS_BLOB_ON[num_blob] = false;
    IS_FADING_BLOB_ON[num_blob] = false;
    FADING_BLOB_POS[num_blob] = 0;
    FADING_BLOB_SIZE[num_blob] = 0;
  }
  enableBlob(0, 0, 1, 13, true); //13
  enableBlob(1, 5, 1, 15, true); //15
  enableBlob(2, 11, 1, 17, true); //17

  //initializeExplosionPos(0, NUM_BLOBS);
  Serial.println("INITIS");
  for (int i = 0; i < NUM_BLOBS; i++) {
    Serial.print(i);
    Serial.print(": ");
    Serial.print(BLOB_LEFT[i]);
    Serial.print(" ");
    Serial.println(BLOB_RIGHT[i]);
  }
  
  /*
  for (int i = 0; i < STRIP_SIZE; i++) {
    //strip.setPixelColor(i, transitionValue(strip.Color(0,255,0), strip.Color(255,0,0),i, STRIP_SIZE - 1));
    strip.setPixelColor(i, transitionValue(strip.Color(255,0,0), strip.Color(0,255,0),i, STRIP_SIZE - 1));
    //strip.setPixelColor(i, transitionValue(strip.Color(255,0,0), strip.Color(0,0,0),i, STRIP_SIZE - 1));
  }
  strip.show();
  delay(100000);
  */
  attachInterrupt(1,beat,RISING);
  attachInterrupt(0,toggle,RISING);
}

void toggle() {
  if (STATE == S_DEFAULT) {
    STATE = S_SIMPLE_FIRE;
  } else if (STATE == S_SIMPLE_FIRE) {
    STATE = S_STROBE;
  } else if (STATE == S_STROBE) {
    STATE = S_DEFAULT;
  }
}

volatile unsigned long LAST_BEAT_TIME = 0;
volatile int AVERAGE_BEAT_PERIOD = 0;
volatile int NUM_BEATS = 0;

void buttonPressed() {
  unsigned long thisBeat = millis();
  if (thisBeat - LAST_BEAT_TIME < 300) {
    return;
  }
  digitalWrite(4, 1);
  NUM_BEATS++;
  AVERAGE_BEAT_PERIOD = (AVERAGE_BEAT_PERIOD * max(0, (NUM_BEATS - 2)) + (thisBeat - LAST_BEAT_TIME)) / (NUM_BEATS - 1);
  LAST_BEAT_TIME = thisBeat;
  digitalWrite(4, 0);
}

// amount to adjust beat colour by, 255 is furthest from beat, 0 is closest.
const int MIN_PERIOD = 300; // (milliseconds)
const int MAX_WINDOW_SIZE = 150;

volatile int BEATS[MAX_WINDOW_SIZE];
volatile int NUM_SAMPLES = 0;

volatile bool NEW_BEAT = false;
void beat() {
    if (NUM_SAMPLES < MAX_WINDOW_SIZE) {
      int now = millis();
      if (now - BEATS[NUM_SAMPLES - 1] < MIN_PERIOD) {
        return;
      }
      BEATS[NUM_SAMPLES] = now;
      NUM_SAMPLES++;
      NEW_BEAT = true;
    }
}

const int BEAT_TWEAK = 255;
const int BEAT_SIZE = 80; // milliseconds
float linear(float v) {
    return 1.0 - v;
}

float binary(float v) {
    if (v > 0) {
      return 1.0;
    }
    return 0.0;
}

float sqr(float v) {
    return 1.0 - (v*v);
}

float (*activation)(float) = sqr;

int beatDist(int period, int last, int cur) {
  cur = abs(cur - last) % period;
  int lcur = period - cur;
  if (lcur < cur) {
    return lcur;
  }
  return cur; 
}

int error(int period, int last) {
  int err = 0;
  for (int i = 0; i < NUM_SAMPLES; ++i) {
    int dist = beatDist(period, last, BEATS[i]);
    err += dist * dist;
  }
  return err;
}

int weightedError(int period, int last) {
  int err = 0;
  for (int i = 0; i < NUM_SAMPLES; ++i) {
    int dist = beatDist(period, last, BEATS[i]);
    err += ((BEATS[i] - BEATS[i - 1]) / period) * dist * dist;
  }
  return err;
}

int lastPrint = 0;

int choosePeriod(int basePeriod, int last) {
  int minerr = error(basePeriod, last);
  int minperiod = basePeriod;
  /**
  float fbpm = 60000.0f / float(basePeriod);
  int ibpm = int(fbpm + 0.5f);
  int minbpm = ibpm;
  int minperiod = 60000 / minbpm;
  /**/
  
  for (int period = basePeriod - 150; period <= basePeriod + 150; ++period) {
    if (period == basePeriod) {
      continue;
    }
    
    int err = weightedError(period, last);
    if (err < minerr) {
      minerr = err;
      minperiod = period;
    }
  }
  
  /**
  for (int off = 0; off <= 0; ++off) {
    if (off == 0) {
      continue;
    }
    int bpm = ibpm + off;
    int period = 60000 / bpm;
    int err = error(period, last);
    if (err < minerr) {
      minperiod = period;
      minerr = err;
      minbpm = bpm;
    }
  }
  /**/
  if (lastPrint != NUM_SAMPLES) {
      Serial.print("samples:");
      Serial.print(NUM_SAMPLES);
      //Serial.print(" bpm:");
      //Serial.print(minbpm);
      Serial.print('\n');
      lastPrint = NUM_SAMPLES;
  }
  return minperiod;
}

int smallestPeriod() {
  if (NUM_SAMPLES < 2) {
    return 0;
  }
  int currentBeat = BEATS[0];
  int smallestPeriod = BEATS[1] - BEATS[0];
  
  for(int i = 2; i < NUM_SAMPLES; ++i) {
    smallestPeriod = min(smallestPeriod, BEATS[i] - BEATS[i-1]);
  }
  
  return smallestPeriod;
}

int stretchPeriod(int period) {
  int err = (BEATS[NUM_SAMPLES - 1] - BEATS[0]) % period;
  return period + err / (BEATS[NUM_SAMPLES - 1] - BEATS[0]);
}

int LAST_PERIOD = 0;
int beatTweak() {
    if (NUM_SAMPLES < 2) {
      return 0;
    }
    
    int last = BEATS[NUM_SAMPLES - 1];
    if (NEW_BEAT) {
      NEW_BEAT = false;
      // LAST_PERIOD = choosePeriod((last - BEATS[0]) / (NUM_SAMPLES - 1), last);
      LAST_PERIOD = choosePeriod(smallestPeriod(), last);
      // LAST_PERIOD = stretchPeriod(smallestPeriod());
      // LAST_PERIOD = (last - BEATS[0]) / (NUM_SAMPLES - 1);
    }
    
    int dist = beatDist(LAST_PERIOD, last, millis());
    
    // too far out, who cares.
    if (dist > BEAT_SIZE) {
      return 0;
    }
    float ft = activation(float(dist) / float(BEAT_SIZE)) * float(BEAT_TWEAK);
    int tweak = int(ft);
    // clamp the result to 0-BEAT_TWEAK
    if (tweak < 0) {
      return 0;
    }
    if (tweak > BEAT_TWEAK) {
      return BEAT_TWEAK;
    }
    return tweak;
}

void initializeExplosionPos(int left, int right) {
  if (left + 1 == right) {
    return;
  }
  int center = (left + right) / 2;
  BLOB_LEFT[center] = (left + center) / 2;
  BLOB_RIGHT[center] = (center + right) / 2;
  initializeExplosionPos(left, center);
  initializeExplosionPos(center, right);
}

void enableBlob(int n, int pos, int blob_size, int period, bool is_rising) {
  IS_BLOB_ON[n] = true;
  BLOB_POS[n] = pos;
  BLOB_SIZE[n] = blob_size;
  BLOB_PERIOD[n] = period;
  IS_BLOB_RISING[n] = is_rising;
}

uint16_t j = 0, blob = 0;

int state = 0;
void loop1() {
  int nstate = digitalRead(4) == HIGH;
  if (state != nstate) {
    if (nstate) {
      //colorWipe(strip.Color(255, 0, 0), 0);
      strip.setPixelColor(30, 255, 0, 0);
      strip.show();
    } else {
      //colorWipe(strip.Color(0, 255, 0), 0);
      strip.setPixelColor(30, 255, 255, 0);
      strip.show();
    }
    state = nstate;
  }
}

void loop() {  
  // RESET
  //colorFill(strip.Color(0, 0, 0));
  // END RESET

  if (STATE == S_DEFAULT) {  
    // RAINBOW
    /**/
    for(uint16_t i=0; i < STRIP_SIZE; i++) {
      strip.setPixelColor(i, Wheel( ( (i * RAINBOW_SIZE / STRIP_SIZE) + RAINBOW_STEP) & 255 ) );
    }
    if (FRAME_NUM % RAINBOW_PERIOD == 0) {
      if (++RAINBOW_STEP == LAST_RAINBOW_STEP) {
        RAINBOW_STEP = 0;
      }
    }
    /**/
    // END RAINBOW
    
    // BLOBS
    /**/
    for (uint16_t num_blob = 0; num_blob < NUM_BLOBS; num_blob++) {
      if (IS_BLOB_ON[num_blob]) {
        uint8_t blob_saturation = 255;
        for (uint16_t i = 0; i < BLOB_SIZE[num_blob]; i++) {
          if (IS_BLOB_RISING[num_blob]) {
            strip.setPixelColor((BLOB_POS[num_blob] - i + STRIP_SIZE) % STRIP_SIZE, blob_saturation, blob_saturation, blob_saturation);
          } else {
            strip.setPixelColor((BLOB_POS[num_blob] + i + STRIP_SIZE) % STRIP_SIZE, blob_saturation, blob_saturation, blob_saturation);
          }
          blob_saturation = 255 * (BLOB_SIZE[num_blob] - i - 1) / BLOB_SIZE[num_blob];
        }
      
        if (FRAME_NUM % BLOB_PERIOD[num_blob] == 0) {
          /**/
          if (BLOB_POS[num_blob] == STRIP_SIZE / 2 & FADING_BLOB_SATURATION[num_blob] == 0) {
            IS_FADING_BLOB_ON[num_blob] = true;
            FADING_BLOB_SATURATION[num_blob] = 255;
            FADING_BLOB_SATURATION_SPEED[num_blob] = 9;
            FADING_BLOB_POS[num_blob] = BLOB_POS[num_blob];
            FADING_BLOB_SIZE[num_blob] = BLOB_SIZE[num_blob];
            FADING_BLOB_PERIOD[num_blob] = BLOB_PERIOD[num_blob] * 0.3;
            /*
            if (IS_BLOB_RISING[num_blob]) {
              IS_BLOB_RISING[num_blob] = false;
            } else {
              IS_BLOB_RISING[num_blob] = true;
            }
            */
            IS_FADING_BLOB_RISING[num_blob] = !IS_BLOB_RISING[num_blob];
          }
          /**/
          /*
          if (BLOB_POS[num_blob] == num_blob) {
            int left = BLOB_LEFT[num_blob];
            IS_BLOB_ON[left] = true;
            BLOB_POS[left] = BLOB_POS[num_blob];
            BLOB_SIZE[left] = 1;
            BLOB_PERIOD[left] = BLOB_PERIOD[num_blob];
            IS_BLOB_RISING[left] = true;
            
            int right = BLOB_RIGHT[num_blob];
            IS_BLOB_ON[right] = true;
            BLOB_POS[right] = BLOB_POS[num_blob];
            BLOB_SIZE[right] = 1;
            BLOB_PERIOD[right] = BLOB_PERIOD[num_blob];
            IS_BLOB_RISING[right] = false;
            
            IS_BLOB_ON[num_blob] = false;
          }
          */
          if (IS_BLOB_RISING[num_blob]) {
            BLOB_POS[num_blob]++;
          } else {
            BLOB_POS[num_blob]--;
          }
          
          if (BLOB_POS[num_blob] == STRIP_SIZE) {
            BLOB_POS[num_blob] = 0;
          }
          if (BLOB_POS[num_blob] == -1) {
            BLOB_POS[num_blob] = STRIP_SIZE - 1;
          }
        }
      }
    }
    /**/
    // END BLOBS
    
    // STATE TRANSITION TO BLOB MEETUP
    if (DETECT_MEET_UP && (abs(BLOB_POS[0]-BLOB_POS[1]) + abs(BLOB_POS[0]-BLOB_POS[2]) + abs(BLOB_POS[1]-BLOB_POS[2]) == 0)) {
      DETECT_MEET_UP = false;
      STATE = S_BLOB_MEET_UP;
      STATE = S_COLOR_BLINK;
    }
    if (abs(BLOB_POS[0]-BLOB_POS[1]) + abs(BLOB_POS[0]-BLOB_POS[2]) + abs(BLOB_POS[1]-BLOB_POS[2]) > 2) {
      DETECT_MEET_UP = true;
    }
    
    // FADING BLOB
    /**/
    for (uint16_t num_blob = 0; num_blob < NUM_FADING_BLOBS; num_blob++) {
      /*
      Serial.print("num_blobs: ");
      Serial.println(num_blob);
      Serial.println(IS_FADING_BLOB_ON[num_blob]);
      Serial.println(FADING_BLOB_SIZE[num_blob]);
      Serial.println(FADING_BLOB_POS[num_blob]);
      Serial.println();
      */
      
      if (IS_FADING_BLOB_ON[num_blob]) {
        uint8_t f_blob_saturation = FADING_BLOB_SATURATION[num_blob];
        for (uint16_t i = 0; i < FADING_BLOB_SIZE[num_blob]; i++) {
          if (IS_FADING_BLOB_RISING[num_blob]) {
            strip.setPixelColor((FADING_BLOB_POS[num_blob] - i + STRIP_SIZE) % STRIP_SIZE, transitionValue(strip.getPixelColor((FADING_BLOB_POS[num_blob] + i + STRIP_SIZE) % STRIP_SIZE), strip.Color(255,255,255), FADING_BLOB_SATURATION[num_blob], 255));
            //strip.setPixelColor((0 + i + STRIP_SIZE) % STRIP_SIZE, 255, 0, 0);
          } else {
            strip.setPixelColor((FADING_BLOB_POS[num_blob] + i + STRIP_SIZE) % STRIP_SIZE, transitionValue(strip.getPixelColor((FADING_BLOB_POS[num_blob] + i + STRIP_SIZE) % STRIP_SIZE), strip.Color(255,255,255), FADING_BLOB_SATURATION[num_blob], 255));
          }
          f_blob_saturation = FADING_BLOB_SATURATION[num_blob] * (FADING_BLOB_SIZE[num_blob] - i - 1) / FADING_BLOB_SIZE[num_blob];
        }
        
        if (FRAME_NUM % FADING_BLOB_PERIOD[num_blob] == 0) {
          //FADING_BLOB_PERIOD[num_blob] = FADING_BLOB_PERIOD[num_blob] + 2;
          if (FADING_BLOB_POS[num_blob] == ((256 / STRIP_SIZE + RAINBOW_STEP) & 255) % STRIP_SIZE + 1000) {
            if (IS_FADING_BLOB_RISING[num_blob]) {
              IS_FADING_BLOB_RISING[num_blob] = false;
            } else {
              IS_FADING_BLOB_RISING[num_blob] = true;
            }
          }
          
          if (IS_FADING_BLOB_RISING[num_blob]) {
            FADING_BLOB_POS[num_blob]++;
          } else {
            FADING_BLOB_POS[num_blob]--;
          }
          
          if (FADING_BLOB_POS[num_blob] == STRIP_SIZE) {
            FADING_BLOB_POS[num_blob] = 0;
          }
          if (FADING_BLOB_POS[num_blob] == -1) {
            FADING_BLOB_POS[num_blob] = STRIP_SIZE - 1;
          }
          
          FADING_BLOB_SATURATION[num_blob] = FADING_BLOB_SATURATION[num_blob] - FADING_BLOB_SATURATION_SPEED[num_blob];
          if (FADING_BLOB_SATURATION[num_blob] < 1) {
            FADING_BLOB_SATURATION[num_blob] = 0;
            IS_FADING_BLOB_ON[num_blob] = false;
          }
        }
      }
    }
    // END FADING BLOB
    /**/
  } // END STATE == S_DEFAULT
  
  if (STATE == S_COLOR_BLINK) {
    if (S_COLOR_BLINK_CURRENT_STEP == S_COLOR_BLINK_NUM_STEPS) {
      
      if (S_COLOR_BLINK_SUB_STATE == S_COLOR_BLINK_S_RED_UP) {
        S_COLOR_BLINK_SUB_STATE = S_COLOR_BLINK_S_RED_DOWN;
        S_COLOR_BLINK_START_COLOR = strip.Color(255, 0, 0);
        S_COLOR_BLINK_END_COLOR = strip.Color(0, 0, 0);
        S_COLOR_BLINK_CURRENT_STEP = 0;
        S_COLOR_BLINK_NUM_STEPS = 200;
      } else if (S_COLOR_BLINK_SUB_STATE == S_COLOR_BLINK_S_RED_DOWN) {
        STATE = S_DEFAULT;
        S_COLOR_BLINK_SUB_STATE = S_COLOR_BLINK_S_RED_UP;
        S_COLOR_BLINK_START_COLOR = strip.Color(0, 0, 0);
        S_COLOR_BLINK_END_COLOR = strip.Color(255, 0, 0);
        S_COLOR_BLINK_CURRENT_STEP = 0;
        S_COLOR_BLINK_NUM_STEPS = 100;
      }
    }
    
    colorFill(transitionValue(S_COLOR_BLINK_START_COLOR, S_COLOR_BLINK_END_COLOR, S_COLOR_BLINK_CURRENT_STEP, S_COLOR_BLINK_NUM_STEPS));
    if (FRAME_NUM % COLOR_BLINK_PERIOD == 0) {
      S_COLOR_BLINK_CURRENT_STEP++;
    }
  }
  
  if (STATE == S_STROBE) {
    if (FRAME_NUM % STROBE_PERIOD == 0) {
      if (STROBE_ON) {
        STROBE_COLOR = strip.Color(0, 0, 0);
        STROBE_ON = false;
      } else {
        STROBE_COLOR = strip.Color(255, 255, 255);
        STROBE_ON = true;
      }
    }
    
    colorFill(STROBE_COLOR);
  }
  
  if (STATE == S_SIMPLE_FIRE) {
    if (S_SIMPLE_FIRE_SUB_STATE == S_SIMPLE_FIRE_S_GENERATE) {
      for(int i = 0; i < STRIP_SIZE; i++) {
        int flicker = random(0, 150);
        int r = max(SIMPLE_FIRE_R - flicker, 0);
        int g = max(SIMPLE_FIRE_G - flicker, 0);
        int b = max(SIMPLE_FIRE_B - flicker, 0);
        SIMPLE_FIRE_NEW[i] = strip.Color(r, g, b);
      }
      SIMPLE_FIRE_CURRENT_STEP = 0;
      SIMPLE_FIRE_MAX_STEP = random(5, 15);
      S_SIMPLE_FIRE_SUB_STATE = S_SIMPLE_FIRE_S_TRANSITION;
    }
    
    if (S_SIMPLE_FIRE_SUB_STATE == S_SIMPLE_FIRE_S_TRANSITION) {
      for(int i = 0; i < STRIP_SIZE; i++) {
        strip.setPixelColor(i, transitionValue(SIMPLE_FIRE_CURRENT[i], SIMPLE_FIRE_NEW[i], SIMPLE_FIRE_CURRENT_STEP, SIMPLE_FIRE_MAX_STEP));
      }
      if (SIMPLE_FIRE_CURRENT_STEP == SIMPLE_FIRE_MAX_STEP) {
        S_SIMPLE_FIRE_SUB_STATE = S_SIMPLE_FIRE_S_GENERATE;
        for (int i = 0; i < STRIP_SIZE; i++) {
          SIMPLE_FIRE_CURRENT[i] = SIMPLE_FIRE_NEW[i];
        }
      }
    }
    
    if (FRAME_NUM % SIMPLE_FIRE_PERIOD == 0) {
      SIMPLE_FIRE_CURRENT_STEP++;
    }
  }
  
  if (STATE == S_AA_BLOB) {
    if (AA_BLOB_POS == AA_BLOB_MAX_POS) {
      AA_BLOB_POS = 0;
    }
    
    int res = (AA_BLOB_MAX_POS / STRIP_SIZE);
    int left = AA_BLOB_POS / res;
    int rest = AA_BLOB_POS % res;
    
    
    strip.setPixelColor((left - 1 + STRIP_SIZE) % STRIP_SIZE, transitionValue(strip.Color(0, 0, 0), strip.Color(255, 255, 255), rest, res));
    strip.setPixelColor((left) % STRIP_SIZE, transitionValue(strip.Color(0, 0, 0), strip.Color(255, 255, 255), rest, res));
    strip.setPixelColor((left + 1) % STRIP_SIZE, transitionValue(strip.Color(255, 255, 255), strip.Color(0, 0, 0), (res - rest), res));
    if (FRAME_NUM % AA_BLOB_PERIOD == 0) {
      AA_BLOB_POS++;
    }
  }
  
  if (STATE == S_BLOB_MEET_UP) {
    //meetUpAnimation(BLOB_POS[0]);
    uint32_t initState[STRIP_SIZE];
    int start = BLOB_POS[0];
    for (int i = 0; i < STRIP_SIZE; i++) {
      initState[i] = strip.getPixelColor(i);
    }
    
    colorWipe2(start, strip.Color(100, 100, 100), 30);
    
    for (int i = 0; i < 200; i++) {
      fillStrip(transitionValue(strip.Color(100, 100, 100), strip.Color(0, 0, 0), i, 199));
      delay(10);
    }
    
    for (int i = 0; i < 70; i++) {
      fillStrip(transitionValue(strip.Color(0, 0, 0), strip.Color(255, 0, 0), i, 69));
      delay(10);
    }
    
    for (int i = 0; i < 200; i++) {
      fillStrip(transitionValue(strip.Color(255, 0, 0), strip.Color(0, 0, 0), i, 199));
      delay(10);
    }
    
    for (int i = 0; i < 70; i++) {
      fillStrip(transitionValue(strip.Color(0, 0, 0), strip.Color(0, 255, 0), i, 69));
      delay(10);
    }
    
    for (int i = 0; i < 200; i++) {
      fillStrip(transitionValue(strip.Color(0, 255, 0), strip.Color(0, 0, 0), i, 199));
      delay(10);
    }
    
    for (int i = 0; i < 70; i++) {
      for (int p = 0; p < STRIP_SIZE; p++) {
        strip.setPixelColor(p, transitionValue(strip.Color(0, 0, 0), initState[p], i, 69));
      }
      strip.show();
      delay(10);
    }
    STATE = S_DEFAULT;
    /*
    if (meetUp == true) {
      if ((abs(BLOB_POS[0]-BLOB_POS[1]) + abs(BLOB_POS[0]-BLOB_POS[2]) + abs(BLOB_POS[1]-BLOB_POS[2]) == 0)) {
        meetUpAnimation(BLOB_POS[0]);
        meetUp = false;
      }
    }
    if ((abs(BLOB_POS[0]-BLOB_POS[1]) + abs(BLOB_POS[0]-BLOB_POS[2]) + abs(BLOB_POS[1]-BLOB_POS[2]) > 2)) {
      meetUp = true;
    }
    */
  } // END STATE == S_BLOB_MEET_UP
  
  if (digitalRead(3) == HIGH) {
    //colorFill(strip.Color(255, 255, 255));
  }
  
  // beatMod is a modifier 0-255 to apply to each pixel.
  int beatMod = 255 - beatTweak();
  for (int p = 0; p < STRIP_SIZE*3; p++) {
    //pixels[p] = abs(beatMod - pixels[p]);
    pixels[p] = pixels[p]*beatMod/255;
  }
  
/*  if ((millis() - LAST_BEAT_TIME) % AVERAGE_BEAT_PERIOD < 10) {
  }
  
  if (millis() - LAST_BEAT_TIME > 8*AVERAGE_BEAT_PERIOD) {
    NO_BEATS = 0;
  }*/
  
  strip.show();
  // delay(FRAME_DELAY);
  if (++FRAME_NUM == LAST_FRAME) {
    FRAME_NUM = 0;
  }
  /**/
}

void setAAPixelColor(int pos, int max_pos, uint32_t color, int width) {
  
}

int foo(int pos, int max_d) {
  
}

void colorFill(uint32_t c) {
  for(uint16_t i=0; i < STRIP_SIZE; i++) {
      strip.setPixelColor(i, c);
  }
}

// Fill the dots one after the other with a color
void colorWipe(uint32_t c, uint8_t wait) {
  for(uint16_t i=0; i<strip.numPixels(); i++) {
      strip.setPixelColor(i, c);
      strip.show();
      delay(wait);
  }
}

// Fill the dots one after the other with a color
void colorWipe2(int start, uint32_t c, uint8_t wait) {
  for(int i=0; i<STRIP_SIZE / 2 + 1; i++) {
      strip.setPixelColor((start + i + STRIP_SIZE) % STRIP_SIZE, c);
      strip.setPixelColor((start - i + STRIP_SIZE) % STRIP_SIZE, c);
      strip.show();
      delay(wait);
  }
}

void meetUpAnimation(int start) {
  uint32_t initState[STRIP_SIZE];
  for (int i = 0; i < STRIP_SIZE; i++) {
    initState[i] = strip.getPixelColor(i);
  }
  
  colorWipe2(start, strip.Color(100, 100, 100), 30);
  
  for (int i = 0; i < 200; i++) {
    fillStrip(transitionValue(strip.Color(100, 100, 100), strip.Color(0, 0, 0), i, 199));
    delay(10);
  }
  
  for (int i = 0; i < 70; i++) {
    fillStrip(transitionValue(strip.Color(0, 0, 0), strip.Color(255, 0, 0), i, 69));
    delay(10);
  }
  
  for (int i = 0; i < 200; i++) {
    fillStrip(transitionValue(strip.Color(255, 0, 0), strip.Color(0, 0, 0), i, 199));
    delay(10);
  }
  
  for (int i = 0; i < 70; i++) {
    fillStrip(transitionValue(strip.Color(0, 0, 0), strip.Color(0, 255, 0), i, 69));
    delay(10);
  }
  
  for (int i = 0; i < 200; i++) {
    fillStrip(transitionValue(strip.Color(0, 255, 0), strip.Color(0, 0, 0), i, 199));
    delay(10);
  }
  
  for (int i = 0; i < 70; i++) {
    for (int p = 0; p < STRIP_SIZE; p++) {
      strip.setPixelColor(p, transitionValue(strip.Color(0, 0, 0), initState[p], i, 69));
    }
    strip.show();
    delay(10);
  }
  
  /*
  for (int i = 0; i < STRIP_SIZE / 2 + 1; i++) {
    strip.setPixelColor((start + i + STRIP_SIZE) % STRIP_SIZE, initState[i]);
    strip.setPixelColor((start - i + STRIP_SIZE) % STRIP_SIZE, initState[i]);
    strip.show();
    delay(30);
  }
  */
}

void fillStrip(uint32_t c) {
  for (int i = 0; i < STRIP_SIZE; i++) {
    strip.setPixelColor(i, c);
  }
  strip.show();
}

void rainbow(uint8_t wait) {
  uint16_t i, j;

  for(j=0; j<256; j++) {
    for(i=0; i<strip.numPixels(); i++) {
      strip.setPixelColor(i, Wheel((i+j) & 255));
    }
    strip.show();
    delay(wait);
  }
}

// Slightly different, this makes the rainbow equally distributed throughout
void rainbowCycle(uint8_t wait) {
  uint16_t i, j;

  for(j=0; j<256; j++) { // 5 cycles of all colors on wheel
    for(i=0; i< strip.numPixels(); i++) {
      strip.setPixelColor(i, Wheel(((i * 256 / strip.numPixels()) + j) & 255));
    }
    strip.show();
    delay(wait);
  }
}

void rainbowCycleAnd(uint8_t wait) {
  uint16_t i, j, blob;
  
  blob = 0;

  for(j=0; j<256; j++) {
    for(i=0; i< strip.numPixels(); i++) {
      strip.setPixelColor(i, Wheel(((i * 256 / strip.numPixels()) + j) & 255));
    }
    strip.setPixelColor(blob, 255, 255, 255);
    strip.setPixelColor(blob+1, 255, 255, 255);
    strip.setPixelColor(blob+2, 255, 255, 255);
    strip.setPixelColor(blob+3, 255, 255, 255);
    if (j % 3 == 0) {
      blob++; if (blob == 57) { blob = 0;}
    }
    strip.show();
    delay(wait);
  }
}

//Theatre-style crawling lights.
void theaterChase(uint32_t c, uint8_t wait) {
  for (int j=0; j<10; j++) {  //do 10 cycles of chasing
    for (int q=0; q < 3; q++) {
      for (int i=0; i < strip.numPixels(); i=i+3) {
        strip.setPixelColor(i+q, c);    //turn every third pixel on
      }
      strip.show();
     
      delay(wait);
     
      for (int i=0; i < strip.numPixels(); i=i+3) {
        strip.setPixelColor(i+q, 0);        //turn every third pixel off
      }
    }
  }
}

//Theatre-style crawling lights with rainbow effect
void theaterChaseRainbow(uint8_t wait) {
  for (int j=0; j < 256; j++) {     // cycle all 256 colors in the wheel
    for (int q=0; q < 3; q++) {
        for (int i=0; i < strip.numPixels(); i=i+3) {
          strip.setPixelColor(i+q, Wheel( (i+j) % 255));    //turn every third pixel on
        }
        strip.show();
       
        delay(wait);
       
        for (int i=0; i < strip.numPixels(); i=i+3) {
          strip.setPixelColor(i+q, 0);        //turn every third pixel off
        }
    }
  }
}

// Input a value 0 to 255 to get a color value.
// The colours are a transition r - g - b - back to r.
uint32_t Wheel(byte WheelPos) {
  WheelPos = 255 - WheelPos;
  if(WheelPos < 85) {
   return strip.Color(255 - WheelPos * 3, 0, WheelPos * 3);
  } else if(WheelPos < 170) {
    WheelPos -= 85;
   return strip.Color(0, WheelPos * 3, 255 - WheelPos * 3);
  } else {
   WheelPos -= 170;
   return strip.Color(WheelPos * 3, 255 - WheelPos * 3, 0);
  }
}

/***** SHOOTING COLORS     *****/
void shootingColors() {
  /* General constants     */
  uint8_t pixelValues[numPixels*3];
  memset(pixelValues, 0, sizeof(pixelValues));
  uint32_t black = strip.Color(0,0,0);
  uint32_t newColor;
  
  // Initialized per mode; brightest is 90, 50, numPixels / 2, 100.
  int baseValue = 90, shotDenominator, maxShotLength, maxWaitTime;
  const int maxShots = 4; // man do I not want to have to deal with malloc
  
  int startPixel[maxShots], endPixel[maxShots], currentStep[maxShots],
    colorIndex[maxShots], waitTime[maxShots];
  boolean forwards[maxShots];
  memset(startPixel, -1, sizeof(startPixel));
  memset(endPixel, -1, sizeof(endPixel));
  memset(currentStep, -1, sizeof(currentStep));
  memset(colorIndex, -1, sizeof(colorIndex));
  memset(waitTime, -1, sizeof(waitTime));
  memset(forwards, true, sizeof(forwards));
  /* End general constants */
  
  // Transition up to baseValue white from current color
  int transitionSteps = 100, wait = 2;
  uint32_t currentValues[numPixels];
  memset(currentValues, 0, sizeof(currentValues));
  for (int p=0; p<numPixels; ++p) currentValues[p] = strip.getPixelColor(p);
  uint32_t targetColor = strip.Color(baseValue, baseValue, baseValue);
  for (int i = 0; i < transitionSteps; ++i) {
    for (int p = 0; p < numPixels; ++p) {
      newColor = transitionValue(currentValues[p], targetColor, i,
        transitionSteps);
      strip.setPixelColor(p, newColor);
    }
    strip.show();
    delay(wait);
  }
  // End  transition up to baseValue white from current color
  
  wait = 15*wait; // between updates
  
  /* Animation constants     */
  // int RED = 0, ORANGE = 1, YELLOW = 2, GREEN = 3, BLUE = 4, INDIGO = 5,
  //   VIOLET = 6;
  const int NUM_COLORS = 7;
  
  uint32_t constantColors[NUM_COLORS] = {strip.Color(127,0,0),
    strip.Color(127,63,0), strip.Color(127,127,0), strip.Color(0,127,0),
    strip.Color(0,0,127), strip.Color(37,0,65), strip.Color(71,0,127)};
  /* End animation constants */
  
  /* Mode definitions     */
  int BRIGHT_ACTIVE = 0, MEDIUM_ACTIVE = 1, LOW_ACTIVE = 2, LOW_SPARSE = 3;
  const int NUM_MODES = 4;

  /* For having it die out gradually */
  int MODES[NUM_MODES] = {BRIGHT_ACTIVE, MEDIUM_ACTIVE, LOW_ACTIVE,
    LOW_SPARSE};
  /* For having it go up in intensity */
  //int MODES[NUM_MODES] = {LOW_SPARSE, LOW_ACTIVE, MEDIUM_ACTIVE,
   //BRIGHT_ACTIVE};
  /* End mode definitions */
  
  /* Mode selection and tracking     */
  int mode;
  unsigned long lastTime = millis();
  long remainingTime = 60000; // one hour
  
  // CHANGE TOGETHER - duration ratios for modes
  int modeRatios[NUM_MODES] = {2, 3, 2, 1};
  int totalModeRatios = 8;
  // END CHANGE TOGETHER
  
  unsigned long modeUnitTime = remainingTime / totalModeRatios;
  /* End mode selection and tracking */
  
  /*** MAIN LOOP     ***/
  while (remainingTime > 0) {
    // Timekeeping...
    unsigned long currentTime = millis();
    remainingTime -= (currentTime - lastTime);
    lastTime = currentTime;
    // ...to determine the right mode to be in
    unsigned long timeTally = 0;
    for (int m = NUM_MODES - 1; m >= 0; --m) {
      timeTally += modeUnitTime * modeRatios[m];
      if (timeTally > remainingTime) {
        mode = MODES[m];
        break;
      }
    }
    
    if (mode == BRIGHT_ACTIVE) {
      baseValue = 90;
      shotDenominator = 20;
      maxShotLength = numPixels / 2;
      maxWaitTime = 100;
    }
    if (mode == MEDIUM_ACTIVE) {
      baseValue = gamma(2*90 + random(1));
      shotDenominator = 20;
      maxShotLength = numPixels / 4;
      maxWaitTime = 100;
    }
    if (mode == LOW_ACTIVE || mode == LOW_SPARSE) {
      baseValue = gamma(90);
      shotDenominator = 40;
      maxShotLength = numPixels / 8;
      maxWaitTime = 100;
    }
    
    memset(pixelValues, 0, sizeof(pixelValues));
    
    for (int s = 0; s < maxShots; ++s) {
      if (startPixel[s] == -1) {
        if (random(shotDenominator) != 0) continue;
        
        // Generate a new shot!
        startPixel[s] = random(numPixels);
        endPixel[s] = startPixel[s] + random(maxShotLength);
        if (endPixel[s] >= numPixels) endPixel[s] %= numPixels;
        currentStep[s] = 1;
        if (mode != LOW_SPARSE)
          colorIndex[s] = random(NUM_COLORS + NUM_COLORS / 2);
        else
          colorIndex[s] = random(NUM_COLORS);
        waitTime[s] = random(maxWaitTime);
        if (random(2) == 0) forwards[s] = false;
      }

      /* Calculate shot contribution     */
      int shotLength = endPixel[s] - startPixel[s] + 1;
      if (shotLength < 0) shotLength += numPixels;
      for (int p = 0; p < shotLength; ++p) {
        int thisStep = currentStep[s];
        int currPixel = startPixel[s] + (forwards[s] ? 1 : -1) * p;
        if (currPixel >= numPixels) currPixel -= numPixels;
        // Pixel is not yet reached or already passed
        if (thisStep <= p || thisStep > shotLength + p) continue;
        
        // Not rainbow
        if (colorIndex[s] < NUM_COLORS) {
          newColor = constantColors[colorIndex[s]];
        }
        // Rainbow
        else {
          if (p >= thisStep - NUM_COLORS && p < thisStep)
            newColor = constantColors[NUM_COLORS - (thisStep - p)];
        }
        
        uint8_t newR = RGB_r(newColor), newG = RGB_g(newColor),
          newB = RGB_b(newColor);
        
        // Add color to existing shot contributions
        pixelValues[3*currPixel] += (int)newR,
          pixelValues[3*currPixel+1] += (int)newG,
          pixelValues[3*currPixel+2] += (int)newB;
     
        // Normalize so 127 is max
        if (pixelValues[3*currPixel] > 127) {
          double ratio = (double)pixelValues[3*currPixel  ] / 127.0;
          pixelValues[3*currPixel  ] = 127;
          pixelValues[3*currPixel+1]=(int)(pixelValues[3*currPixel+1]*ratio);
          pixelValues[3*currPixel+2]=(int)(pixelValues[3*currPixel+2]*ratio);
        }
        if (pixelValues[3*currPixel+1] > 127) {
          double ratio = (double)pixelValues[3*currPixel+1] / 127.0;
          pixelValues[3*currPixel  ]=(int)(pixelValues[3*currPixel  ]*ratio);
          pixelValues[3*currPixel+1] = 127;
          pixelValues[3*currPixel+2]=(int)(pixelValues[3*currPixel+2]*ratio);
        }
        if (pixelValues[3*currPixel+2] > 127) {
          double ratio = (double)pixelValues[3*currPixel+2] / 127.0;
          pixelValues[3*currPixel  ] = (int)(pixelValues[3*currPixel  ]*ratio);
          pixelValues[3*currPixel+1] = (int)(pixelValues[3*currPixel+1]*ratio);
          pixelValues[3*currPixel+2] = 127;
        }
        // End normalize
      }
      /* End calculate shot contribution */
       
      currentStep[s] += 1;
      if (currentStep[s] == 2*shotLength) {
        startPixel[s] = -1;
        endPixel[s] = -1;
        currentStep[s] = -1;
        colorIndex[s] = -1;
        waitTime[s] = -1;
        forwards[s] = true;
      }
    }
    
    for (int p = 0; p < numPixels; ++p) {
      int pixelR = pixelValues[3*p], pixelG = pixelValues[3*p+1],
        pixelB = pixelValues[3*p+2];   
      
      if (pixelR == 0 && pixelG == 0 && pixelB == 0)
        strip.setPixelColor(p, strip.Color(baseValue, baseValue, baseValue));
      else {
        if (mode == MEDIUM_ACTIVE) {
          pixelR = gamma(2*pixelR + random(1)),
            pixelG = gamma(2*pixelG + random(1)),
            pixelB = gamma(2*pixelB + random(1));
        }
        else if (mode == LOW_ACTIVE || mode == LOW_SPARSE) {
          pixelR = gamma(pixelR), pixelG = gamma(pixelG),
            pixelB = gamma(pixelB);
        }
        strip.setPixelColor(p, strip.Color(pixelR, pixelG, pixelB));
      }
    }
    strip.show();
    delay(wait);
  }
  /*** END MAIN LOOP ***/
} 
/***** END SHOOTING COLORS *****/

/***** FIRE MELD     *****/
void fireMeld() {
  /* General constants     */
  uint32_t black = strip.Color(0,0,0);
  uint32_t newColor;
  int transitionSteps = 100, wait = 2; // between transitions
  int frontIdx = 0; // which image is front image
  uint8_t stripColors[2][numPixels*3];
  memset(stripColors, 0, sizeof(stripColors));
  /* End general constants */
  
  /* Mode definitions     */
  int BRIGHT_NO_FLICKER = 0, MEDIUM_NO_FLICKER = 1, LOW_NO_FLICKER = 2,
    LOW_FLICKER = 3, SPARSE_FLICKER = 4, EMBERS = 5;
  const int NUM_MODES = 6;

  /* For having it die gradually */
  //int MODES[NUM_MODES] = {BRIGHT_NO_FLICKER, MEDIUM_NO_FLICKER, LOW_NO_FLICKER,
    //LOW_FLICKER, SPARSE_FLICKER, EMBERS};
  /* For having it go up in intensity */
  int MODES[NUM_MODES] = {EMBERS, SPARSE_FLICKER, LOW_FLICKER, LOW_NO_FLICKER,
    MEDIUM_NO_FLICKER, BRIGHT_NO_FLICKER};
    /* End mode definitions */
  
  // Initialize front image to current image for a gentle transition from
  //  initial starting color
  for (int p = 0; p < numPixels; ++p) {
    uint32_t currColor = strip.getPixelColor(p);
    stripColors[frontIdx][p*3  ] = RGB_r(currColor);
    stripColors[frontIdx][p*3+1] = RGB_g(currColor);
    stripColors[frontIdx][p*3+2] = RGB_b(currColor);
  }
  
  /* Flicker constants     */
  int shouldFlicker = 0;
  int maxFlickerPeriod = transitionSteps / 2, maxNumFlickers = 3;
  // Only want to flicker when pixel is close to red; these are cutoffs
  int flickerR = 18, flickerG = 1, flickerB = 1;
  // State-keeping
  int remainingFlickerSteps[numPixels];
  int remainingFlickerPeriods[numPixels];
  boolean flickeringBlack[numPixels];
  memset(remainingFlickerSteps, 0, sizeof(remainingFlickerSteps));
  memset(remainingFlickerPeriods, 0, sizeof(remainingFlickerPeriods));
  memset(flickeringBlack, false, sizeof(flickeringBlack));
  /* End flicker constants */
  
  /* Sparse flickering constants     */
  // How long at most can we stay black or colored for?
  int maxSparseBlackPeriod = transitionSteps * 2;
  // Initialize right here to random values per pixel (half black to start)
  int sparseBlackSteps[numPixels];
  for (int p = 0; p < numPixels; ++p) {
    sparseBlackSteps[p] = 1 + random(maxSparseBlackPeriod);
    if (random(1)) sparseBlackSteps[p] *= -1;
  }
  /* End sparse flickering constants */
  
  /* Ember constants     */
  // 1/this chance of spawning an ember per transition step
  int emberDenominator = 750;
  int maxEmberPeriod = transitionSteps * 2; // ember lifetime
  int maxEmberExtent = 3; // (one more than) max nearby pixels to catch
  int emberSteps[numPixels];
  memset(emberSteps, 0, sizeof(emberSteps));
  /* End ember constants */

  /* Mode selection and tracking     */
  int mode;
  unsigned long lastTime = millis();
  long remainingTime = 60000; // one hour
  
  // CHANGE TOGETHER - duration ratios for modes
  int modeRatios[NUM_MODES] = {1, 2, 3, 3, 3, 3};
  int totalModeRatios = 15;
  // END CHANGE TOGETHER
  
  unsigned long modeUnitTime = remainingTime / totalModeRatios;
  /* End mode selection and tracking */

  /*** MAIN LOOP     ***/
  while (remainingTime > 0) {
    // Timekeeping...
    unsigned long currentTime = millis();
    remainingTime -= (currentTime - lastTime);
    lastTime = currentTime;
    // ...to determine the right mode to be in
    unsigned long timeTally = 0;
    for (int m = NUM_MODES - 1; m >= 0; --m) {
      timeTally += modeUnitTime * modeRatios[m];
      if (timeTally > remainingTime) {
        mode = MODES[m];
        break;
      }
    }
    
    // Mode-based adjustments to flickering constants
    if (mode == LOW_FLICKER || mode == SPARSE_FLICKER || mode == EMBERS)
      shouldFlicker = 1; // Only flicker in these modes
    else shouldFlicker = 0;
    // Embers should flicker all the goddamn time
    if (mode == EMBERS) flickerR = 5, flickerG = 8, flickerB = 8;
    else flickerR = 18, flickerG = 1, flickerB = 1;
    
    /* Calculate pixel values for target image     */
    for (int p = 0; p < numPixels; ++p) {
      // FIRE
      long color = hsv2rgb(random(128)+384,0xff,0xff);
      uint8_t r = RGB_r(color), g = RGB_g(color), b = RGB_b(color);
      
      // Gamma correct (darken) in all but the brightest mode
      if (mode == MEDIUM_NO_FLICKER) {
        r = gamma(2*r + random(1)), g = gamma(2*g + random(1)),
          b = gamma(2*b + random(1));
      }
      else if (mode != BRIGHT_NO_FLICKER) {
        r = gamma(r), g = gamma(g), b = gamma(b);
      }
      
      // Store new color for this pixel
      stripColors[1-frontIdx][p*3]   = r;
      stripColors[1-frontIdx][p*3+1] = g;
      stripColors[1-frontIdx][p*3+2] = b;
    }
    /* End calculate pixel values for target image */
  
    // Cross-fade transition to above-calculated new color from current ones
    for (int i = 0; i < transitionSteps; ++i) {
      for (int p = 0; p < numPixels; p++) {
        uint32_t frontColor = strip.Color(stripColors[frontIdx][p*3],
          stripColors[frontIdx][p*3+1], stripColors[frontIdx][p*3+2]);
        uint32_t backColor = strip.Color(stripColors[1-frontIdx][p*3],
          stripColors[1-frontIdx][p*3+1], stripColors[1-frontIdx][p*3+2]);
          
        newColor = transitionValue(frontColor, backColor, i, transitionSteps);
        uint8_t newR = RGB_r(newColor), newG = RGB_g(newColor),
          newB = RGB_b(newColor);
        
        /* Flicker effect     */
        /* Note that we flicker even in SPARSE_FLICKER and EMBERS */
        
        // Enter the flicker
        if (newR >= flickerR && newG <= flickerG && newB <= flickerB &&
          remainingFlickerPeriods[p] == 0 && shouldFlicker) {
          remainingFlickerPeriods[p] = 1 + random(maxNumFlickers);
        }
        
        // Are we flickering?
        if (remainingFlickerPeriods[p] > 0) {
          // Toggle from off-flicker to on-flicker?
          if (remainingFlickerSteps[p] == 0) {
            remainingFlickerPeriods[p] -= 1;
            remainingFlickerSteps[p] = 1 + random(maxFlickerPeriod);
            flickeringBlack[p] = 1 - flickeringBlack[p];
          }
          // Turn black, decrement steps
          if (flickeringBlack[p]) newColor = black;
          remainingFlickerSteps[p] -= 1;
        }
        
        // If we're done, stop flickering
        if (remainingFlickerPeriods[p] == 0) remainingFlickerPeriods[p] = -1;
        
        // Can start flickering again once we've emerged from red
        if ((newR < flickerR || newG > flickerG || newB > flickerG) &&
          remainingFlickerPeriods[p] == -1) {
          remainingFlickerPeriods[p] = 0;
        }
        /* End flicker effect */
        
        // Sparse fire; black-based instead of color-based about half the time
        if (mode == SPARSE_FLICKER) {
          // Black period
          if (sparseBlackSteps[p] > 0) {
            newColor = black;
            sparseBlackSteps[p] -= 1;
            if (sparseBlackSteps[p] == 0)
              sparseBlackSteps[p] = (1 + random(maxSparseBlackPeriod)) * -1;
          }
          // Colored period
          if (sparseBlackSteps[p] < 0) {
            sparseBlackSteps[p] += 1;
            if (sparseBlackSteps[p] == 0)
              sparseBlackSteps[p] = 1 + random(maxSparseBlackPeriod);
          }
        }
        
        // Embers
        if (mode == EMBERS) {
          // Very occasionally spawn embers
          int emberRoll = random(emberDenominator);
          if (emberRoll == 0) {
            emberSteps[p] = random(maxEmberPeriod);
            // Catch up nearby pixels in the ember
            int emberExtent = random(maxEmberExtent);
            for (int e = 1; e <= emberExtent; ++e) {
              emberSteps[max(p-e,0)] = random(maxEmberPeriod);
              emberSteps[min(p+e,numPixels)] = random(maxEmberPeriod);
            }
          }
          // If we're not in an ember, we're just black
          if (emberSteps[p] == 0) {
            newColor = black;
          }
          emberSteps[p] = max(emberSteps[p] - 1, 0);
        }
        // New color calculated!
        strip.setPixelColor(p, newColor);
      }
      strip.show(); // takes about 6ms in its own right
      delay(wait); // between transition steps
    }

    // Switch which image is front
    frontIdx = 1-frontIdx;
  }
  /*** END MAIN LOOP ***/
}
/***** END FIRE MELD *****/

/**********************/

const PROGMEM unsigned char gammaTable[]  = {
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  1,  1,  1,  1,
    1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  2,  2,  2,  2,
    2,  2,  2,  2,  2,  3,  3,  3,  3,  3,  3,  3,  3,  4,  4,  4,
    4,  4,  4,  4,  5,  5,  5,  5,  5,  6,  6,  6,  6,  6,  7,  7,
    7,  7,  7,  8,  8,  8,  8,  9,  9,  9,  9, 10, 10, 10, 10, 11,
   11, 11, 12, 12, 12, 13, 13, 13, 13, 14, 14, 14, 15, 15, 16, 16,
   16, 17, 17, 17, 18, 18, 18, 19, 19, 20, 20, 21, 21, 21, 22, 22,
   23, 23, 24, 24, 24, 25, 25, 26, 26, 27, 27, 28, 28, 29, 29, 30,
   30, 31, 32, 32, 33, 33, 34, 34, 35, 35, 36, 37, 37, 38, 38, 39,
   40, 40, 41, 41, 42, 43, 43, 44, 45, 45, 46, 47, 47, 48, 49, 50,
   50, 51, 52, 52, 53, 54, 55, 55, 56, 57, 58, 58, 59, 60, 61, 62,
   62, 63, 64, 65, 66, 67, 67, 68, 69, 70, 71, 72, 73, 74, 74, 75,
   76, 77, 78, 79, 80, 81, 82, 83, 84, 85, 86, 87, 88, 89, 90, 91,
   92, 93, 94, 95, 96, 97, 98, 99,100,101,102,104,105,106,107,108,
  109,110,111,113,114,115,116,117,118,120,121,122,123,125,126,127
};

inline byte gamma(byte x) {
  return pgm_read_byte(&gammaTable[x]);
}

inline uint8_t RGB_r(uint32_t rgb) {
  //return (uint8_t)(0x7f & (rgb >> 16));
  return (uint8_t)(rgb >> 16);
}

inline uint8_t RGB_g(uint32_t rgb) {
  //return (uint8_t)(0x7f & (rgb >> 8));
  return (uint8_t)(rgb >> 8);
}

inline uint8_t RGB_b(uint32_t rgb) {
  //return (uint8_t)(0x7f & (rgb));
  return (uint8_t)rgb;
}

long hsv2rgb(long h, byte s, byte v) {
  byte r, g, b, lo;
  int  s1;
  long v1;

  // Hue
  h %= 1536;           // -1535 to +1535
  if(h < 0) h += 1536; //     0 to +1535
  lo = h & 255;        // Low byte  = primary/secondary color mix
  switch(h >> 8) {     // High byte = sextant of colorwheel
    case 0 : r = 255     ; g =  lo     ; b =   0     ; break; // R to Y
    case 1 : r = 255 - lo; g = 255     ; b =   0     ; break; // Y to G
    case 2 : r =   0     ; g = 255     ; b =  lo     ; break; // G to C
    case 3 : r =   0     ; g = 255 - lo; b = 255     ; break; // C to B
    case 4 : r =  lo     ; g =   0     ; b = 255     ; break; // B to M
    default: r = 255     ; g =   0     ; b = 255 - lo; break; // M to R
  }

  // Saturation: add 1 so range is 1 to 256, allowig a quick shift operation
  // on the result rather than a costly divide, while the type upgrade to int
  // avoids repeated type conversions in both directions.
  s1 = s + 1;
  r = 255 - (((255 - r) * s1) >> 8);
  g = 255 - (((255 - g) * s1) >> 8);
  b = 255 - (((255 - b) * s1) >> 8);

  // Value (brightness) and 24-bit color concat merged: similar to above, add
  // 1 to allow shifts, and upgrade to long makes other conversions implicit.
  v1 = v + 1;
  return (((r * v1) & 0xff00) << 8) |
          ((g * v1) & 0xff00)       |
         ( (b * v1)           >> 8);
}

void printRGB(uint32_t rgb) {
  Serial.print("(");
  Serial.print(int(RGB_r(rgb)));
  Serial.print(",");
  Serial.print(int(RGB_g(rgb)));
  Serial.print(",");
  Serial.print(int(RGB_b(rgb)));
  Serial.println(")");
}

uint32_t transitionValue(uint32_t init, uint32_t final, int now, int total) {
  uint8_t frontR = RGB_r(init), frontG = RGB_g(init), frontB = RGB_b(init);
  uint8_t backR = RGB_r(final), backG = RGB_g(final), backB = RGB_b(final);
  
  // Linear transition
  uint8_t newR = frontR + (backR-frontR)*(transitionValueFunction((float)now/total)),
    newG = frontG + (backG-frontG)*(transitionValueFunction((float)now/total)),
    newB = frontB + (backB-frontB)*(transitionValueFunction((float)now/total));
        
  return strip.Color(newR, newG, newB);
}

float transitionValueFunction(float x) {
  return x;
  return 0.5*(2*x-1)*(2*x-1)*(2*x-1)+0.5;
  if (x > 0.5) {
    return -1 * (1/(2*x) -1) + 0.5;
  } else {
    return 1/(2 - 2*x) - 0.5;
  }
  //return 1 - sqrt(1 - x*x);
  return x*x;
  return sqrt(2)*x / sqrt(1 + x*x);
  return 2*x / (1 + x);
}

