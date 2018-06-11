//Joseph Schroedl FRC Team 2059

//A delay of 1000 Microseconds is Full Reverse
//A delay of 1000 to 1460 Microseconds is Proportional Reverse
//A delay of 1460 to 1540 Microseconds is neutral
//A delay of 1540 to 2000 Microseconds is Proportional Forward
//A delay of 2000 Microseconds is Full Forward

//For Xbox controller with USB shield
#include <XBOXRECV.h>

//Other Includes for USB shield
#ifdef dobogusinclude
#include <spi4teensy3.h>
#include <SPI.h>
#endif

//Need to change USB library pin assignments so the shield works with Arduino Mega
#if defined(AVR_ATmega1280) || (AVR_ATmega2560)
#define SCK_PIN 52
#define MISO_PIN 50
#define MOSI_PIN 51
#define SS_PIN 53
#endif
#if defined(AVR_ATmega168) || defined(AVR_ATmega328P)
#define SCK_PIN 13
#define MISO_PIN 12
#define MOSI_PIN 11
#define SS_PIN 10
#endif
#define MAX_SS 53
#define MAX_INT 9
#define MAX_GPX 8
#define MAX_RESET 7

/*****User Configurable Variables*****/

//Joystick Deadzones. This zone keeps the motor controllers from fluctuation from off to +-1% speed.
const int lowNeutral = 1460;
const int highNeutral = 1540;

//This is deadzone threshold on the joystick because the resting position of the joystick varries. Making this value bigger will reqire the user to move the joystick further before the code starts using the joystick values
const int joystickDeadzone = 7500;

const int backLeft = 43;        //Back Left Motor pin
int backLeftSpeed = 1500; //Back Left Motor starting speed

const int frontLeft = 32;        //Front Left Motor pin
int frontLeftSpeed = 1500; //Front Left Motor starting speed

const int backRight = 33;        //Back Right Motor pin
int backRightSpeed = 1500; //Back Right Motor starting speed

const int frontRight = 34;        //Front Right Motor pin
int frontRightSpeed = 1500; //Front Right Motor starting speed

const int topRoller = 31;        //Top Roller Motor pin
//1460 for no running motors, no buttons held.
//1600 for low power, "X" button held.
//1800 for medium power, "A" button held.
//2000 for high power, "B" button held.
int topRollerSpeed = 1500; //Top Roller Motor starting speed

const int bottomRoller = 36;                                                                       //Bottom Roller Motor pin
//1460 for no running motors, no buttons held.
//1600 for low power, "X" button held.
//1800 for medium power, "A" button held.
//2000 for high power, "B" button held.
int bottomRollerSpeed = 1500; //Top Roller Motor starting speed

//Set to false to receive serial motor speed data. If false, Arduino will run motor controllers.
bool sendToMotor = true;

//Use the Xbox controller number 0, I only have one controller
const int controlNum = 0;

/*****Non-Configurable Variables*****/

short joyX = 0;             //joyX < 0 = Left, joyX > 0 = Right
short joyY = 0;             //joyY > 0 = Forward, joyY < 0 = Reverse

//Back Left Motor
unsigned long endTimeBackLeftHigh = 0;
unsigned long endTimeBackLeftLow = 0;
bool backLeftL = false;
bool initBackLeft = false;

//Front Left Motor
unsigned long endTimeFrontLeftHigh = 0;
unsigned long endTimeFrontLeftLow = 0;
bool frontLeftL = false;
bool initFrontLeft = false;

//Back Right Motor
unsigned long endTimeBackRightHigh = 0;
unsigned long endTimeBackRightLow = 0;
bool backRightL = false;
bool initBackRight = false;

//Front Right Motor
unsigned long endTimeFrontRightHigh = 0;
unsigned long endTimeFrontRightLow = 0;
bool frontRightL = false;
bool initFrontRight = false;

//Top Roller Motor
unsigned long endTimeTopRollerHigh = 0;
unsigned long endTimeTopRollerLow = 0;
bool topRollerL = false;
bool initTopRoller = false;

//Bottom Roller Motor
unsigned long endTimeBottomRollerHigh = 0;
unsigned long endTimeBottomRollerLow = 0;
bool bottomRollerL = false;
bool initBottomRoller = false;

//Variable to keep track of whether we are driving in any direction
bool driving = false;

