/** LINEAR STEPPER ACCELERATION
    Authors       : Zaky Hussein, Priyanka Deo
    Last Modified : June 18th, 2018
    The following program moves the stepper motor to bring it to the ball screw to the home position.
*/

// INPUTS
const byte DIR = 9;                                                                 // direction pin on Arduino board
const byte STEP = 10;                                                               // clock pin on Arduino board
const byte LIMIT = 12;


// SETUP
void setup() {
  pinMode(DIR, OUTPUT);
  pinMode(STEP, OUTPUT);
  pinMode(LIMIT, INPUT_PULLUP);
  digitalWrite(DIR, HIGH);   //LOW -> TOWARDS MOTOR; HIGH -> AWAY FROM MOTOR
  digitalWrite(STEP, LOW);
  Serial.begin(2000000);
  homeleft();
}


//Home to the left limit switch
void homeleft() {
  digitalWrite(DIR, HIGH);                 //LOW == towards motor (left)            
  double homingFreq = 750;
  double homingPeriod = 1 / homingFreq * pow(10, 6);
  while (digitalRead(LIMIT) == HIGH) {   //Run at homing frequency until limit hit
    digitalWrite(STEP, HIGH);
    delayMicroseconds(homingPeriod / 2.0);
    digitalWrite(STEP, LOW);
    delayMicroseconds(homingPeriod / 2.0);
  }
}

void loop() {
}
