/*
ultrasonic_sensor_demo
-a demo code of how to read the ultrasonic sensor 

By Gabriel Staples
http://www.ElectricRCAircraftGuy.com 
-click "Contact me" at the top of my website to find my email address 
Started: 8 May 2017 
Updated: 8 May 2017 

Helpful References:
-https://www.arduino.cc/en/Tutorial/Smoothing

Required Libraries:
- http://playground.arduino.cc/Code/NewPing - this library works VERY WELL to read the ultrasonic sensors, but is UNNECESSARILY BLOCKING and UNNECESSARILY USES TIMER2. One of these days I'll fix this by writing my own library that relies on Pin Change interrupts instead, but DOESN'T USE ANY TIMERS and IS NOT BLOCKING. 
- https://github.com/ElectricRCAircraftGuy/eRCaGuy_GetMedian - self-explanatory name; is required to implement an external "moving window" type median filter 

*/

//Library includes:
#include <NewPing.h>
#include <eRCaGuy_GetMedian.h> //for finding median values 

//Ultrasonic range-finder "ping" sensor 
const byte TRIGGER_PIN = A5;
const byte ECHO_PIN = A4;
const unsigned int MAX_DISTANCE = 500; //cm; max distance we want to ping for. Maximum sensor distance is rated at 400~500cm.
const float MICROSECONDS_PER_INCH = 74.6422; //us per inch; sound travelling through air at sea level ~GS
const unsigned int DESIRED_PING_PD = 39000; //us; this is the desired sample period; NB: THIS MUST *NOT* BE TOO SHORT, OR ELSE THE PING SENSOR READINGS BECOME *REALLY* UNSTABLE AND LOUSY! See the NewPing library, for instance, which uses a value of 29000us (29ms) (see v1.8 of NewPing.h, line 159) as their fixed sample period when doing their median filter! Also see this datasheet here, "C:\Gabe\Gabe - RC-extra\Arduino\Products (Hardware)\Sensors & Dataloggers (input only)\Distance\Ultrasonic Rangefinder\HC-SR04Users_Manual - Google Docs%%%%%.pdf", which shows in the diagram on pg. 7 that a pulse of 38ms corresponds to "no obstacle". Therefore, let's use a DESIRED_SAMPLE_PD just over 38ms (39ms is good). Note: p. 7 also says "Please make sure the surface of object to be detect should have at least 0.5 meter^2 for better performance." This corresponds to an object ~0.7 x 0.7 m *minimum* in order to provide an adequate reflection surface for best measurements. 
#define UNK_DIST NO_ECHO //alias for NO_ECHO, meaning the sensor is too far away from the measured surface 

NewPing sonar(TRIGGER_PIN, ECHO_PIN, MAX_DISTANCE); //create NewPing object 

//------------------------------------------------------------------------------
//setup
//------------------------------------------------------------------------------
void setup()
{
  Serial.begin(115200);
  Serial.println("\nBegin\n");
}

