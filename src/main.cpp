/******************************************************************************

  "Multiverse's Lowest LO-FI Wood Pixel Display—Arduino Art" (Version 1.0)
  https://www.youtube.com/watch?v=4f1J5LzRdIo

  This code was written to power the WTD 1.0 (Wood Tile Display) project 
  created while participating in Mark Rober's 
  https://monthly.com/mark-rober-engineering course.

  The code, originally written in a day, started out based on an AdaFruit 
  sample, and uses Adafruit's library for PCA9685 based 16-channel PWM & Servo 
  drivers.  Consider buying drivers and other hardware goodies from the 
  Adafruit shop @ http://www.adafruit.com/products/815
  
  Adafruit invests time and resources providing this open source code, 
  please support Adafruit and open-source hardware by purchasing 
  products from Adafruit!

  Cheers!

  aaron (Aza's Built to Code)
******************************************************************************/

#include <Wire.h>
#include <Adafruit_PWMServoDriver.h>

// Servo driver I2C Address definitions
Adafruit_PWMServoDriver pwmDrivers[] =
{
   Adafruit_PWMServoDriver(0x40),
   Adafruit_PWMServoDriver(0x41),
   Adafruit_PWMServoDriver(0x42)
};

#define DRIVER_COUNT 3

// Depending on your servo make, the pulse width min and max may vary, you 
// want these to be as small/large as possible without hitting the hard stop
// for max range. You'll have to tweak them as necessary to match the servos
// you have!

#define SERVOMIN  150 // This is the 'minimum' pulse length count (out of 4096)
#define SERVOMAX  600 // This is the 'maximum' pulse length count (out of 4096)
#define USMIN  600    // This is the rounded 'minimum' microsecond length based on the minimum pulse of 150
#define USMAX  2400   // This is the rounded 'maximum' microsecond length based on the maximum pulse of 600

#define SERVO_FREQ 60 // Analog servos run at ~60 Hz updates.  Try 50 (original 
                      // sample value) if your servos have a seizure using 60.

#define SERVO_COUNT 16

#define MIN_SERVO_ANGLE 0
#define MAX_SERVO_ANGLE 60
#define DEFAULT_SERVO_ANGLE 30

// Servo panel layout
#define TILESET_WIDTH 4
#define TILESET_HEIGHT 4
#define CANVAS_WIDTH 12
#define CANVAS_HEIGHT 4
#define PIXEL_COUNT CANVAS_WIDTH * CANVAS_HEIGHT

// Animation modes
#define MODE_NONE 0
#define MODE_WAVE 1
#define MODE_INVERT 2
int _mode = 0;

// Menu variable to pick each menu item(or display the menu)
int _menu = -1;

// Flag indicating whether to move/animate
bool _isActive = true;

float _pixels[PIXEL_COUNT];
bool _pixelsDirty = true;
int _currentPixelIndex = 0;
unsigned long _lastChangeMs = 0;
unsigned long _invertWaitIntervalMs = 10;
unsigned long _waveWaitIntervalMs = 10;

void drawFill(int color);
void processMenu();
void processPixels();
void woodTilePanel_loop();


void setup() {
  Serial.begin(9600);
  Serial.println("setup");

  for (int i = 0; i < DRIVER_COUNT; i++) {
    pwmDrivers[i].begin();

  /* From AdaFruit sample code:
   * "In theory the internal oscillator (clock) is 25MHz but it really isn't
   * that precise. You can 'calibrate' this by tweaking this number until
   * you get the PWM update frequency you're expecting!
   * The int.osc. for the PCA9685 chip is a range between about 23-27MHz and
   * is used for calculating things like writeMicroseconds()
   * Analog servos run at ~50 Hz updates, It is importaint to use an
   * oscilloscope in setting the int.osc frequency for the I2C PCA9685 chip.
   * 1) Attach the oscilloscope to one of the PWM signal pins and ground on
   *    the I2C PCA9685 chip you are setting the value for.
   * 2) Adjust setOscillatorFrequency() until the PWM update frequency is the
   *    expected value (50Hz for most ESCs)
   * Setting the value here is specific to each individual I2C PCA9685 chip and
   * affects the calculations for the PWM update frequency. 
   * Failure to correctly set the int.osc value will cause unexpected PWM results"
   */
    pwmDrivers[i].setOscillatorFrequency(27000000);
    pwmDrivers[i].setPWMFreq(SERVO_FREQ);

    delay(10);
  }
}


