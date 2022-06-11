const char INDEX_HTML[] PROGMEM = R"=====(
<!DOCTYPE html> 
<html>
   <head>
      <meta name="viewport" content="width=device-width, initial-scale=1.0, user-scalable=no">
      <title>LED Control</title>
      <style>html { font-family: Helvetica; display: inline-block; margin: 0px auto; text-align: center;}
         body{margin-top: 50px;} h1 {color: #444444;margin: 50px auto 30px;} h3 {color: #444444;margin-bottom: 50px;}
         .button {display: block;width: 80px;background-color: #3498db;border: none;color: white;padding: 13px 30px;text-decoration: none;font-size: 25px;margin: 0px auto 35px;cursor: pointer;border-radius: 4px;}
         .button-on {background-color: #3498db;}
         .button-on:active {background-color: #2980b9;}
         .button-off {background-color: #34495e;}
         .button-off:active {background-color: #2c3e50;}
         p {font-size: 14px;color: #888;margin-bottom: 10px;}
      </style>
   </head>
   <body>
      <h1>ESP32 Web Server</h1>
      <h3>Using Access Point(AP) Mode</h3>
   </body>
</html>
)=====";
