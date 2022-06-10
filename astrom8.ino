#include <WiFi.h>
#include <WebServer.h>
#include <uri/UriBraces.h>

#include <ESP32Servo.h>

Servo mainServo;

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
  mainServo.setPeriodHertz(50);    // standard 50 hz servo
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
  delay(2);
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
  String ptr = "<!DOCTYPE html> <html>\n";
  ptr += "<head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0, user-scalable=no\">\n";
  ptr += "<title>LED Control</title>\n";
  ptr += "<style>html { font-family: Helvetica; display: inline-block; margin: 0px auto; text-align: center;}\n";
  ptr += "body{margin-top: 50px;} h1 {color: #444444;margin: 50px auto 30px;} h3 {color: #444444;margin-bottom: 50px;}\n";
  ptr += ".button {display: block;width: 80px;background-color: #3498db;border: none;color: white;padding: 13px 30px;text-decoration: none;font-size: 25px;margin: 0px auto 35px;cursor: pointer;border-radius: 4px;}\n";
  ptr += ".button-on {background-color: #3498db;}\n";
  ptr += ".button-on:active {background-color: #2980b9;}\n";
  ptr += ".button-off {background-color: #34495e;}\n";
  ptr += ".button-off:active {background-color: #2c3e50;}\n";
  ptr += "p {font-size: 14px;color: #888;margin-bottom: 10px;}\n";
  ptr += "</style>\n";
  ptr += "</head>\n";
  ptr += "<body>\n";
  ptr += "<h1>ESP32 Web Server</h1>\n";
  ptr += "<h3>Using Access Point(AP) Mode</h3>\n";
//
//  if (led1stat)
//  {
//    ptr += "<p>LED1 Status: ON</p><a class=\"button button-off\" href=\"/led1off\">OFF</a>\n";
//  }
//  else
//  {
//    ptr += "<p>LED1 Status: OFF</p><a class=\"button button-on\" href=\"/led1on\">ON</a>\n";
//  }
//
//  if (led2stat)
//  {
//    ptr += "<p>LED2 Status: ON</p><a class=\"button button-off\" href=\"/led2off\">OFF</a>\n";
//  }
//  else
//  {
//    ptr += "<p>LED2 Status: OFF</p><a class=\"button button-on\" href=\"/led2on\">ON</a>\n";
//  }

  ptr += "</body>\n";
  ptr += "</html>\n";
  return ptr;
}

void servoAngle(int angle) {
  int pulse_width_us;
  float temp_p;
  //transform angle to pulse width
  temp_p = ((angle + 45) * 7.407) + 500; //   2000/270 = 7.407
  pulse_width_us = int(temp_p);

  mainServo.writeMicroseconds(pulse_width_us);

  Serial.print("angle: ");
  Serial.print(angle);
  Serial.print("°   ");
  Serial.print("pulse_width:");
  Serial.print(pulse_width_us);
  Serial.println("uS");

  return;
}
