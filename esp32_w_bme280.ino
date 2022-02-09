//=*=*=*= function summary =*=*=*=*=*=*=*=*=*
// This device measures the outside temperature, humidity and air pressure.
// And the device sends the data to ambient server every minute.
// If the temperature reaches below freezing, the device notifies you of the value every 30min via LINE NOTIFY.

//=*=*=*= set up information for real circuit =*=*=*=*=*=*=*=*=*
//you need to use AE-BME280 w solder-brige on J1, J2, J3
//J1 and J2 mean pull-up resister for I2C communication.
//J3 is selector for I2C.
//
// BME280 -> ESP32
// VDD -> 3V3
// GND -> GND
// CSB -> 3V3
// SDI -> D21
// SDO -> GND
// SCK -> D22

//=*=*=*= set up information for Arduino IDE =*=*=*=*=*=*=*=*=*
// - Download Board Manager : https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
// - Set Board Manager : ESP32 Dev Module
// - Install 2 libraries : "BME280" by Tyler Glenn, "Ambient ESP32 ESP8266 lib" by Ambient Data
// - create credentials.h in C:\Users\username\Documents\Arduino\libraries\_private (when you use ArduinoIDE WindowsApp ver.)
// 		#ifndef CREDENTIALS_H
// 		#define CREDENTIALS_H
// 		// Wifi parameters
// 		const char *WiFi_SSID = "your SSID";
// 		const char *WiFi_PASSWORD = "your password";
// 		// ambient
// 		unsigned int channelId = 123456; //your channelId
// 		const char *writeKey = "your writekey";
// 		// lineNotify
// 		const char* lineApiToken = "your lineApiToken";
// 		const char* host = "notify-api.line.me";
// 		const String url = "/api/notify";
// 		const int httpsPort = 443;
// 		#endif

//=*=*=*= include library =*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*
#include <time.h>
#include <Wire.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <BME280I2C.h>
#include <Ambient.h>
#include <credentials.h> // put the file in 

//=*=*=*= define constant value =*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*
#define LINE_SEND_CYCLE 30 // if freezing, notify you of the data every 30 minutes.
#define NOTIFY_TEMP 0 // this temp is freezing temp.
#define IGNORE_INTERVAL_MINUTES 720 // if more than 12h and below freezing, LINE NOTIFY when it first detects the temp.

//=*=*=*= set global variable =*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*
unsigned int sendCount = 0; // count the device send the date to ambient server
unsigned int count12h = IGNORE_INTERVAL_MINUTES - 1;
boolean testFlag = true;
float minTemp = NOTIFY_TEMP;
unsigned startTime;
String message;
BME280I2C bme;
Ambient ambient;
WiFiClient client;

//=*=*=*= define func =*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*
void lineNotify(String message){
	WiFiClientSecure clientSecure;
	Serial.print("connecting to ");
	Serial.print(host); // from credentials.h
	clientSecure.setInsecure();
	if(!clientSecure.connect(host, httpsPort)){
		Serial.println("LINE server connection failed.");
		return;
	}
	Serial.print("requesting URL: ");
	Serial.println(url); // from credentials.h
	String lineMessage = "message=" + message;
	String header = "POST " + url + " HTTP/1.1\r\n" +
        "HOST: " + host + "\r\n" +
        "User-Agent: ESP32\r\n" +
        "Authorization: Bearer " + lineApiToken + "\r\n" +
        "Connection: close\r\n" +
        "Content-Type: application/x-www-form-urlencoded\r\n" +
        "Content-Length: " + lineMessage.length() + "\r\n";
	clientSecure.print(header); // request to LINE server
	clientSecure.print("\r\n");
	clientSecure.print(lineMessage + "\r\n");
	Serial.print("request sent: ");
	Serial.println(lineMessage);
	String res = clientSecure.readStringUntil('\n');
	Serial.print("response: ");
	Serial.println(res);
	Serial.print("\r\n");
}

//=*=*=*= main =*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*
void setup() {
	// set up serial communication and sensor
	Serial.begin(115200);
	while(!Serial);
	Wire.begin();
	while(!bme.begin()){
		Serial.println("Could not find BME280 sensor!");
		delay(5000);
	}
	// connect WiFi
	WiFi.begin(WiFi_SSID, WiFi_PASSWORD);
	while(WiFi.status()!=WL_CONNECTED){
		delay(500);
		Serial.print(".");
	}
	Serial.println("WiFi connected!!");
	Serial.print("IP address: ");
	Serial.println(WiFi.localIP());
	// set up ambient
	ambient.begin(channelId, writeKey, &client);
}

void loop() {
	startTime = millis();
	// measure temperature, humidity and air pressure.
	float temp = bme.temp();
	float hum = bme.hum();
	float pres = bme.pres();
	Serial.printf("temp: %.1f, humid: %.1f, press: %.2f\r\n", temp, hum, pres);
	// send the data to Ambient
	ambient.set(1, temp);
	ambient.set(2, hum);
	ambient.set(3, pres);
	ambient.send();
	// if first loop, confirm LINE NOTIFY
	if(testFlag == true){
		message = "LINE message test." + String(temp, 1) + "℃.";
		lineNotify(message);
		testFlag = false;
	}
	//if below freezing, update minimum temperature
	if(temp < minTemp){
		minTemp = temp;
	}
	//if more than 12 hours and below freezing, then lineNotify
	if((count12h >= IGNORE_INTERVAL_MINUTES) & (minTemp < NOTIFY_TEMP)){
		message = "外気温が" + String(minTemp, 1) + "℃ になりました！";
		lineNotify(message);
		count12h = 0;
		sendCount = 0;
	}else{
		count12h++;
	}
	//if below freezing, lineNotify every 30 min from first notify
	if(sendCount == (LINE_SEND_CYCLE - 1)){
		sendCount = 0;
		if(minTemp < NOTIFY_TEMP){
			message = "氷点下継続中. " + String(minTemp, 1) + "℃.";
			lineNotify(message);
			minTemp = NOTIFY_TEMP;
		}
	}else{
		sendCount++;
	}
	// measure 1 min
	uint64_t delayTime = 60 * 1000 - (millis() - startTime);
	delay(delayTime);
}