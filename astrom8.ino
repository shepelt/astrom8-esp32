#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <SPIFFS.h>

#include <ESP32Servo.h>
#include <ServoEasing.hpp>
#include "html.h"

//Servo mainServo;
ServoEasing mainServo;

int servo_min_pulse = 500;
int servo_max_pulse = 2500;
int cover_top = 170;
int cover_bottom = 10;
int currentAngle = cover_top;
int timeout = 1000 * 30;  // 30 seconds
unsigned long startTime;

/* Put your SSID & Password */
const char *ssid = "rifelcon";         // Enter SSID here
const char *password = "rifelcon123";  //Enter Password here
const char *remote_ssid = "roofwifi_hidden";
const char *remote_password = NULL;


/* Put IP Address details */
IPAddress local_ip(192, 168, 1, 1);
IPAddress gateway(192, 168, 1, 1);
IPAddress subnet(255, 255, 255, 0);

AsyncWebServer server(80);
bool coverMovementInProgress = false;

void setup() {
  Serial.begin(115200);
  startTime = millis();

  // setup PWM port
  ledcSetup(2, 5000, 8);
  ledcAttachPin(26, 2);
  ledcSetup(3, 5000, 8);
  ledcAttachPin(LED_BUILTIN, 3);

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

  // Route for root /00 web page
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
      request->send(200, "text/plain", "Astrom8 mini HTTP server");
  });

  server.on("/cover", HTTP_GET, [](AsyncWebServerRequest *request) {
    handleCover(request);
  });

  server.on("/pwm", HTTP_GET, [](AsyncWebServerRequest *request) {
    handlePWM(request);
  });

  server.begin();
  Serial.println("HTTP server started");
}


void loop() {

}

// Replaces placeholder with LED state value
String processor(const String &var) {
  Serial.println(var);
  return String();
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
  if (angle == 0) {
    request->send(400, "text/plain", "Invalid request");
    return;
  }

  Serial.print("starting servo movement to ");
  Serial.println(angle);
  Serial.flush();
  mainServo.startEaseTo(angle, 15, START_UPDATE_BY_INTERRUPT);

  currentAngle = angle;

  request->send(200, "text/plain", "cover movement started");
}


void handlePWM(AsyncWebServerRequest *request) {
  String pwmText;
  if (request->hasParam("duty")) {
    pwmText = request->getParam("duty")->value();
  } else {
    request->send(400, "text/plain", "Invalid request");
    return;
  }
  int pwm = pwmText.toInt();

  // FIXME: sanity check
  ledcWrite(2, pwm);
  ledcWrite(3, pwm);
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
