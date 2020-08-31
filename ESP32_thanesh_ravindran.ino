/*
   THANESH RAVINDRAN - 012018022619
   FINAL YEAR PROJECT - AIRAPP
   NODEMCU ESP32 & CCS811
*/
#include <WiFi.h> // WiFi lib
#include <NTPClient.h> // ntp time client
#include <WiFiUdp.h> / udp conn
#include <IOXhop_FirebaseESP32.h> // Firebase lib
#include <Wire.h> // I2C lib
#include "ccs811.h" // CCS811 lib

// Connect to Firebase
#define FIREBASE_HOST "" // link of your firebase database
#define FIREBASE_AUTH "" // firebase secret key
#define WIFI_SSID "" // your wifi ssid
#define WIFI_PASSWORD "" // wifi password

// Get board internal temperature reading
#ifdef __cplusplus
extern "C" {
#endif
uint8_t temprature_sens_read();
#ifdef __cplusplus
}
#endif
uint8_t temprature_sens_read();

// CCS811 nWAKE wiring (D23)
CCS811 ccs811(23);

// Define NTP Client to get time
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP);
String formattedDate;

void setup() {
  // Enable serial
  Serial.begin(9600);
  // Connect to wifi.
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting..");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(500);
  }
  Serial.println();
  Serial.print("Connected: ");
  Serial.println(WiFi.localIP());
  Firebase.begin(FIREBASE_HOST, FIREBASE_AUTH);

  // Enable I2C
  Wire.begin();
  // Enable CCS811
  bool ok = ccs811.begin();
  if ( !ok ) Serial.println("setup: CCS811 begin FAILED");
  // Start measuring
  ok = ccs811.start(CCS811_MODE_1SEC);
  if ( !ok ) Serial.println("setup: CCS811 start FAILED");

  // Initialize a NTPClient to get time
  timeClient.begin();
  // Malaysia timezone: GMT +8 = 28800
  timeClient.setTimeOffset(28800);
}

void loop() {
  // Read mesurements
  float temp = (temprature_sens_read() - 32) / 1.8;
  uint16_t eco2, etvoc, errstat, raw;
  ccs811.read(&eco2, &etvoc, &errstat, &raw);

  // get time
  while (!timeClient.update()) {
    timeClient.forceUpdate();
  }
  formattedDate = timeClient.getFormattedDate();

  // Print measurement based on status
  if ( errstat == CCS811_ERRSTAT_OK ) {
    Serial.print("CCS811: ");
    Serial.print("tvoc="); Serial.print(etvoc);    Serial.print(" ppb  ");
    Serial.print("eco2=");  Serial.print(eco2);     Serial.print(" ppm  ");
    Serial.print("temp="); Serial.print(temp);    Serial.print(" .C  ");

    // Push Real-Time measurements to Firebase
    Serial.println("Data uploading..");
    Firebase.setFloat("/AirApp/TVOC/", etvoc);
    Firebase.setFloat("/AirApp/eCO2/", eco2);
    Firebase.setFloat("/AirApp/Temperature/", temp);

    // Push measurements for storage to Firebase
    Serial.println("Pushing data to storage..");
    StaticJsonBuffer<200> jsonBuffer;
    JsonObject& value = jsonBuffer.createObject();
    value["TVOC"] = etvoc;
    value["eCO2"] = eco2;
    value["Int_temp"] = temp;
    value["Time"] = formattedDate;
    String name = Firebase.push("/AirApp/Log/", value);

    // If Firebase upload failed..
    if (Firebase.failed()) {
      Serial.print("Failed to upload data. Error: " + Firebase.error());
      delay(2000);
      return;
    }
    else {
      Serial.println("All Done!");
      Serial.println();
    }
    // If reading measurement failed..
  } else if ( errstat == CCS811_ERRSTAT_OK_NODATA ) {
    Serial.println("CCS811: waiting for (new) data");
  } else if ( errstat & CCS811_ERRSTAT_I2CFAIL ) {
    Serial.println("CCS811: I2C error");
  } else {
    Serial.print("CCS811: errstat="); Serial.print(errstat, HEX);
    Serial.print("="); Serial.println( ccs811.errstat_str(errstat) );
  }

  // Wait
  delay(2000);
}
