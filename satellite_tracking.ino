#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <AccelStepper.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_LEDBackpack.h>
#include <Sgp4.h>
#include <MD_Parola.h>
#include <MD_MAX72xx.h>
#include <SPI.h>





const char *ssid = "SSID_NAME";
const char *password = "SSID_PASSWORD";

char satnames[20][30] = {"RESOURCESAT-2A", "FENGYUN 1C DEB", 
                               "BEIDOU-3 G1", "ISS (ZARYA) "};
char satURL[20][60] = {
                       "/NORAD/elements/gp.php?CATNR=41877", "/NORAD/elements/gp.php?CATNR=41828",
                       "/NORAD/elements/gp.php?CATNR=43683", "/NORAD/elements/gp.php?CATNR=25544"};
                       

char TLE1[20][70];
char TLE2[20][70];
long upcomingPasses[20];
int numSats = 4;
float minPassElevation = 10.0;
float myLat = 27.11;
float myLong = 75.57;
float myAlt = 457;
const int timeZone = +5.30;

#define AZmotorPin1 D0
#define AZmotorPin2 D1
#define AZmotorPin3 D2
#define AZmotorPin4 D3
#define AZLimit D4

#define ELmotorPin1 D5
#define ELmotorPin2 D6
#define ELmotorPin3 D7
#define ELmotorPin4 D8
#define ELLimit D9


float oneTurn = 4096;

int SAT;
int nextSat;
int AZstart;
long passEnd;
int satVIS;
int year;
int mon;
int day;
int hr;
int minute;
double sec;
int today;
long nextpassEpoch;
long nextpassEndEpoch;
long passDuration;

Sgp4 sat;

int satAZsteps;
int satELsteps;
int turns = 0;
int passStatus = 0;

AccelStepper stepperAZ(AccelStepper::HALF4WIRE, AZmotorPin1, AZmotorPin3, AZmotorPin2, AZmotorPin4);
AccelStepper stepperEL(AccelStepper::HALF4WIRE, ELmotorPin1, ELmotorPin3, ELmotorPin2, ELmotorPin4);

bool initpredpoint(unsigned long _timeNow, float minElevation) 
{
  passinfo overpass;                      //structure to store overpass info
  sat.initpredpoint( _timeNow , minElevation);     //finds the startpoint

  bool error;

  error = sat.nextpass(&overpass, 20);    //search for the next overpass, if there are more than 20 maximums below the horizon it returns false
  delay(0);

  if ( error == 1) { //no error, prints overpass information
    nextpassEpoch = (overpass.jdstart - 2440587.5) * 86400;
    nextpassEndEpoch = (overpass.jdstop - 2440587.5) * 86400;
    passDuration = nextpassEndEpoch - nextpassEpoch;
    //if(passDuration < 300){
    //   Predict(nextpassEndEpoch);
    //}
    AZstart = overpass.azstart;
    invjday(overpass.jdstart , timeZone , true , year, mon, day, hr, minute, sec); // Convert Julian date to print in serial.
    Serial.println("Next pass for: " + String(satnames[SAT]) + " In: " + String(nextpassEpoch - _timeNow) + " Duration:" + String(passDuration));
    Serial.println("Start: az=" + String(overpass.azstart) + "° " + String(hr) + ':' + String(minute) + ':' + String(sec));
  } else {
    Serial.println("Prediction error");
    while (true);
  }
  delay(0);

  return nextpassEpoch;
}

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org");

Adafruit_8x8matrix matrix = Adafruit_8x8matrix();


unsigned long getTime()
{
  timeClient.update();
  return timeClient.getEpochTime();
}

int nextSatPass(long _nextpassEpoch[20])
{
  for (int i = 0; i < numSats; ++i)
  {
    if (_nextpassEpoch[0] - getTime() >= _nextpassEpoch[i] - getTime())
    {
      _nextpassEpoch[0] = _nextpassEpoch[i];
      nextSat = i;
    }
  }
  return nextSat;
}

void initMotors()
{
  stepperEL.setMaxSpeed(1000);
  stepperEL.setCurrentPosition(0);
  stepperEL.setAcceleration(100);
  stepperAZ.setMaxSpeed(300);
  stepperAZ.setCurrentPosition(0);
  stepperAZ.setAcceleration(100);

  // Zeroing code remains unchanged

  delay(1000);

}

void clearMatrix()
{
  matrix.clear();
  matrix.writeDisplay();
}

