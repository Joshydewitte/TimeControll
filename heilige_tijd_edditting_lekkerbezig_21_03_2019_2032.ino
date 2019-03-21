// Arduino Timezone Library Copyright (C) 2018 by Jack Christensen and
// licensed under GNU GPL v3.0, https://www.gnu.org/licenses/gpl.html
//
// Self-adjusting clock for time zones.
// Jack Christensen Mar 2012
//
// Editted by Jacob Bartholomé Christiaan Steenbergen Mar 2019
// Arduino gets ntp time and updates it every minute (good for cheduled tasks)

#include <SPI.h>
#include <Ethernet.h>
#include <Time.h>
#include <Timezone.h>
#include <EthernetUdp.h>


//CET Time Zones
// Central European Time (Amsterdam)
TimeChangeRule CEST = {"CEST", Last, Sun, Mar, 2, 120};     // Central European Summer Time
TimeChangeRule CET = {"CET ", Last, Sun, Oct, 3, 60};       // Central European Standard Time
Timezone CE(CEST, CET);
Timezone* timezones[] = { &CE  };
Timezone* tz;                   //pointer to the time zone
uint8_t tzIndex;                //indexes the timezones[] array
TimeChangeRule* tcr;            //pointer to the time change rule, use to get TZ abbrev
time_t utc;
time_t local;

// Enter a MAC address for your controller below.
// Newer Ethernet shields have a MAC address printed on a sticker on the shield
byte mac[] = {
  0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0x18
};
unsigned int localPort = 8888;       // local port to listen for UDP packets
const char timeServer[] = "0.nl.pool.ntp.org"; //nl.pool.ntp.org NTP server //dont abuse
const int NTP_PACKET_SIZE = 48; // NTP time stamp is in the first 48 bytes of the message

long previousMillis = 0;
long previousMillis1 = 0;
long previousMillis2 = 0;

unsigned long epoch;

const long ReplyBuffer = 1000;
const long TimeRequestBuffer = 60000;

boolean FirstTimeCheck = true;
boolean ParseCheck = false;

byte packetBuffer[NTP_PACKET_SIZE]; //buffer to hold incoming and outgoing packets
// A UDP instance to let us send and receive packets over UDP
EthernetUDP Udp;

void setup()
{
  // set the system time to UTC
  // warning: assumes that compileTime() returns US EST
  // adjust the following line accordingly if you're in another time zone
  //setTime(compileTime() + 0 * 60 );
  Serial.begin(9600);
  tz = timezones[tzIndex];
  setTime(compileTime() - 60 * 60);
  // start Ethernet and UDP
  if (Ethernet.begin(mac) == 0) {
    Serial.println("Failed to configure Ethernet using DHCP");
    // Check for Ethernet hardware present
    if (Ethernet.hardwareStatus() == EthernetNoHardware) {
      Serial.println("Ethernet shield was not found.  Sorry, can't run without hardware. :(");
    } else if (Ethernet.linkStatus() == LinkOFF) {
      Serial.println("Ethernet cable is not connected.");
    }
    while (!Serial) {
      ;
    }
  }
  Udp.begin(localPort);
}


void printDateTime(Timezone tz, time_t utc, const char *descr)
{
  char buf[40];
  char m[4];    // temporary storage for month string (DateStrings.cpp uses shared buffer)
  TimeChangeRule *tcr;        // pointer to the time change rule, use to get the TZ abbrev
  time_t t = tz.toLocal(utc, &tcr);
  strcpy(m, monthShortStr(month(t)));
  sprintf(buf, "%.2d:%.2d:%.2d %s %.2d %s %d %s",
          hour(t), minute(t), second(t), dayShortStr(weekday(t)), day(t), m, year(t), tcr -> abbrev);
  Serial.print(buf);
  Serial.print(' ');
  Serial.println(descr);
  epoch = epoch + 1; //counts 1second eachtime this void is called = (once per second) epoch correct it automaticly

  
  //////////////////////////////////////////////////////////////
  ////optional you can put your desired time in this statement to trigger code at that time
  //if ((hour(t)) == (20) && (minute(t)) == (29) && (second(t)) == (15))
  //{
  //  Serial.println("This Time Has Come");
  //}
  ////////////////////////////////////////////////////////////////

  
}

