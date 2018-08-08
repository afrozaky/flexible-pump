/** Flexible Pump Platform
    Authors       : Zaky Hussein, Priyanka Deo
    Last Modified : August 8th, 2018
    This program does the following:
    -> Measures the frequency of EMF cycles
    -> Synchronizes the working fluid cycle with the EMF cycle
    -> Linearly accelerates a stepper motor to bring it to a maximum frequency, runs it at the frequency for a set time,
    then decelerates it back to a minimum frequency. Then it changes the direction of rotation and repeats the sequence a given number of times.

    Please follow the folllowing instructions for running the code:
    1) Turn the sine EMF ON by increasing the temperature to 37 (GE magnetic refigeration prototype)
    2) Hit the upload button on the screen (the arrow next to the checkmark right beneath edit)
    3) Select Tools -> Serial Monitor
    4) Turn motor driver ON when indicated
    5) Follow remaining instructions on the serial monitor
**/

//-------------------------------------
// INITIALIZING - Declaring inputs and constants
//-------------------------------------

// Input/Output Pins
const byte interruptPin = 3;                                                           //pin for limit switch mounted on the rotating magnetic drum
const byte DIR = 9;                                                                 //direction pin on Arduino board
const byte CLOCK = 10;                                                              //clock pin on Arduino board
const byte LIMIT = 12;                                                              //home limit switch pin on Arduino board
const byte ONOFF = 2;                                                               //On off pin on Arduino board, HIGH turns off the driver

double fluxRPM = 0;
double fluxFreq = 0;

//USER INPUTS
double strokeLength = 0;                                                            //[mm] stroke length to cover in a half cycle
double cycles = 0;                                                                  //Number of cycles to run the pump for (one cycle means moving the stroke length away from home and back to home)
int speedSetting = 0;                                                               //Current speed setting of GE prototype

//Stepper Motor Motion Variables
double peakTime;                                                                    //Time for which the max frequency will be held
const double startFreq = 250;                                                   // [Hz] the start/stop frequency, minimum value of 62 Hz by precision of delayMicroseconds function
double maxFreq;                                                                     // [Hz] the max frequency attained
double maxPeriod;                                                                   // [us] associated period with maxFreq
const double df = 135;                                                              // [Hz] frequency increments
unsigned int dt = 2950;                                                             // [us] time increments
double rampSteps;                                                                   // the number of steps (or pulses) that should be covered during one ramp up or one ramp down                                         
double constSteps;                                                                  // the number of steps (or pulses) that should be sent at the constant max frequency
double totalTime;                                                                   // [s] time for one half cycle/full stroke length
double travelTime;

//Variables that change throughout the code
unsigned long initialTime;                                                          //Saves an initial time that can be used later to calculate time passed
double freq;                                                                        //The current frequency being outputted to the stepper motor
unsigned long stepCount = 0;                                                        //Used to count steps throughout frequency profile
unsigned long totalSteps = 0;
int counter = 0;
unsigned long initialTime2;                                                         
unsigned long totalTimebegin;
double period;

//-------------------------------------
// setup() - runs once at startup
//-------------------------------------

void setup() {
  Serial.begin(2000000);                                                            // initialize the serial port and display
  pinMode(interruptPin, INPUT_PULLUP);                                              // enable the pullup resistor and attach the interrupt
  pinMode(ONOFF, OUTPUT);
  pinMode(DIR, OUTPUT);                                                             // driver and limit switch settings
  pinMode(CLOCK, OUTPUT);
  pinMode(LIMIT, INPUT_PULLUP);
  digitalWrite(ONOFF, LOW);
  digitalWrite(CLOCK, LOW);
  digitalWrite(DIR, HIGH);   //LOW -> TOWARDS MOTOR; HIGH -> AWAY FROM MOTOR
}

//-------------------------------------
// Helper methods - called throughout the program
//-------------------------------------

//-------------------------------------
// homeleft() - HOME TO THE LEFT LIMIT SWITCH
// home position determined by limt switch
// Brings pistons back to the home position
//-------------------------------------