void setup()
{
  pinMode(ELLimit, INPUT_PULLUP);
  pinMode(AZLimit, INPUT_PULLUP);
  Serial.begin(9600);
  matrix.begin(0x70); // Address of your LED matrix
  matrix.setBrightness(15); // Adjust brightness as needed
  matrix.setTextWrap(false);
  matrix.setTextColor(LED_ON);
  matrix.setTextSize(1);

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(2000);
    Serial.println("Connecting to WiFi...");
    matrix.println("Connecting to WiFi...");
  }
  Serial.println("Connected to WiFi!");
  matrix.println("Connected to WiFi!");

  clearMatrix();
  matrix.println("Fetching time...");
  timeClient.begin();
  while (!timeClient.update())
  {
    timeClient.forceUpdate();
  }
  Serial.println("Unixtime: " + String(getTime()));
  matrix.println("Unixtime: " + String(getTime()));

  Serial.println("Zeroing");
  clearMatrix();
  matrix.println("Zeroing...");
  initMotors();

  HTTPClient http;
  sat.site(myLat, myLong, myAlt);
  for (SAT = 0; SAT < numSats; SAT++)
  {
    Serial.println("Request #: " + String(SAT) + " For: " + String(satnames[SAT]));
    HTTPClient http;
    WiFiClient client;
    String completeURL =( "https://celestrak.org" , satURL[SAT]); // Concatenate strings to form the complete URL
    http.begin(client, completeURL);
    int httpCode = http.GET(); // Send the HTTP GET request
    String TLE;

    if (httpCode == HTTP_CODE_OK)
    {
      TLE = http.getString();
      TLE.substring(26, 96).toCharArray(TLE1[SAT], 70);
      TLE.substring(97, 167).toCharArray(TLE2[SAT], 70);
      Serial.println(TLE1[SAT]);
      Serial.println(TLE2[SAT]);

      sat.init(satnames[SAT], TLE1[SAT], TLE2[SAT]);
      upcomingPasses[SAT] = sat.initpredpoint(getTime(), minPassElevation);
    }
    else
    {
      Serial.println("Failed to fetch TLE data for " + String(satnames[SAT]));
    }
    delay(100);
  }
  http.end();

  nextSat = nextSatPass(upcomingPasses);
  Serial.println("Next satellite: " + String(nextSat));
  sat.init(satnames[nextSat], TLE1[nextSat], TLE2[nextSat]);
  sat.initpredpoint(getTime(), 0.0);
}

String printHHmmss(unsigned long _secondsIn)
{
  unsigned int _hour = floor(_secondsIn / 60 / 60);
  unsigned int _minute = floor((_secondsIn / 60) % 60);
  unsigned int _seconds = _secondsIn % 60;
  char _DateAndTimeString[20];
  sprintf(_DateAndTimeString, "%02d:%02d:%02d", _hour, _minute, _seconds);
  return _DateAndTimeString;
}

void loop()
{
  sat.findsat(getTime());
  satAZsteps = -round(sat.satAz * oneTurn / 360);
  satELsteps = -round(sat.satEl * oneTurn / 360);

  invjday(sat.satJd, timeZone, true, year, mon, day, hr, minute, sec);
  Serial.println("\nLocal time: " + String(day) + '/' + String(mon) + '/' + String(year) + ' ' + String(hr) + ':' + String(minute) + ':' + String(sec));
  Serial.println("azimuth = " + String(sat.satAz) + " elevation = " + String(sat.satEl) + " distance = " + String(sat.satDist));
  Serial.println("latitude = " + String(sat.satLat) + " longitude = " + String(sat.satLon) + " altitude = " + String(sat.satAlt));
  Serial.println("AZsteps = " + String(satAZsteps));

  if (nextpassEpoch - getTime() < 300 && nextpassEpoch + 5 - getTime() > 0)
  {
    Serial.println("Status: Pre-pass");
    Serial.println("Next satellite is #: " + String(satnames[nextSat]) + " in: " + String(nextpassEpoch - getTime()));
    clearMatrix();
    matrix.print(satnames[nextSat]);
    matrix.writeDisplay();
    delay(1000);
    matrix.setCursor(0, 1);
    matrix.print("AZ:" + String(int(sat.satAz)) + " EL:" + String(int(sat.satEl)));
    matrix.writeDisplay();
    prepass();
  }
  else if (sat.satVis != -2)
  {
    Serial.println("Status: In pass");
    clearMatrix();
    matrix.print(satnames[nextSat]);
    unsigned int timeToLOS = nextpassEndEpoch - getTime();
    unsigned int _minute = floor((timeToLOS / 60) % 60);
    unsigned int _seconds = timeToLOS % 60;
    char DateAndTimeString[20];
    sprintf(DateAndTimeString, "%02d:%02d", _minute, _seconds);
    matrix.print(" LOS");
    matrix.print(DateAndTimeString);
    matrix.writeDisplay();
    delay(1000);
    matrix.setCursor(0, 1);
    matrix.print("AZ:" + String(int(sat.satAz)) + " EL:" + String(int(sat.satEl)));
    matrix.writeDisplay();
    inPass();
  }
  else if (getTime() - passEnd < 60)
  {
    Serial.println("Status: Post-pass");
    clearMatrix();
    matrix.print("Post-pass:" + String(passEnd + 60 - getTime()));
    matrix.writeDisplay();
    postpass();
  }
  else if (sat.satVis == -2)
  {
    Serial.println("Status: Standby");
    Serial.println("Next satellite is: " + String(satnames[nextSat]) + " in: " + String(nextpassEpoch - getTime()));
    clearMatrix();
    matrix.print(satnames[nextSat]);
    matrix.writeDisplay();
    delay(1000);
    matrix.setCursor(0, 1);
    matrix.print("AZ:" + String(int(sat.satAz)) + " EL:" + String(int(sat.satEl)));
    matrix.writeDisplay();
    standby();
  }
  delay(500);
}

