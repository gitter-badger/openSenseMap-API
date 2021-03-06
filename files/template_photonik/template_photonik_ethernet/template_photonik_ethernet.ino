/*
 * Jan Wirwahn, Institute for Geoinformatics, Uni Muenster
 * July 2015
 * Arduino Web-Client for pushing senseBox:Home measurements
 * to the OpenSenseMap server.
 */

#include <BMP280.h>
#include <Wire.h>
#include <HDC100X.h>
#include <SPI.h>
#include <Ethernet.h>
#include <Makerblog_TSL45315.h>

#define UV_ADDR 0x38
#define IT_1   0x1

//INDIVIDUAL SETUP
IPAddress ip(192, 168, 179, 194); //IP Adress
IPAddress dn(192, 168, 179, 10);  //DNS Server
IPAddress gw(192, 168, 179, 254); //Gateway
byte mac[] = { 0x90, 0xA2, 0xDA, 0x0D, 0x86, 0x46 }; //MAC Adress

char server[] = "opensensemap.org";
EthernetClient client;

//IDs are being generated by the OSM server
//If you want to update your old sketch, insert the IDs from your old file
//senseBox ID

//Sensor IDs


BMP280 bmp;
HDC100X HDC(0x43);
//Makerblog_TSL45315 luxsensor = Makerblog_TSL45315(TSL45315_TIME_M4);

//measurement variables
float temperature = 0;
float humidity = 0;
double tempBaro, pressure;
uint32_t lux;
int messTyp;
byte msb=0, lsb=0;
uint16_t uv;
float uvIndex;

//post sample each minute
int postInterval = 60000;
long timeOld = 0;
long timeNew = 0;


void setup()
{
 Serial.begin(9600);
//Try DHCP first

  Ethernet.begin(mac, ip, dn, gw);
  HDC.begin(HDC100X_TEMP_HUMI,HDC100X_14BIT,HDC100X_14BIT,DISABLE);
  //Serial.println(luxsensor.begin());
  if(!bmp.begin()){
    Serial.println("BMP init failed!");
    while(1);
  }
  else Serial.println("BMP init success!");
  bmp.setOversampling(4);
  Wire.begin();
  Wire.beginTransmission(UV_ADDR);
  Wire.write((IT_1<<2) | 0x02);
  Wire.endTransmission();
  delay(500);
}

void loop()
{
  timeNew = millis();
  if (timeNew - timeOld > postInterval)
  {
    //-----Pressure-----//
    messTyp = 1;
    char result = bmp.startMeasurment();
    if(result!=0){
      delay(result);
      result = bmp.getTemperatureAndPressure(tempBaro,pressure);
      pressure = pressure * 100;
      postObservation(pressure, PRESSURESENSOR_ID, SENSEBOX_ID);
    }
    //-----Humidity-----//
    messTyp = 2;
    humidity = HDC.getHumi();
    postObservation(humidity, HUMIDITYSENSOR_ID, SENSEBOX_ID);
    //-----Temperature-----//
    messTyp = 2;
    temperature = HDC.getTemp();
    postObservation(temperature, TEMPERATURESENSOR_ID, SENSEBOX_ID);
    //-----Lux-----//
    messTyp = 1;
    //lux = luxsensor.readLux();
    //Serial.print("Illumi = "); Serial.println(lux);
    //postObservation(lux, LUXSENSOR_ID, SENSEBOX_ID);
    //-----UV-Index-----//
    messTyp = 2;
    Wire.requestFrom(UV_ADDR+1, 1); //MSB
    delay(1);
    if(Wire.available())
      msb = Wire.read();
    Wire.requestFrom(UV_ADDR+0, 1); //LSB
    delay(1);
    if(Wire.available())
      lsb = Wire.read();
    uv = (msb<<8) | lsb;
    uvIndex = uv * 5.625;

    postObservation(uvIndex, UVSENSOR_ID, SENSEBOX_ID);

    timeOld = millis();
  }
}

void postObservation(float measurement, String sensorId, String boxId)
{
  char obs[10];
  if (messTyp == 1) dtostrf(measurement, 5, 0, obs);
  else if (messTyp == 2) dtostrf(measurement, 5, 2, obs);
  Serial.println(obs);
  //json must look like: {"value":"12.5"}
  //post observation to: http://opensensemap.org:8000/boxes/boxId/sensorId
  Serial.println("connecting...");
  String value = "{\"value\":";
  value += obs;
  value += "}";
  // if you get a connection, report back via serial:
  boxId += "/";
  if (client.connect(server, 8000))
  {
    Serial.println("connected");
    // Make a HTTP Post request:
    client.print("POST /boxes/");
    client.print(boxId);
    client.print("/");
    client.print(sensorId);
    client.println(" HTTP/1.1");
    // Send the required header parameters
    client.println("Host:opensensemap.org");
    client.println("Content-Type: application/json");
    client.println("Connection: close");
    client.print("Content-Length: ");
    client.println(value.length());
    client.println();
    client.print(value);
    client.println();
  }
  waitForResponse();
}

void waitForResponse()
{
  // if there are incoming bytes available
  // from the server, read them and print them:
  boolean repeat = true;
  do{
    if (client.available())
    {
      char c = client.read();
      Serial.print(c);
    }
    // if the servers disconnected, stop the client:
    if (!client.connected())
    {
      Serial.println();
      Serial.println("disconnecting.");
      client.stop();
      repeat = false;
    }
  }
  while (repeat);
}
