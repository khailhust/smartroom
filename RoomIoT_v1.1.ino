/*************************************************************************************************************************************/
/* Fill-in information from Blynk Device Info here */
#define BLYNK_TEMPLATE_ID ""
#define BLYNK_TEMPLATE_NAME ""
#define BLYNK_AUTH_TOKEN "W2WtIv9GgTzd3a4BjNafViXnvGeoEDiT"

#include <ESP8266WiFi.h>
#include <BlynkSimpleEsp8266.h>
#include "DHT.h"
#include <LiquidCrystal_I2C.h>
#include <Wire.h>
#include <PZEM004Tv30.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <EEPROM.h>
// #include <ESP8266HTTPClient.h>
#include "HTTPSRedirect.h"
#include <WiFiClientSecure.h>

/* Comment this out to disable prints and save space */
#define BLYNK_PRINT Serial

#define Tx D5 //--->Rx pzem
#define Rx D6 //--->Tx pzem

const int DHTPIN = D3;     // Đọc dữ liệu từ DHT11
const int DHTTYPE = DHT11; // Khai báo loại cảm biến, có 2 loại là DHT11 và DHT22

// Your WiFi credentials.
// Set password to "" for open networks.
const char *ssid1 = "Khai's Room";
const char *password1 = "14051998";

const char *ssid2 = "Queen House";
const char *password2 = "0936136513Trangbtht@";

// ThingSpeakSetting
const int channelID = 2634930; // Enter Channel ID
String writeAPIKey = "FF9RE6FLCNZDKG16";
const char *server = "api.thingspeak.com";

// Google Sheet Setting
const String googleScriptID = "AKfycbwHnmk-6AHTF_BfVRU8CJDHVUInDUzxsopmppC9FuITBYYeeJ1tpNV5KpcFqPqESouE"; // Thay bằng Google Script ID của bạn
const char *host = "script.google.com";
String payload_base = "{\"command\": \"insert_row\", \"sheet_name\": \"Sheet1\", \"values\": ";
const int httpsPort = 443;
String payload = "";

SoftwareSerial port(Rx, Tx);
PZEM004Tv30 pzem(port);
LiquidCrystal_I2C lcd(0x27, 16, 2); // Check I2C address of LCD, normally 0x27 or 0x3F
DHT dht(DHTPIN, DHTTYPE);
WiFiClient client;
HTTPSRedirect *clientHttp = nullptr;

// Define NTP Client to get time
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org");

BlynkTimer timer1;
BlynkTimer timer2;
BlynkTimer timer3;

int ledStatus = 0;

float temp = 0;
float hum = 0;
float voltage = 0;
float current = 0;
float power = 0;
float energy = 0;
float frequency = 0;
float pf;

float csHomqua, csHomtruoc, csThangroi, csThangtruoc;
int ngayChotSo = 1;
int gioChotSo = 0;
int phutChotSo = 0;

int currentHour = 0;
int currentMinute = 0;
int monthDay = 0;
int currentMonth = 0;
int currentYear = 0;

boolean resetE = false;
boolean savedata = false;
int ledState = LOW;

unsigned long times = millis();
// Week Days
String weekDays[7] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};
// Month names
String months[12] = {"January", "February", "March", "April", "May", "June", "July", "August", "September", "October", "November", "December"};

/*************************************************************************************************************************************/
// FunctionDeclare
void wifiSetup();
void connectToWiFi();
void httpPostToThinkSpeak();
void sendDataToBlynk();
void readSensorData();
void readPzem();
void readDHTSensor();
void displayLCDScreen();
void updatRealTime();
void sendDataToGoogleSheet();