void standby()
{
  stepperAZ.runToNewPosition(0);
  stepperEL.runToNewPosition(-280);

  digitalWrite(AZmotorPin1, LOW);
  digitalWrite(AZmotorPin2, LOW);
  digitalWrite(AZmotorPin3, LOW);
  digitalWrite(AZmotorPin4, LOW);

  digitalWrite(ELmotorPin1, LOW);
  digitalWrite(ELmotorPin2, LOW);
  digitalWrite(ELmotorPin3, LOW);
  digitalWrite(ELmotorPin4, LOW);
}

void prepass()
{
  if (AZstart < 360 && AZstart > 180)
  {
    AZstart = AZstart - 360;
  }
  stepperAZ.runToNewPosition(-AZstart * oneTurn / 360);
  stepperEL.runToNewPosition(0);

  digitalWrite(AZmotorPin1, LOW);
  digitalWrite(AZmotorPin2, LOW);
  digitalWrite(AZmotorPin3, LOW);
  digitalWrite(AZmotorPin4, LOW);

  digitalWrite(ELmotorPin1, LOW);
  digitalWrite(ELmotorPin2, LOW);
  digitalWrite(ELmotorPin3, LOW);
  digitalWrite(ELmotorPin4, LOW);
}

void inPass()
{
  if (AZstart > 0)
  {
    satAZsteps = satAZsteps - oneTurn;
  }
  if (satAZsteps - stepperAZ.currentPosition() > 100)
  {
    stepperAZ.setCurrentPosition(stepperAZ.currentPosition() + oneTurn);
    turns--;
  }
  if (satAZsteps - stepperAZ.currentPosition() < -100)
  {
    stepperAZ.setCurrentPosition(stepperAZ.currentPosition() - oneTurn);
    turns++;
  }
  stepperAZ.runToNewPosition(satAZsteps);
  stepperEL.runToNewPosition(satELsteps);
  passEnd = getTime();
}

void postpass()
{
  Serial.println("Post pass time left: " + String(passEnd + 60 - getTime()));
  if (getTime() - passEnd > 30)
  {
    if (turns > 0)
    {
      stepperAZ.setCurrentPosition(stepperAZ.currentPosition() + oneTurn);
      turns--;
    }
    if (turns < 0)
    {
      stepperAZ.setCurrentPosition(stepperAZ.currentPosition() - oneTurn);
      turns++;
    }
  }
  if (passStatus == 1 && getTime() - passEnd > 50)
  {
    for (SAT = 0; SAT < numSats; SAT++)
    {
      sat.init(satnames[SAT], TLE1[SAT], TLE2[SAT]);
      upcomingPasses[SAT] = sat.initpredpoint(getTime() + 200, minPassElevation);
      Serial.println("Next pass for Satellite #: " + String(SAT) + " in: " + String(upcomingPasses[SAT] - getTime()));
    }
    nextSat = nextSatPass(upcomingPasses);
    sat.init(satnames[nextSat], TLE1[nextSat], TLE2[nextSat]);
    sat.initpredpoint(getTime(), 0.0);
    passStatus = 0;
  }
}