void homeleft() {
  digitalWrite(DIR, LOW);                                                          // LOW  -> TOWARDS MOTOR (LEFT)
  double homingFreq = 750;
  double homingPeriod = 1 / homingFreq * pow(10, 6);
  while (digitalRead(LIMIT) == LOW) {                                             // run at homing frequency until limit hit
    digitalWrite(CLOCK, HIGH);
    delayMicroseconds(homingPeriod / 2.0);
    digitalWrite(CLOCK, LOW);
    delayMicroseconds(homingPeriod / 2.0);
  }
  digitalWrite(DIR, HIGH);                                      //Change direction to be away from limit switch
}

//-------------------------------------
// waitTime() - SYNCHRONIZE MOTIONS
// holds the pistons at home until
// new EMF cycle is started
//-------------------------------------

void waitTime() {
  while (digitalRead(interruptPin) == LOW) {}
}

//-------------------------------------
// loop() - runs until input number of cycles
//-------------------------------------

void loop() {
  // HANDLE USER INPUT SECTION
  if (strokeLength == 0 || cycles == 0 || speedSetting == 0) {
    counter = 0;                                                                    // Reset counter to zero
    Serial.println("Enter the current speed setting of magnetic refigeration prototype, 1 to 2");
    while (Serial.available() == 0) {}
    speedSetting = Serial.parseInt();

    Serial.println("Enter stroke length in mm (the length of each stroke of the syringes, between 15-50 mm");                                   // Prompt User for input
    while (Serial.available() == 0) {}
    strokeLength = Serial.parseFloat();                                             // Read user input into stroke length

    Serial.println("Enter number of cycles (the number of magnetic flux cycles to run the flexible platform for)");                                      // Prompt User for input
    while (Serial.available() == 0) {}
    cycles = Serial.parseFloat();                                                   // Read user input into cycles

    fluxFreq = 0.35 * speedSetting;                                                    //Speed setting scales linearly 1 RPS = 0.35 Hz, characterized using tachometer code
    fluxRPM = fluxFreq * 60.0;
    totalTime = 1.0 / 2.0 / fluxFreq;

    if (speedSetting == 1) {
      travelTime = 0.76 * totalTime;                        //Decrease travel time to increase speed at speed setting 1 to stay within reliable operating frequency of stepper motor
    } else if (speedSetting == 2) {
      travelTime = 0.9 * totalTime - 0.12;                  //Need to decrease travel time less at higher speed settings
    }

    //Initialize variables from user input
    peakTime = sqrt(pow(travelTime, 2) - 3840 * strokeLength / pow(10, 6));        // [s] time for which max frequency should be maintained
    maxFreq = pow(10, 6) * (travelTime - peakTime) / 48;
    maxPeriod = 1 / (maxFreq) * pow(10, 6);                           // [us] period corresponding to maximum frequency
    rampSteps = ((maxFreq - startFreq) / 2.0) * .024 * maxFreq * .001;
    constSteps = maxFreq * peakTime;
    period = 1 / (startFreq) * pow(10, 6);                           // [us] Initialize period to start/stop period

    if (speedSetting > 2 || speedSetting < 0) {
      Serial.println("Please enter a speed setting in the range of 1 to 2");
      speedSetting = cycles = strokeLength = 0;
      delay(500);
    }

    if (strokeLength > 50) {
      Serial.println("Stroke length > 50, enter a decreased stroke length");
      strokeLength = cycles = 0;
      delay(500);
    }

    if (maxFreq > 5200) {
      Serial.println("Entered stroke length yields a max frequency > 5200, enter a decreased stroke length");
      strokeLength = cycles = 0;
      delay(500);
    } else if (maxFreq < 500) {
      Serial.println("Entered stroke length yields a max frequency < 500, enter an increased stroke length");
      strokeLength = cycles = 0;
      delay(500);
    }

    if (cycles != 0 && strokeLength != 0) {
      Serial.print("Total time calculated: ");
      Serial.println(totalTime);
      Serial.print("Travel time calculated: ");
      Serial.println(travelTime);
      Serial.print("Stroke length entered: ");
      Serial.println(strokeLength);
      Serial.print("Number of cycles entered: ");
      Serial.println(cycles);
      Serial.print("Average Stroke Frequency: ");
      Serial.println(1.0 / (2.0 * totalTime));
      Serial.print("Max Stepper RPM: ");
      Serial.println(maxFreq / 200.0 * 60);
      Serial.print("Max Stepper Frequency: ");
      Serial.println(maxFreq);
      Serial.println(cycles * 1.0 / fluxFreq);
      Serial.println("Beginning motor control sequence in 3 seconds");
      homeleft();                                        // bring pistons to home position before starting cycles
      delay(3000);
      waitTime();
    }
  }

  // SYNCHRONIZE AND RUN THE STEPPER MOTOR
  delay(0.05 * totalTime * pow(10, 3));              // Add initial delay to match Brayton cycle
  while (counter < 2 * cycles) {
    totalTimebegin = millis();
    stepCount = 0;
    totalSteps = 0;
    initialTime2 = millis();
    // RAMP UP TO MAX FREQUENCY
    while (stepCount < rampSteps && freq < maxFreq) {
      initialTime = micros();
      while (micros() - initialTime < dt) {
        digitalWrite(CLOCK, HIGH);
        delayMicroseconds(period / 2.0);
        digitalWrite(CLOCK, LOW);
        delayMicroseconds(period / 2.0);
        stepCount++;
        if (stepCount >= rampSteps)
          break;
      }
      freq += df;
      period = 1 / freq * pow(10, 6);
    }
    totalSteps += stepCount;
    //Serial.println(millis()-initialTime2);
    //Serial.println(freq);

    // REMAIN AT MAX FREQUENCY
    stepCount = 0;
    freq = maxFreq;
    for (int i = 0; i < constSteps; i++) {
      digitalWrite(CLOCK, HIGH);
      delayMicroseconds(period / 2.0);
      digitalWrite(CLOCK, LOW);
      delayMicroseconds(period / 2.0);
      stepCount++;
    }

    totalSteps += stepCount;
    // RAMP DOWN FROM MAX FREQUENCY
    stepCount = 0;
    while (freq > startFreq)  {
      initialTime = micros();
      while (micros() - initialTime < dt) {
        digitalWrite(CLOCK, HIGH);
        delayMicroseconds(period / 2.0);
        digitalWrite(CLOCK, LOW);
        delayMicroseconds(period / 2.0);
        stepCount++;
      }
      freq -= df;
      period = 1 / freq * pow(10, 6);
    }
    totalSteps += stepCount;

    /*if (counter % 2 == 1) {                                                             //Drift correction
      if (digitalRead(LIMIT) == LOW) {
        double homingFreq = 750;
        double homingPeriod = 1 / homingFreq * pow(10, 6);
        while (digitalRead(LIMIT) == LOW) {                                             // run at homing frequency until limit hit
          digitalWrite(CLOCK, HIGH);
          delayMicroseconds(homingPeriod / 2.0);
          digitalWrite(CLOCK, LOW);
          delayMicroseconds(homingPeriod / 2.0);
        }
      }
    }*/

    //    Serial.println(millis() - initialTime2);
    //    Serial.println(stepCount);
    counter++;                                        // End of one half cycle, increment counter

    // DIRECTION SWITCH
    digitalWrite(DIR, !digitalRead(DIR));

    if (counter % 2 == 1) {
      delay(0.05 * totalTime * pow(10, 3));           // Add delay between changing directions to match Gauss curve
    }
    else {
      //      initialTime = millis();
      waitTime();
      Serial.println(totalSteps);
      //      Serial.println("Wait Time: ");
      //      Serial.println(millis() - initialTime);
      //      Serial.println("Total Time: ");
      //      Serial.println(millis() - totalTimebegin);
      //      Serial.println(counter);
    }

    delay(0.05 * totalTime * pow(10, 3));         // Add delay between changing directions to match Brayton cycle
  }

}