/***********************************************************MAIN**********************************************************************/
void setup()
{
  // Debug console
  Serial.begin(115200);
  dht.begin();
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(D0, OUTPUT); // Back Light LCD
  digitalWrite(LED_BUILTIN, HIGH);
  // wifiSetup();
  connectToWiFi();
  EEPROM.begin(512);

  // Blynk.begin(BLYNK_AUTH_TOKEN, ssid, pass);
  // You can also specify server:
  Blynk.begin(BLYNK_AUTH_TOKEN, ssid1, password1, "blynk-server.com", 8080);
  // Blynk.begin(BLYNK_AUTH_TOKEN, ssid, pass, IPAddress(192,168,1,100), 8080);

  // Initialize a NTPClient to get time
  timeClient.begin();
  // Set offset time in seconds to adjust for your timezone, for example: GMT +1 = 3600, GMT +8 = 28800, GMT -1 = -3600, GMT 0 = 0
  timeClient.setTimeOffset(7 * 3600);

  clientHttp = new HTTPSRedirect(httpsPort);
  clientHttp->setInsecure();
  clientHttp->setPrintResponseBody(false);
  clientHttp->setContentTypeHeader("application/json");

  // Setup a function to be called every second
  timer1.setInterval(1000L, readSensorData);
  timer1.setInterval(2000L, updatRealTime);
  timer1.setInterval(60000L, saveData);
  timer2.setInterval(1000L, sendDataToBlynk);
  timer2.setInterval(15000L, httpPostToThinkSpeak);

  lcd.init();
  lcd.backlight();

  readChiSo();
}

void loop()
{
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi disconnected! Reconnecting...");
    connectToWiFi();
  }

  Blynk.run();
  timer1.run();
  timer2.run();

  if (ledStatus)
  {
    if (millis() - times > 1000)
    {
      times = millis();
      if (ledState == LOW)
      {
        ledState = HIGH;
      }
      else
      {
        ledState = LOW;
      }
      digitalWrite(LED_BUILTIN, ledState);
    }
  }

  if (resetE == true)
  {
    resetE = false;
    pzem.resetEnergy();
    for (int i = 0; i < 16; ++i)
    {
      EEPROM.write(i, 0);
      // delay(10);
      // digitalWrite(led,!digitalRead(led));
    }
    EEPROM.commit();
    readChiSo();
  }

  if (savedata == true)
  {
    savedata = false;
    writeChiSo();
  }
}

/*************************************************************************************************************************************/
BLYNK_WRITE(V4)
{
  ledStatus = param.asInt();
}

BLYNK_WRITE(V5)
{
  int backLightLCD = param.asInt();
  if (backLightLCD)
  {
    digitalWrite(D0, HIGH);
  }
  else
  {
    digitalWrite(D0, LOW);
  }
}

// BLYNK_WRITE(V10){
//   ngayChotSo = param.asInt();
// }

// This function is called every time the device is connected to the Blynk.Cloud
BLYNK_CONNECTED()
{
  Blynk.syncAll();
}
/*************************************************************************************************************************************/
void wifiSetup()
{
  Serial.print("Connecting");
  WiFi.begin(ssid1, password1);
  while (WiFi.status() != WL_CONNECTED)
  {
    Serial.print(".");
    delay(100);
  }
  Serial.print("\r\nWiFi connected");
}

void connectToWiFi() {
  Serial.println("Connecting to WiFi...");

  WiFi.begin(ssid1, password1); // Thử kết nối với mạng Wi-Fi 1

  unsigned long startTime = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - startTime < 10000) {
    delay(500);
    Serial.print(".");
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("Connected to WiFi 1");
  } else {
    Serial.println("Failed to connect to WiFi 1. Trying WiFi 2...");
    WiFi.begin(ssid2, password2); // Thử kết nối với mạng Wi-Fi 2

    startTime = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - startTime < 10000) {
      delay(500);
      Serial.print(".");
    }

    if (WiFi.status() == WL_CONNECTED) {
      Serial.println("Connected to WiFi 2");
    } else {
      Serial.println("Failed to connect to both WiFi networks");
    }
  }
}

