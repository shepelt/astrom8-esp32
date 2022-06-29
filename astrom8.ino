#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <SPIFFS.h>

#include <ESP32Servo.h>
#include <ServoEasing.hpp>
#include "html.h"

#include <esp32DHT.h>
#define ENVSENSOR_VCC 12
#define ENVSENSOR_IN 32

DHT11 sensor;
float temp;
float humidity;
bool tempAvailable = false;
unsigned long tempTime;

ServoEasing  mainServo;

int servo_min_pulse = 500;
int servo_max_pulse = 2500;
int cover_top = 160;
int cover_bottom = 10;
int timeout = 1000 * 30; // 30 seconds

#define NUM_PORTS  3
int dcPorts[] = {19, 21, 4};
int pwmChannels[] = {2, 3, 4};

// TODO: use eeprom for configuration storage
// TODO: basic HTML UI

/* Put your SSID & Password */
const char* ssid = "rifelcon";  // Enter SSID here
const char* password = "rifelcon123";  //Enter Password here

/* Put IP Address details */
IPAddress local_ip(192, 168, 1, 1);
IPAddress gateway(192, 168, 1, 1);
IPAddress subnet(255, 255, 255, 0);

AsyncWebServer server(80);
bool coverMovementInProgress = false;
unsigned long startTime;

void setup() {
  Serial.begin(115200);

  // env sensor
  // authorize power
  pinMode(ENVSENSOR_VCC, OUTPUT);
  digitalWrite(ENVSENSOR_VCC, HIGH);
  // initialize sensor
  sensor.setup(ENVSENSOR_IN);
  sensor.onData([](float newHumid, float newTemp) {
    temp = newTemp;
    humidity = newHumid;
    tempTime = millis();
    tempAvailable = true;
    Serial.printf("Temp: %.1f°C\nHumid: %.1f%%\n", newTemp, newHumid);
  });
  sensor.onError([](uint8_t error) {
    tempAvailable = false;
    Serial.printf("Error: %d-%s\n", error, sensor.getError());
  });

  // setup PWM port
  for (int i = 0; i < NUM_PORTS; i++) {
    ledcSetup(pwmChannels[i], 5000, 8);
    ledcAttachPin(dcPorts[i], pwmChannels[i]);
  }

  ledcSetup(5, 5000, 8);
  ledcAttachPin(LED_BUILTIN, 5);

  // setup servo
  ESP32PWM::allocateTimer(0);
  ESP32PWM::allocateTimer(1);
  mainServo.attach(23, servo_min_pulse, servo_max_pulse);
  mainServo.write(cover_top);
  mainServo.setTargetPositionReachedHandler(servoTargetPositionReachedHandler);

  Serial.println("servo attached");


  // setup server
  // Initialize SPIFFS
  if (!SPIFFS.begin(true)) {
    Serial.println("An Error has occurred while mounting SPIFFS");
    return;
  }

  WiFi.softAP(ssid, password);
  WiFi.softAPConfig(local_ip, gateway, subnet);
  delay(100);

  // Route for root / web page
  server.on("/", HTTP_GET, [](AsyncWebServerRequest * request) {
    request->send(SPIFFS, "/index.html", String(), false, processor);
  });

  // Route to load style.css file
  server.on("/style.css", HTTP_GET, [](AsyncWebServerRequest * request) {
    request->send(SPIFFS, "/style.css", "text/css");
  });

  server.on("/cover", HTTP_GET, [](AsyncWebServerRequest * request) {
    handleCover(request);
  });

  server.on("/pwm", HTTP_GET, [](AsyncWebServerRequest * request) {
    handlePWM(request);
  });

  server.on("/temp", HTTP_GET, [](AsyncWebServerRequest * request) {
    handleTemp(request);
  });

  server.begin();
  Serial.println("HTTP server started");
}
void loop() {
  // read sensor value
  static uint32_t lastMillis = 0;
  if (millis() - lastMillis > 10000) {
    lastMillis = millis();
    sensor.read();
    Serial.print("Read DHT...\n");
  }
}

// Replaces placeholder with LED state value
String processor(const String& var) {
  Serial.println(var);
  return String();
}

void handleTemp(AsyncWebServerRequest *request) {
  if (!tempAvailable) {
    request->send(503, "text/plain", "Environmental data not available");
    return;
  }

  String elapsed = String(millis() - tempTime);

  String tempText = String(temp, 1);
  String humidText = String(humidity, 1);

  request->send(200, "text/plain", "Temp: " + tempText + " Humidity: " + humidText + " " + elapsed + "ms ago");
}


void handleCover(AsyncWebServerRequest *request) {
  if (coverMovementInProgress) {
    unsigned long elapsedTime = millis() - startTime;
    if (elapsedTime < timeout) {
      request->send(409, "text/plain", "Cover movement already in progress");
      return;
    } else {
      // ignore ongoing movement since timeout
      mainServo.stop();
    }
  }

  // block other requests
  coverMovementInProgress = true;
  startTime = millis();

  String angleText;
  if (request->hasParam("angle")) {
    angleText = request->getParam("angle")->value();
  } else {
    request->send(400, "text/plain", "Invalid request");
    return;
  }
  int angle = angleText.toInt();
  if (angle < cover_bottom || angle > cover_top) {
    request->send(400, "text/plain", "Invalid angle request");
    return;
  }

  Serial.print("starting servo movement to ");
  Serial.println(angle);
  Serial.flush();
  mainServo.startEaseTo(angle, 20, START_UPDATE_BY_INTERRUPT);

  request->send(200, "text/plain", "cover movement started");
}


void handlePWM(AsyncWebServerRequest *request) {
  String pwmText;
  String portText;
  if (request->hasParam("duty")) {
    pwmText = request->getParam("duty")->value();
  } else {
    request->send(400, "text/plain", "Invalid request");
    return;
  }

  if (request->hasParam("port")) {
    portText = request->getParam("port")->value();
  } else {
    request->send(400, "text/plain", "Invalid request");
    return;
  }

  int pwm = pwmText.toInt();
  int port = portText.toInt();

  if (pwm < 0 || pwm > 255) {
    request->send(400, "text/plain", "Invalid request - duty value");
    return;
  }


  if (port != 0 && port != 1 && port != 2) {
    request->send(400, "text/plain", "Invalid request - pwm port");
    return;
  }

  int channel = pwmChannels[port];
  ledcWrite(5, pwm); // FIXME: built in LED (for debugging)
  ledcWrite(channel, pwm);

  Serial.print("pwm port: ");
  Serial.println(portText);
  Serial.print("pwm channel: ");
  Serial.println(channel);
  Serial.print("gpio: ");
  Serial.println(dcPorts[port]);
  Serial.print("pwm target: ");
  Serial.println(pwmText);
  request->send(200, "text/plain", "pwm value: " + pwmText);
}

void servoTargetPositionReachedHandler(ServoEasing *aServoEasingInstance) {
  // servoangle request complete
  unsigned long elapsedTime = millis() - startTime;
  Serial.print("servo movement complete in ");
  Serial.print(elapsedTime);
  Serial.println(" ms");
  Serial.flush();
  coverMovementInProgress = false;
}