// Set pulse length in seconds. e.g. setServoPulse(0, 0.001) is a ~1 ms pulse width. It's not precise!
void setServoPulse(uint8_t driverIndex, uint8_t n, double pulse) {

  double pulselength;
  
  pulselength = 1000000;        // 1,000,000 us per second
  pulselength /= SERVO_FREQ;    // Analog servos run at ~60 Hz updates
  Serial.print(pulselength); 
  pulselength /= 4096;          // 12 bits of resolution
  Serial.print(pulselength); 
  pulse *= 1000000;             // convert input seconds to us
  pulse /= pulselength;
  Serial.println(pulse);

  pwmDrivers[driverIndex].setPWM(n, 0, pulse);
}


void loop() {

  processMenu();

  processPixels();

  woodTilePanel_loop();

  delay(10);
}


void processPixels() {

  // Bail if inactive
  if (!_isActive) return;

  // Invert mode and time to invert next pixel?
  if (_mode == MODE_INVERT &&  millis() > _lastChangeMs + _invertWaitIntervalMs) {
    _lastChangeMs = millis();

    if (_currentPixelIndex >= PIXEL_COUNT) {
      _currentPixelIndex = 0;
    }

    float newColor = 100 - _pixels[_currentPixelIndex];
    _pixels[_currentPixelIndex] = newColor;
    _pixelsDirty = true;
    _currentPixelIndex++;
  }

  if (_mode == MODE_WAVE && millis() > _lastChangeMs + _waveWaitIntervalMs) {

    Serial.print("processPixels MODE_WAVE");

    if (_currentPixelIndex >= CANVAS_WIDTH) {
      _currentPixelIndex = 0;
    }

    for(int x = 0; x < CANVAS_WIDTH; x++) {
      
      _lastChangeMs = millis();
      _pixelsDirty = true;

      // Compute Degree phase for current column, offset by _currentPixelIndex.  Stretch full wave 
      // to span width of the canvas.
      long waveLength = CANVAS_WIDTH;
      float phaseRatio  = ((1.0 * ((x + _currentPixelIndex) % waveLength)) / waveLength);
      float phaseAngleDeg = (int)(phaseRatio * 360.0) ;//% 360;
      // TODO:P0 Implement enabling configurable phase delta and overall wave length to span longer than canvas width.

      // Convert column's phase angle to radians so we can calculate sin wave height.
      float phaseAngleRad = phaseAngleDeg * 0.01745329252;
      float amplitude = sin(phaseAngleRad);

      // Map to brightness scale (0 - 100)
      float pixelColor = (amplitude + 1.0) * 50.0;

      if (x == 0) {
        Serial.print("phaseRatio: ");
        Serial.print(phaseRatio);
        Serial.print(", phaseAngleDeg: ");
        Serial.print(phaseAngleDeg);
        Serial.print(", phaseAngleRad: ");
        Serial.print(phaseAngleRad);
        Serial.print(", amplitude: ");
        Serial.print(amplitude);
        Serial.print(", pixelColor: ");
        Serial.print(pixelColor);
        Serial.println();
      }

      for(int y = 0; y < CANVAS_HEIGHT; y++) {
        _pixels[(CANVAS_WIDTH * y) + x] = pixelColor;
      }
    }

    _currentPixelIndex++;
  }
}

