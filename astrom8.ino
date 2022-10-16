#include <Preferences.h>
Preferences prefs;

#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <SPIFFS.h>

#include <ESP32Servo.h>
#include <ServoEasing.hpp>
#include "html.h"

#include <Wire.h>
#include <Adafruit_INA219.h>
#define I2C_SDA 27
#define I2C_SCL 26

#include <esp32DHT.h>
#define ENVSENSOR_VCC 12
#define ENVSENSOR_IN 32

TwoWire INA219WIRE = TwoWire(0);
Adafruit_INA219 ina219;

DHT11 sensor;
float temp;
float humidity;
bool tempAvailable = false;
unsigned long tempTime;

ServoEasing  mainServo;

int servo_min_pulse = 500;
int servo_max_pulse = 2500;
int cover_top = 170;
int cover_bottom = 10;
int timeout = 1000 * 30; // 30 seconds

#define NUM_PORTS  3
int dcPorts[] = {19, 21, 4};
int pwmChannels[] = {2, 3, 4};
int pwmValues[] = {0, 0, 0};

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

  // load configuration
  processConfig();


  // env sensor
  // authorize power
  pinMode(ENVSENSOR_VCC, OUTPUT);
  digitalWrite(ENVSENSOR_VCC, HIGH);


  // initialize current/voltage sensor (INA219)
  INA219WIRE.begin(I2C_SDA, I2C_SCL);
  ina219.begin(&INA219WIRE);

  // initialize temp sensor
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


  // initialize PWM values from config
  for (int i = 0; i < NUM_PORTS; i++) {
    int channel = pwmChannels[i];
    int pwm = pwmValues[i];
    ledcWrite(channel, pwm);
  }

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

  server.on("/status", HTTP_GET, [](AsyncWebServerRequest * request) {
    handleStatus(request);
  });

  server.begin();
  Serial.println("HTTP server started");
}

void processConfig() {
  Serial.println("Loading config");
  prefs.begin("astrom8");

  size_t pwmLen = prefs.getBytesLength("pwm");
  char buffer[pwmLen]; // prepare a buffer for the data
  prefs.getBytes("pwm", buffer, pwmLen);

  if (pwmLen % sizeof(pwmValues)) {
    // invalid size
    return;
  }

  int* pwmValuesLoaded = (int*) buffer;
  for (int i = 0; i < NUM_PORTS; i++) {
    int pwm = pwmValuesLoaded[i];
    if (pwm < 0 || pwm > 255) {
      // set to 0
      pwm = 0;
    }
    pwmValues[i] = pwm;

    Serial.print("PWM "); Serial.println(pwm);
  }
}

void loop() {
  // read sensor value
  static uint32_t lastMillis = 0;
  if (millis() - lastMillis > 10000) {
    lastMillis = millis();


    Serial.println("<Cover>");
    Serial.print("Angle      :   "); Serial.print(mainServo.read()); Serial.println(" degrees");

    Serial.println("<PWM>");
    Serial.print("PWM 1      :   "); Serial.print(pwmValues[0]); Serial.println(" / 255");
    Serial.print("PWM 2      :   "); Serial.print(pwmValues[1]); Serial.println(" / 255");
    Serial.print("PWM 3      :   "); Serial.print(pwmValues[2]); Serial.println(" / 255");




    Serial.println("<Environment>");

    sensor.read();

    // INA219
    float shuntvoltage = 0;
    float busvoltage = 0;
    float current_mA = 0;
    float loadvoltage = 0;
    float power_mW = 0;

    shuntvoltage = ina219.getShuntVoltage_mV();
    busvoltage = ina219.getBusVoltage_V();
    current_mA = abs(ina219.getCurrent_mA());
    power_mW = ina219.getPower_mW();
    loadvoltage = busvoltage + (shuntvoltage / 1000);

    Serial.println("<Power>");
    Serial.print("Bus Voltage:   "); Serial.print(busvoltage); Serial.println(" V");
    Serial.print("Shunt Voltage: "); Serial.print(shuntvoltage); Serial.println(" mV");
    Serial.print("Load Voltage:  "); Serial.print(loadvoltage); Serial.println(" V");
    Serial.print("Current:       "); Serial.print(current_mA); Serial.println(" mA");
    Serial.print("Power:         "); Serial.print(power_mW); Serial.println(" mW");
    Serial.println("");

  }
}

// Replaces placeholder with LED state value
String processor(const String& var) {
  Serial.println(var);
  return String();
}

void handleStatus(AsyncWebServerRequest *request) {
  String pwm1 = String(pwmValues[0]);
  String pwm2 = String(pwmValues[1]);
  String pwm3 = String(pwmValues[2]);

  request->send(200, "text/plain", "PWM: " + pwm1 + " " + pwm2 + " " + pwm3 + "\n" + "Cover: " + mainServo.read());
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
  pwmValues[port] = pwm;
  // save pwm values
  prefs.putBytes("pwm", pwmValues, sizeof(pwmValues));



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