//Variable to keep track of weather we are shooting
bool shooting = false;

//Variable used by the custom PWM code to keep track of how many motors are running.
int motorsRunning = 0;

//Initialization for USB shield
USB Usb;
XBOXRECV Xbox(&Usb);

void setup()
{
  //Turn on pin 7 to fix a problem with early versions of Sparkfun USB Host shield.
  pinMode(7, OUTPUT);
  digitalWrite(7, HIGH);

  //Initialize USB shield
  Usb.Init();
  //Wait for initialization before continueing
  delay(1000);

  //Setup for the motor controller outputs pins
  //Drive train
  pinMode(backLeft, OUTPUT);
  pinMode(frontLeft, OUTPUT);
  pinMode(backRight, OUTPUT);
  pinMode(frontRight, OUTPUT);
  //Other motors
  pinMode(topRoller, OUTPUT);
  pinMode(bottomRoller, OUTPUT);

  Serial.begin(115200);

  //Wait for USB and other setup to finish. This delay is probably not necessary.
  delay(1000);
}

void runMotor(bool & init, unsigned long & endTimeHigh, unsigned long & endTimeLow, int Speed, const int pin, bool & isLow) {
  if (init) {   //Back Left
    endTimeHigh = micros() + Speed;   //How many microseconds since startup until we end sending HIGH
    digitalWrite(pin, HIGH);     //Set motor to HIGH
    endTimeLow = 4294967295;        //Crazy big number since we don't yet know when to end sending LOW
    init = false;             //We initialized the back left motor
  }
  else {
    unsigned long t = micros();       //Used for comparason
    if (t > endTimeLow) {           //Done sending LOW
      motorsRunning--;                //This motor is done being signaled
    }
    else if (t > endTimeHigh) {      //Done sending HIGH for PWM wave
      if (!isLow) {
        digitalWrite(pin, LOW);    //Set motor to LOW
        endTimeLow = micros() + Speed; //How many microseconds since startup until we end sending LOW
      }
      isLow = true;   //Make sure we only set to LOW one time
    }
  }
}