void updatRealTime()
{
  timeClient.update();

  time_t epochTime = timeClient.getEpochTime();
  // Serial.print("Epoch Time: ");
  // Serial.println(epochTime);

  String formattedTime = timeClient.getFormattedTime();
  Serial.print("Formatted Time: ");
  Serial.println(formattedTime);

  currentHour = timeClient.getHours();
  // Serial.print("Hour: ");
  // Serial.println(currentHour);

  currentMinute = timeClient.getMinutes();
  // Serial.print("Minutes: ");
  // Serial.println(currentMinute);

  // int currentSecond = timeClient.getSeconds();
  // Serial.print("Seconds: ");
  // Serial.println(currentSecond);

  // String weekDay = weekDays[timeClient.getDay()];
  // Serial.print("Week Day: ");
  // Serial.println(weekDay);

  // Get a time structure
  struct tm *ptm = gmtime((time_t *)&epochTime);

  monthDay = ptm->tm_mday;
  // Serial.print("Month day: ");
  // Serial.println(monthDay);

  currentMonth = ptm->tm_mon + 1;
  // Serial.print("Month: ");
  // Serial.println(currentMonth);

  // String currentMonthName = months[currentMonth - 1];
  // Serial.print("Month name: ");
  // Serial.println(currentMonthName);

  currentYear = ptm->tm_year + 1900;
  // Serial.print("Year: ");
  // Serial.println(currentYear);

  // Print complete date:
  String currentDate = String(currentYear) + "-" + String(currentMonth) + "-" + String(monthDay);
  Serial.print("Current date: ");
  Serial.println(currentDate);

  Serial.println("");
}

void displayLCDScreen()
{
  // lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Nhiet do: " + String(temp));
  lcd.setCursor(0, 1);
  lcd.print("Do Am: " + String(hum));
}

void httpPostToThinkSpeak()
{
  if (client.connect(server, 80))
  {
    String body = "field1=" + String(temp, 1) +
                  "&field2=" + String(hum, 1) +
                  "&field3=" + String(voltage, 1) +
                  "&field4=" + String(current, 1) +
                  "&field5=" + String(power, 1) +
                  "&field6=" + String(energy, 1) +
                  "&field7=" + String(frequency, 1) +
                  "&field8=" + String(pf, 1);
    client.print("POST /update HTTP/1.1\n");
    client.print("Host: api.thingspeak.com\n");
    client.print("Connection: close\n");
    client.print("X-THINGSPEAKAPIKEY: " + writeAPIKey + "\n");
    client.print("Content-Type: application/x-www-form-urlencoded\n");
    client.print("Content-Length: ");
    client.print(body.length());
    client.print("\n\n");
    client.print(body);
    client.print("\n\n");

    Serial.print("Post To ThinkSpeak: ");
    Serial.println(body);
    Serial.println();
    // String answer = getResponse();
  }
  else
  {
    Serial.println("Connection Failed");
  }
}

// This function sends Arduino's uptime every second to Virtual Pin 2.
void sendDataToBlynk()
{
  int timeInSeconds = millis() / 1000;

  int seconds = timeInSeconds % 60;
  timeInSeconds /= 60;
  int minutes = timeInSeconds % 60;
  timeInSeconds /= 60;
  int hours = timeInSeconds % 24;
  int days = timeInSeconds / 24;

  String time = String(days) + "D:" + String(hours) + "H:" + String(minutes) + "M:" + String(seconds) + "S";

  Blynk.virtualWrite(V0, temp);
  Blynk.virtualWrite(V1, hum);
  // Blynk.virtualWrite(V2, millis() / 1000);
  Blynk.virtualWrite(V3, time);
  Blynk.virtualWrite(V6, voltage);
  Blynk.virtualWrite(V7, current);
  Blynk.virtualWrite(V8, power);
  Blynk.virtualWrite(V9, energy);
  Blynk.virtualWrite(V10, frequency);
  Blynk.virtualWrite(V11, pf);

  Blynk.virtualWrite(V12, energy - csHomqua);
  Blynk.virtualWrite(V13, csHomqua - csHomtruoc);
  Blynk.virtualWrite(V14, energy - csThangroi);
  Blynk.virtualWrite(V15, csThangroi - csThangtruoc);

  displayLCDScreen();

  Serial.print(time);
  Serial.print(" Nhiet do: ");
  Serial.print(temp);
  Serial.print(" Do am: ");
  Serial.println(hum);
  Serial.println();
}

