//-------------------------------------
// Tachometer
//-------------------------------------

// External interrpt pin for sensor
#define interruptPin 3

unsigned long lastUpdate = 0;  // for timing display updates
volatile long accumulator = 0;  // sum of last 8 revolution times
volatile unsigned long startTime = 0; // start of revolution in microseconds
volatile unsigned int revCount = 0; // number of revolutions since last display update
double rpm = 0;
double freq = 0;
double oldFreq;
double error = 1;
double eps = 0.01 * freq;

//-------------------------------------
// Setup - runs once at startup
//-------------------------------------
void setup()
{
  // Initialize the serial port and display
  Serial.begin(2000000);

  // Enable the pullup resistor and attach the interrupt
  pinMode(interruptPin, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(interruptPin), tach_interrupt, FALLING);
}

//-------------------------------------
// Interrupt Handler
// IR reflective sensor - target passed
// Calculate revolution time
//-------------------------------------
void tach_interrupt()
{
  // calculate the microseconds since the last interrupt
  long usNow = micros();
  long elapsed = usNow - startTime;
 
  if(elapsed > 100000){
  startTime = usNow;  // reset the clock                                 
  accumulator -= (accumulator >> 1);              // Accumulate the last 8 interrupt intervals
  accumulator += elapsed;
  revCount++;
  }
}

//-------------------------------------
// Main Loop - runs repeatedly.
// Calculate RPM and Update LCD Display
//-------------------------------------
void loop() {
  if (millis() - lastUpdate > 1000) // update every second
  {
    // divide number of microseconds in a minute, by the average interval.
    if (revCount > 0)
    {
      oldFreq = freq;
      rpm = 60000000.0 / (accumulator >> 1);
      freq = (double) rpm / 60.0;
      error = abs(oldFreq - freq) / freq;
    }

    if (rpm != 0) {
      //Serial.println(rpm);
      Serial.println(freq);
    }
    lastUpdate = millis();
    revCount = 0;
  }
}