void loop()
{
  if (motorsRunning <= 0)
  {
    Usb.Task();
    if (Xbox.XboxReceiverConnected) {
      //This if statement makes sure that the motors will run only when the controller is connected. The motors will stop running when the controller is disconnected. This is the only way to disable the system other than cutting power.
      if (Xbox.Xbox360Connected[controlNum]) {

        //We have to use the "Val" to seperate it from LeftHatX which is a different varible in the library
        //The "LeftHat" is the left joystick
        //"Pre" = Joystick value before maping the values
        int LeftHatXValPre = 0;
        int LeftHatYValPre = 0;

        driving = false;
        shooting = false;

        //This is deadzone detection on the joystick because the resting position of the joystick varries
        if (Xbox.getAnalogHat(LeftHatX, controlNum) > joystickDeadzone || Xbox.getAnalogHat(LeftHatX, controlNum) < -joystickDeadzone) {
          LeftHatXValPre = (Xbox.getAnalogHat(LeftHatX, controlNum));
          driving = true;
          //Serial.println(LeftHatXValPre);
        }
        if (Xbox.getAnalogHat(LeftHatY, controlNum) > joystickDeadzone || Xbox.getAnalogHat(LeftHatY, controlNum) < -joystickDeadzone) {
          LeftHatYValPre = (Xbox.getAnalogHat(LeftHatY, controlNum));
          driving = true;
          //Serial.println(LeftHatYValPre);
        }

        //This section detects which buttons on the Xbox controller are being held and sets the shooter speed (shooterSpeed) based on which button pressed.
        //1460 for no running motors, no buttons held.
        //1600 for low power, "X" button held.
        //1800 for medium power, "A" button held.
        //2000 for high power, "B" button held.
        //topRollerSpeed and bottomRollerSpeed are set opposite because the shooter needs to spin rollers the opposite direction to pull the ball out
        if ((Xbox.getButtonPress(X, controlNum))) {
          topRollerSpeed = 1400;
          bottomRollerSpeed = 1600;
          shooting = true;
          //Serial.println("X button is being held");
        }
        else if ((Xbox.getButtonPress(A, controlNum))) {
          topRollerSpeed = 1300;
          bottomRollerSpeed = 1800;
          shooting = true;
          //Serial.println("A button is being held");
        }
        else if ((Xbox.getButtonPress(B, controlNum))) {
          topRollerSpeed = 1000;
          bottomRollerSpeed = 2000;
          shooting = true;
          //Serial.println("B button is being held");
        }
        else if (!(Xbox.getButtonPress(X, controlNum)) && !(Xbox.getButtonPress(A, controlNum)) && !(Xbox.getButtonPress(B, controlNum))) {
          topRollerSpeed = 1500;
          bottomRollerSpeed = 1500;
          shooting = false;
          //Serial.println("No Shooter buttons are being held");
        }

        //Serial.println(topRollerSpeed);
        //Serial.println(bottomRollerSpeed);

        joyY = map(LeftHatYValPre, -32768, 32768, 1000, 2000);
        joyX = map(LeftHatXValPre, -32768, 32768, -150, 150);

        if (driving) {
          initBackLeft = initFrontLeft = initBackRight = initFrontRight = true;
          motorsRunning += 4;
          backLeftL = frontLeftL = backRightL = frontRightL = false;
        }
        if (shooting) {
          initTopRoller = initBottomRoller = true;
          motorsRunning += 2;
          topRollerL = bottomRollerL = false;
        }
      }
    }
  }

  bool drivingForward = joyY > highNeutral;  //Are we driving?
  bool drivingReverse = joyY < lowNeutral;

  //Serial.println(joyX);
  //Serial.println(joyY);
  //Serial.println(drivingForward);
  //Serial.println(drivingReverse);

  backLeftSpeed = frontLeftSpeed = backRightSpeed = frontRightSpeed = joyY;   //Sets the speed for all motors based on the Forward/Reverse of the joystick

  if (abs(joyX) > 7) {    //Am I moving the joystick left or right?
    if (joyX < 0 && (!drivingForward && !drivingReverse)) {     //Zero point turn Left
      backRightSpeed = highNeutral + abs(joyX);   //highNeutral for forwards movement
      frontRightSpeed = highNeutral + abs(joyX);  //lowNeutral for backwords movement
      backLeftSpeed = lowNeutral + joyX;
      frontLeftSpeed = lowNeutral + joyX;
    }
    else if (joyX > 0 && (!drivingForward && !drivingReverse)) {      //Zero point turn Right
      backRightSpeed = lowNeutral - joyX;
      frontRightSpeed = lowNeutral - joyX;
      backLeftSpeed = highNeutral + joyX;
      frontLeftSpeed = highNeutral + joyX;
    }
    else if (joyX < 0) {       //Turning Left
      backRightSpeed += drivingForward ? abs(joyX) : joyX;
      frontRightSpeed += drivingForward ? abs(joyX) : joyX;
    }
    else if (joyX > 0) {                //Turning Right
      backLeftSpeed += drivingForward ? joyX : -joyX;
      frontLeftSpeed += drivingForward ? joyX : -joyX;
    }
  }

  //This turns on serial diagnostic when the sendToMotor boolean in the user configurable variables is set to false.
  if (!sendToMotor && motorsRunning > 0) {
    Serial.print("bl ="); Serial.println(backLeftSpeed);
    Serial.print("fl ="); Serial.println(frontLeftSpeed);
    Serial.print("br ="); Serial.println(backRightSpeed);
    Serial.print("fr ="); Serial.println(frontRightSpeed);
    motorsRunning = 0;
  }
  if (sendToMotor && motorsRunning > 0) {
    //My owm PWM code because analogWrite 0-255 is not precise enough

    if (shooting) {
      runMotor(initTopRoller, endTimeTopRollerHigh, endTimeTopRollerLow, topRollerSpeed, topRoller, topRollerL);
      runMotor(initBottomRoller, endTimeBottomRollerHigh, endTimeBottomRollerLow, bottomRollerSpeed, bottomRoller, bottomRollerL);
    }

    if (driving) {
      runMotor(initBackLeft, endTimeBackLeftHigh, endTimeBackLeftLow, backLeftSpeed, backLeft, backLeftL);
      runMotor(initFrontLeft, endTimeFrontLeftHigh, endTimeFrontLeftLow, frontLeftSpeed, frontLeft, frontLeftL);
      runMotor(initBackRight, endTimeBackRightHigh, endTimeBackRightLow, backRightSpeed, backRight, backRightL);
      runMotor(initFrontRight, endTimeFrontRightHigh, endTimeFrontRightLow, frontRightSpeed, frontRight, frontRightL);
    }


    /*if (initBackLeft) {   //Back Left
      endTimeBackLeftHigh = micros() + backLeftSpeed;   //How many microseconds since startup until we end sending HIGH
      digitalWrite(backLeft, HIGH);     //Set motor to HIGH
      endTimeBackLeftLow = 4294967295;        //Crazy big number since we don't yet know when to end sending LOW
      initBackLeft = false;             //We initialized the back left motor
      }
      else {
      unsigned long t = micros();       //Used for comparason

      if (t > endTimeBackLeftLow) {           //Done sending LOW
        motorsRunning--;                //This motor is done being signaled
      }
      else if (t > endTimeBackLeftHigh) {      //Done sending HIGH for PWM wave
        if (!backLeftL) {
          digitalWrite(backLeft, LOW);    //Set motor to LOW
          endTimeBackLeftLow = micros() + backLeftSpeed; //How many microseconds since startup until we end sending LOW
        }
        backLeftL = true;   //Make sure we only set to LOW one time
      }
      }

      if (initFrontLeft) {   //Front Left
      endTimeFrontLeftHigh = micros() + frontLeftSpeed;
      digitalWrite(frontLeft, HIGH);
      endTimeFrontLeftLow = 4294967295;
      initFrontLeft = false;
      }
      else {
      unsigned long t = micros();
      if (t > endTimeFrontLeftLow) {
        motorsRunning--;
      }
      else if (t > endTimeFrontLeftHigh) {
        if (!frontLeftL) {
          digitalWrite(frontLeft, LOW);
          endTimeFrontLeftLow = micros() + frontLeftSpeed;
        }
        frontLeftL = true;
      }
      }

      if (initBackRight) {   //Back Right
      endTimeBackRightHigh = micros() + backRightSpeed;
      digitalWrite(backRight, HIGH);
      endTimeBackRightLow = 4294967295;
      initBackRight = false;
      }
      else {
      unsigned long t = micros();
      if (t > endTimeBackRightLow) {
        motorsRunning--;
      }
      else if (t > endTimeBackRightHigh) {
        if (!backRightL) {
          digitalWrite(backRight, LOW);
          endTimeBackRightLow = micros() + backRightSpeed;
        }
        backRightL = true;
      }
      }

      if (initFrontRight) {   //Front Right
      endTimeFrontRightHigh = micros() + frontRightSpeed;
      digitalWrite(frontRight, HIGH);
      endTimeFrontRightLow = 4294967295;
      initFrontRight = false;
      }
      else {
      unsigned long t = micros();
      if (t > endTimeFrontRightLow) {
        motorsRunning--;
      }
      else if (t > endTimeFrontRightHigh) {
        if (!frontRightL) {
          digitalWrite(frontRight, LOW);
          endTimeFrontRightLow = micros() + frontRightSpeed;
        }
        frontRightL = true;
      }
      }

      if (initTopRoller) {   //Top Roller
      endTimeTopRollerHigh = micros() + topRollerSpeed;
      digitalWrite(topRoller, HIGH);
      endTimeTopRollerLow = 4294967295;
      initTopRoller = false;
      }
      else {
      unsigned long t = micros();
      if (t > endTimeTopRollerLow) {
        motorsRunning--;
      }
      else if (t > endTimeTopRollerHigh) {
        if (!topRollerL) {
          digitalWrite(topRoller, LOW);
          endTimeTopRollerLow = micros() + topRollerSpeed;
        }
        topRollerL = true;
      }
      }

      if (initBottomRoller) {   //Bottom Roller
      endTimeBottomRollerHigh = micros() + bottomRollerSpeed;
      digitalWrite(bottomRoller, HIGH);
      endTimeBottomRollerLow = 4294967295;
      initBottomRoller = false;
      }
      else {
      unsigned long t = micros();
      if (t > endTimeBottomRollerLow) {
        motorsRunning--;
      }
      else if (t > endTimeBottomRollerHigh) {
        if (!bottomRollerL) {
          digitalWrite(bottomRoller, LOW);
          endTimeBottomRollerLow = micros() + bottomRollerSpeed;
        }
        bottomRollerL = true;
      }
      }*/
  }
}