void sendDataToGoogleSheet()
{
  if (clientHttp != nullptr)
  {
    if (!clientHttp->connected())
    {
      clientHttp->connect(host, httpsPort);
    }
  }
  else
  {
    Serial.println("Error creating client object!");
    // error_count = 5;
  }

  String url = String("/macros/s/") + googleScriptID + "/exec"; // neu dung trang tinh khac thì thay GScriptId thành GAS_ID

  String localTime = String(currentYear) + "/" + String(currentMonth) + "/" + String(monthDay) + " " + String(currentHour) + ":" + String(currentMinute);
  String values = localTime + "," + String(energy);
  payload = payload_base + "\"" + values + "\"}";

  // Publish data to Google Sheets
  Serial.println("Publishing data...");
  Serial.println(payload);
  if (clientHttp->POST(url, host, payload))
  {
    // do stuff here if publish was successful
    Serial.println("Publish was successful");
  }
  else
  {
    // do stuff here if publish was not successful
    Serial.println("Error while connecting");
  }
}

void readSensorData()
{
  readDHTSensor();
  readPzem();
}

void readDHTSensor()
{
  temp = dht.readTemperature();
  hum = dht.readHumidity();
}

void readPzem()
{
  voltage = pzem.voltage();
  if (!isnan(voltage))
  {
    // Serial.print("Voltage: "); Serial.print(voltage); Serial.println("V");
  }
  current = pzem.current();
  if (!isnan(current))
  {
    // Serial.print("Current: "); Serial.print(current); Serial.println("A");
  }
  power = pzem.power();
  if (!isnan(power))
  {
    // Serial.print("Power: "); Serial.print(power); Serial.println("W");
  }
  energy = pzem.energy();
  if (!isnan(energy))
  {
    // Serial.print("Energy: "); Serial.print(energy,3); Serial.println("kWh");
  }
  else
  {
    Serial.println("Error reading energy");
  }

  frequency = pzem.frequency();
  if (!isnan(frequency))
  {
    // Serial.print("Frequency: "); Serial.print(frequency, 1); Serial.println("Hz");
  }
  pf = pzem.pf();
  if (!isnan(pf))
  {
    // Serial.print("PF: "); Serial.println(pf);
  }
  // Serial.println();
}

//-------------Ghi dữ liệu kiểu float vào bộ nhớ EEPROM----------------------//
float readFloat(unsigned int addr)
{
  union
  {
    byte b[4];
    float f;
  } data;
  for (int i = 0; i < 4; i++)
  {
    data.b[i] = EEPROM.read(addr + i);
  }
  return data.f;
}
void writeFloat(unsigned int addr, float x)
{
  union
  {
    byte b[4];
    float f;
  } data;
  data.f = x;
  for (int i = 0; i < 4; i++)
  {
    EEPROM.write(addr + i, data.b[i]);
  }
}
void readChiSo()
{
  csHomqua = readFloat(0);
  if(isnan(csHomqua)){
    csHomqua = 0;
  }

  csHomtruoc = readFloat(4);
  if(isnan(csHomtruoc)){
    csHomtruoc = 0;
  }

  csThangroi = readFloat(8);
  if(isnan(csThangroi)){
    csThangroi = 0;
  }

  csThangtruoc = readFloat(12);
  if(isnan(csThangtruoc)){
    csThangtruoc = 0;
  }

  Serial.print("Chỉ số hôm qua: ");
  Serial.println(csHomqua);
  Serial.print("Chỉ số hôm trước: ");
  Serial.println(csHomtruoc);
  Serial.print("Chỉ số tháng rồi: ");
  Serial.println(csThangroi);
  Serial.print("Chỉ số tháng trước: ");
  Serial.println(csThangtruoc);
}
void saveData()
{
  savedata = true;
  // sendDataToGoogleSheet(); // Test
}
void writeChiSo()
{
  updatRealTime();
  if (currentHour == gioChotSo && currentMinute == phutChotSo)
  {
    Serial.println("Ghi số ngày mới!");
    writeFloat(4, csHomqua);
    writeFloat(0, energy);
    sendDataToGoogleSheet();

    if (monthDay == ngayChotSo)
    {
      Serial.println("Ghi số tháng mới!");
      writeFloat(12, csThangroi);
      writeFloat(8, energy);
    }
    EEPROM.commit();
    readChiSo();
  }
}
/*************************************************************************************************************************************/