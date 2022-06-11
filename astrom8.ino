#include <WiFi.h>
#include <WebServer.h>
#include <uri/UriBraces.h>

#include <ESP32Servo.h>
#include <ServoEasing.hpp>
#include "html.h"

//Servo mainServo;
ServoEasing  mainServo;

int servo_min_pulse = 500;
int servo_max_pulse = 2500;
int cover_top = 220;
int cover_bottom = -45;
int currentAngle = cover_top;

/* Put your SSID & Password */
const char* ssid = "rifelcon";  // Enter SSID here
const char* password = "rifelcon123";  //Enter Password here

/* Put IP Address details */
IPAddress local_ip(192, 168, 1, 1);
IPAddress gateway(192, 168, 1, 1);
IPAddress subnet(255, 255, 255, 0);

WebServer server(80);

void setup() {
  Serial.begin(115200);

  // setup servo
  ESP32PWM::allocateTimer(0);
  ESP32PWM::allocateTimer(1);
  ESP32PWM::allocateTimer(2);
  ESP32PWM::allocateTimer(3);
//  mainServo.setPeriodHertz(50);    // standard 50 hz servo
//  mainServo.attach(23, servo_min_pulse, servo_max_pulse);
  mainServo.attach(23, servo_min_pulse, servo_max_pulse);

  Serial.println("servo attached");


  // setup server
  WiFi.softAP(ssid, password);
  WiFi.softAPConfig(local_ip, gateway, subnet);
  delay(100);

  server.on("/", handle_OnConnect);
  server.on(UriBraces("/cover/{}"), handle_OnCover);
  server.onNotFound(handle_NotFound);

  server.begin();
  Serial.println("HTTP server started");
}
void loop() {
  server.handleClient();
  delay(100);
}

void handle_OnConnect() {
  server.send(200, "text/html", SendHTML());
}

void handle_OnCover() {
  String angleText = server.pathArg(0);
  int angle = angleText.toInt();
  if (angle == 0) {
    server.send(400, "text/plain", "Invalid angle");
    return;
  }

  if (angle != currentAngle) {
    servoAngle(angle);
    currentAngle = angle;
  }

  server.send(200, "text/plain",  "cover angle: '" + angleText + "'");
}

void handle_NotFound() {
  server.send(404, "text/plain", "Not found");
}

String SendHTML() {
  return INDEX_HTML;
}

void servoAngle(int angle) {
//  int pulse_width_us;
//  float temp_p;
//  //transform angle to pulse width
//  temp_p = ((angle + 45) * 7.407) + 500; //   2000/270 = 7.407
//  pulse_width_us = int(temp_p);
//
//  mainServo.writeMicroseconds(pulse_width_us);
//
//  Serial.print("°   ");
//  Serial.print("pulse_width:");
//  Serial.print(pulse_width_us);
//  Serial.println("uS");
  mainServo.easeTo(angle, 40);
  Serial.print("angle target: ");
  Serial.print(angle);
  Serial.print(" | angle result: ");
  Serial.println(mainServo.getCurrentAngle());
  return;
}