void woodTilePanel_loop() {

  // Bail if inactive
  if (!_isActive) {
    return;
  } 

  // TODO:P2 Bail if !_pixelsDirty optimization.
  
  // Enumerate through each pixel.
  for (int row = 0; row < CANVAS_HEIGHT; row++) {
    for (int col = 0; col < CANVAS_WIDTH; col++) {

      float pixelBrightness = _pixels[row * CANVAS_WIDTH + col];

      // Ensure brightness within bounds
      // TODO:P0 ASSERT/WARN about unexpected values, fix upstream bug(s).
      if (pixelBrightness < 0.0 ) pixelBrightness = 0.0;
      if (pixelBrightness > 100) pixelBrightness = 100;

      // Map pixel brightness to servo angle in degrees
      long servoAngle = map(pixelBrightness, 0, 100, MIN_SERVO_ANGLE, MAX_SERVO_ANGLE);

      // ENSURE angle value is bounded.  Also, WARN about unexpected values, fix upstream bug(s).
      if (servoAngle > MAX_SERVO_ANGLE) {
        Serial.print("WARN: Ignoring unexpected large angle, servoAngle=");
        Serial.println(servoAngle);

        servoAngle = MAX_SERVO_ANGLE;
      }

      if (servoAngle < MIN_SERVO_ANGLE) {
        Serial.print("WARN: Ignoring unexpected small angle, servoAngle=");
        Serial.println(servoAngle);

        servoAngle = MIN_SERVO_ANGLE;
      }

      // Map servo angle to pulse length
      long pulseLength = map(servoAngle, 0, 180, SERVOMIN, SERVOMAX);

      // Lookup PWM Driver index for current pixel
      // TODO:P1 Implement Canvas-TileSet map when PWM drivers not in order
      // Assumptions:
      // - TileSets are layed out from top left, row by row.
      int pwmIndex = 
        ((CANVAS_WIDTH / TILESET_WIDTH) * (row / TILESET_HEIGHT)) + // Compute Row tileset offset
        (col / TILESET_WIDTH);                                      // Compute Col tileset

      // Lookup pixel servo
      Adafruit_PWMServoDriver pwmDriver = pwmDrivers[pwmIndex];

      // Assumptions:
      // - Tile wiring is layed out column by colum, from top to bottom within a column.
      //    0  4  8 12
      //    1  5  9 13
      //    2  6 10 14
      //    3  7 11 15
      int servoNum =
        (col % TILESET_WIDTH) * TILESET_HEIGHT + (row % TILESET_HEIGHT);
      
      // Set servo PWM
      pwmDriver.setPWM(servoNum, 0, pulseLength);
    }
  }

  _pixelsDirty = false;
}


void processMenu()
{
  // Display menu if just started of finished some other action.
  if (_menu == -1) {
    
    // Print menu to serial port (based on Mark Rober's example)
    Serial.println("Select an option:");
    Serial.println("-----------------");
    Serial.println((_isActive) ? "1) Stop!!" : "1) Go...");
    Serial.println("2) Set Servos to Min");
    Serial.println("3) Set Servos to Mid");
    Serial.println("4) Set Servos to Max");
    Serial.println("5) Wave Mode");
    Serial.println("6) Invert Mode");
    Serial.flush();

    _menu = 0;
  }

  // Check if we have input data on the serial port to process?
  if (Serial.available()) {
    _menu = Serial.parseInt(); // Takes the Serial input and looks for an integer
    
    Serial.print(">");
    Serial.println(_menu);
    Serial.flush();
  }

  if (_menu == 1) {
    _isActive = !_isActive;
  }  
  else if (_menu == 2) {
    _isActive = true;
    _mode = MODE_NONE;
    drawFill(0);
  }
  else if (_menu == 3) {
    _isActive = true;
    _mode = MODE_NONE;
    drawFill(50);
  }
  else if (_menu == 4) {
    _isActive = true;
    _mode = MODE_NONE;
    drawFill(100);
  }
  else if (_menu == 5) {
    _isActive = true;
    _mode = MODE_WAVE;
    Serial.println("MODE_WAVE selected");

    _currentPixelIndex =  0;  // Use to track sin wave by column
  }  
  else if (_menu == 6) {
    _isActive = true;
    _mode = MODE_INVERT;
    Serial.println("MODE_INVERT selected");
    
    _currentPixelIndex =  0; // Use to tracke pixel being inverted
  }  
  else if (_menu > 0) {
    Serial.print("'");
    Serial.print(_menu);
    Serial.print("' is not a valid menu option.  Try harder.");
    Serial.println();
  }

  // Reset menu state to display prompt on next iteration
  if (_menu > 0) {
    _menu = -1;
  }
}


void drawFill(int color)
{
  for (int i = 0; i < CANVAS_WIDTH * CANVAS_HEIGHT; i++) {
    _pixels[i] = color;
  }
  _pixelsDirty = true;
}