void loop()
{
  unsigned long currentMillis = millis();
  // send an NTP packet to a time server with 10seconds in between
  if ((currentMillis - previousMillis >= TimeRequestBuffer) || (FirstTimeCheck))
  {
    FirstTimeCheck = false;
    sendNTPpacket(timeServer);
    previousMillis = currentMillis;
    if (!ParseCheck)
    {
      Serial.println("\tupdated arduino time with ntp server");
    }
  }
  // wait to see if a reply is available
  if (Udp.parsePacket()) {
    ParseCheck = false;
    previousMillis2 = currentMillis;
    // We've received a packet, read the data from it
    Udp.read(packetBuffer, NTP_PACKET_SIZE); // read the packet into the buffer
    // the timestamp starts at byte 40 of the received packet and is four bytes,
    // or two words, long. First, extract the two words:
    unsigned long highWord = word(packetBuffer[40], packetBuffer[41]);
    unsigned long lowWord = word(packetBuffer[42], packetBuffer[43]);
    // combine the four bytes (two words) into a long integer
    // this is NTP time (seconds since Jan 1 1900):
    unsigned long secsSince1900 = highWord << 16 | lowWord;
    //Serial.print("Seconds since Jan 1 1900 = ");
    //Serial.println(secsSince1900);
    // now convert NTP time into everyday time:
    //Serial.println();
    //Serial.print("Unix time = ");
    // Unix time starts on Jan 1 1970. In seconds, that's 2208988800:
    // subtract seventy years:
    const unsigned long seventyYears = 2208988800UL;
    epoch = secsSince1900 - seventyYears;
    // print Unix time:
    //Serial.println(epoch);
    // print the time if it's changed
    static time_t tLast;
    Ethernet.maintain();
  }
  if ((currentMillis - previousMillis1 >= ReplyBuffer) && (!ParseCheck)) //call date/time void every second
  {
    previousMillis1 = currentMillis;
    time_t utc = epoch;
    printDateTime(CE, utc, "Amsterdam");
  }
  //if there isn't a response after 15 seconds it is called then notify once
  if ((currentMillis - previousMillis2  >= TimeRequestBuffer + 15000) && (!ParseCheck) && (!Udp.parsePacket()))
  {
    Serial.println("\tcouldn't parse Packet. Got internet?");
    ParseCheck = true;
  }

  //  if ((t(hour)) == (200200))
  // {
  //   Serial.println("");
  //  }


}


// function to return the compile date and time as a time_t value
time_t compileTime()
{
  const time_t FUDGE(10);    //fudge factor to allow for upload time, etc. (seconds, YMMV)
  const char *compDate = __DATE__, *compTime = __TIME__, *months = "JanFebMarAprMayJunJulAugSepOctNovDec";
  char compMon[3], *m;
  strncpy(compMon, compDate, 3);
  compMon[3] = '\0';
  m = strstr(months, compMon);
  tmElements_t tm;
  tm.Month = ((m - months) / 3 + 1);
  tm.Day = atoi(compDate + 4);
  tm.Year = atoi(compDate + 7) - 1970;
  tm.Hour = atoi(compTime);
  tm.Minute = atoi(compTime + 3);
  tm.Second = atoi(compTime + 6);
  time_t t = makeTime(tm);
  return t + FUDGE;        //add fudge factor to allow for compile time
}

// send an NTP request to the time server at the given address
void sendNTPpacket(const char * address) {
  // set all bytes in the buffer to 0
  memset(packetBuffer, 0, NTP_PACKET_SIZE);
  // Initialize values needed to form NTP request
  // (see URL above for details on the packets)
  packetBuffer[0] = 0b11100011;   // LI, Version, Mode
  packetBuffer[1] = 0;     // Stratum, or type of clock
  packetBuffer[2] = 6;     // Polling Interval
  packetBuffer[3] = 0xEC;  // Peer Clock Precision
  // 8 bytes of zero for Root Delay & Root Dispersion
  packetBuffer[12]  = 49;
  packetBuffer[13]  = 0x4E;
  packetBuffer[14]  = 49;
  packetBuffer[15]  = 52;
  // all NTP fields have been given values, now
  // you can send a packet requesting a timestamp:
  Udp.beginPacket(address, 123); // NTP requests are to port 123
  Udp.write(packetBuffer, NTP_PACKET_SIZE);
  Udp.endPacket();
}