//------------------------------------------------------------------------------
//loop
//------------------------------------------------------------------------------
void loop()
{
  
  //Do some basic Digital Signal Processing (DSP):
  //-Get a new distance reading as fast as possible, and load it into a "moving window" raw data buffer; we will then find the median of the data in that buffer and load the median value into a "moving window" median value buffer; lastly, we will come up with a moving average "smoothed" value (which now has increased resolution) by averaging all values in the median value buffer, and we will use that average as the value to output. 
  
  //Raw data sample buffer 
  const byte PING_RAW_BUF_LEN = 5; 
  static unsigned int pingRawBuffer[PING_RAW_BUF_LEN];
  
  //Median data sample buffer 
  const byte PING_MEDIAN_BUF_LEN = 5; //NB: *I think* that the smoothed *response freq* (not sample freq) = sampleFreq/(PING_MEDIAN_BUF_LEN), so for a sample freq of 1/39ms = 25.64 Hz, and a PING_MEDIAN_BUF_LEN of 5, we get: 25.64/5 = 25.64 samples/sec / 5 samples = 5.1Hz frequency response
  static unsigned int pingMedianBuffer[PING_MEDIAN_BUF_LEN];
  
  //initialize the buffers
  static bool buffersInitialized = false;
  if (buffersInitialized==false)
  {
    buffersInitialized = true; //update 
    for (byte i=0; i<PING_RAW_BUF_LEN; i++)
      pingRawBuffer[i] = 0;
    for (byte i=0; i<PING_MEDIAN_BUF_LEN; i++)
      pingMedianBuffer[i] = 0;
  }
  
  //For smoothing the median samples 
  static byte pingMedBuf_i = 0; //pingMedianBuffer index 
  static unsigned long pingMedTotal = 0; //the running total 
  static unsigned int pingMedAvg = 0; //the average 
  
  //Take raw samples no faster than the desired max sample freq
  static unsigned long tStartPing = micros(); //us; timestamp 
  unsigned long tNow = micros(); //us 
  unsigned long dt = tNow - tStartPing; //us 
  if (dt >= DESIRED_PING_PD)
  {
    tStartPing = tNow; //us; update 
    unsigned int tPing = sonar.ping(); //us; ping time; note: a 0 returned means the distance is outside of the set distance range (MAX_DISTANCE)
    
    //load new value into moving window pingRawBuffer 
    static byte pingBuf_i = 0; //pingRawBuffer index 
    pingRawBuffer[pingBuf_i] = tPing; //us 
    pingBuf_i++;
    if (pingBuf_i>=PING_RAW_BUF_LEN)
      pingBuf_i = 0; //reset 
    
    //find the median and do the data smoothing
    
    //copy pingRawBuffer, sort the copy in place, & find the median
    unsigned int pingRawBuffer_cpy[PING_RAW_BUF_LEN];
    for (byte i=0; i<PING_RAW_BUF_LEN; i++)
    {
      pingRawBuffer_cpy[i] = pingRawBuffer[i];
    }
    unsigned int pingMedian = Median.getMedian(pingRawBuffer_cpy, PING_RAW_BUF_LEN); //us; median ping time

    //median data smoothing (see: https://www.arduino.cc/en/Tutorial/Smoothing)
    pingMedTotal -= pingMedianBuffer[pingMedBuf_i]; //subtract the last reading 
    pingMedianBuffer[pingMedBuf_i] = pingMedian; //us; load a new reading into the buffer 
    pingMedTotal += pingMedianBuffer[pingMedBuf_i]; //add the reading to the total 
    //advance to the next position in the array 
    pingMedBuf_i++; 
    if (pingMedBuf_i>=PING_MEDIAN_BUF_LEN)
      pingMedBuf_i = 0; //reset 
    //calculate the average median 
    pingMedAvg = pingMedTotal/PING_MEDIAN_BUF_LEN; //us; average of the values in the median buffer
    
    //Convert the us avg to mm distance 
    unsigned int dist_mm = microsecondsToMm(pingMedAvg);
    
    //For Serial Plotting 
    Serial.print(microsecondsToMm(tPing)); Serial.print(", "); //mm; raw sample 
    Serial.print(microsecondsToMm(pingMedian)); Serial.print(", "); //mm; median sample 
    Serial.println(dist_mm); //mm; avg median sample 
  }
} //end of loop 

//------------------------------------------------------------------------------
//microsecondsToMm
//------------------------------------------------------------------------------
unsigned int microsecondsToMm(unsigned int us)
{
  return (unsigned int)((float)us/MICROSECONDS_PER_INCH/2.0*25.4 + 0.5); //mm; distance; divide by 2 since the ultrasonic pressure wave must travel there AND back, which is 2x the distance you are measuring; do +0.5 to round to the nearest integer during truncation from float to unsigned int 
}
