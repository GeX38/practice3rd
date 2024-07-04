#include <WiFi.h>
#include <WebServer.h>
#include <Servo.h>
#include "esp_camera.h"

// Настройки камеры
#define CAMERA_MODEL_AI_THINKER
#include "camera_pins.h"

// Создаем объекты для сервоприводов и веб-сервера
Servo servoHorizontal;
Servo servo1;
Servo servo2;
WebServer server(80);

// Устанавливаем пины для управления мотором через H-мост
const int motorPin1 = 26; // GPIO26
const int motorPin2 = 27; // GPIO27

// Устанавливаем пины для сервоприводов на ESP32
const int servoPinHorizontal = 14; // GPIO14 для горизонтального сервопривода
const int servoPin1 = 13;          // GPIO13 для первого дополнительного сервопривода
const int servoPin2 = 12;          // GPIO12 для второго дополнительного сервопривода

// Устанавливаем SSID и пароль для WiFi
const char* ssid = "Your_SSID";
const char* password = "Your_PASSWORD";

// Переменные для текущих значений углов сервоприводов и скорости мотора
int currentServoAngleHorizontal = 90;
int currentServoAngle1 = 90;
int currentServoAngle2 = 90;
int currentMotorSpeed = 0;

void setup() {
  // Присоединяем сервоприводы к пинам
  servoHorizontal.attach(servoPinHorizontal);
  servo1.attach(servoPin1);
  servo2.attach(servoPin2);
  
  // Устанавливаем начальные углы сервоприводов
  servoHorizontal.write(currentServoAngleHorizontal);
  servo1.write(currentServoAngle1);
  servo2.write(currentServoAngle2);

  // Настройка пинов мотора
  pinMode(motorPin1, OUTPUT);
  pinMode(motorPin2, OUTPUT);

  // Настройка серийного порта
  Serial.begin(115200);
  delay(10);

  // Подключение к WiFi сети
  Serial.println();
  Serial.println();
  Serial.print("Подключение к ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi подключено");
  Serial.println("IP адрес: ");
  Serial.println(WiFi.localIP());

  // Настройки камеры
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;

  if(psramFound()){
    config.frame_size = FRAMESIZE_UXGA;
    config.jpeg_quality = 10;
    config.fb_count = 2;
  } else {
    config.frame_size = FRAMESIZE_SVGA;
    config.jpeg_quality = 12;
    config.fb_count = 1;
  }
  
  // Инициализация камеры
  if (esp_camera_init(&config) != ESP_OK) {
    Serial.println("Ошибка инициализации камеры");
    return;
  }


  server.on("/", handleRoot);

 
  server.on("/joystick", handleJoystick);

  // Определяем обработчик для получения изображения с камеры
  server.on("/capture", HTTP_GET, handleCapture);


  server.on("/slider1", handleSlider1);
  server.on("/slider2", handleSlider2);


  server.begin();
  Serial.println("HTTP сервер запущен");
}

void loop() {

  server.handleClient();
}

void handleRoot() {

  String html = "<!DOCTYPE html>\
                  <html>\
                  <body>\
                    <h1>Управление сервоприводами, мотором и просмотр камеры</h1>\
                    <canvas id=\"joystick\" width=\"200\" height=\"200\"></canvas>\
                    <img id=\"camera\" src=\"/capture\" width=\"320\" height=\"240\" />\
                    <br>\
                    <label>Сервопривод 1:</label>\
                    <input type=\"range\" id=\"slider1\" min=\"0\" max=\"180\" value=\"90\" oninput=\"sendSlider1(this.value)\">\
                    <br>\
                    <label>Сервопривод 2:</label>\
                    <input type=\"range\" id=\"slider2\" min=\"0\" max=\"180\" value=\"90\" oninput=\"sendSlider2(this.value)\">\
                    <script>\
                      var canvas = document.getElementById('joystick');\
                      var ctx = canvas.getContext('2d');\
                      var centerX = canvas.width / 2;\
                      var centerY = canvas.height / 2;\
                      var radius = 50;\
                      var maxServoAngle = 180;\
                      var maxMotorSpeed = 255;\
                      var horizontalValue = 90;\
                      var verticalValue = 0;\
                      \
                      function drawJoystick(x, y) {\
                        ctx.clearRect(0, 0, canvas.width, canvas.height);\
                        ctx.beginPath();\
                        ctx.arc(centerX, centerY, radius, 0, 2 * Math.PI, false);\
                        ctx.fillStyle = 'lightgray';\
                        ctx.fill();\
                        ctx.lineWidth = 2;\
                        ctx.strokeStyle = 'black';\
                        ctx.stroke();\
                        ctx.beginPath();\
                        ctx.arc(centerX + x * radius, centerY - y * radius, 10, 0, 2 * Math.PI, false);\
                        ctx.fillStyle = 'red';\
                        ctx.fill();\
                      }\
                      \
                      function sendJoystickPosition(x, y) {\
                        var xhttp = new XMLHttpRequest();\
                        var servoHorizontal = Math.round(x * maxServoAngle);\
                        var motorSpeed = Math.round(y * maxMotorSpeed);\
                        xhttp.open(\"GET\", \"/joystick?x=\" + servoHorizontal + \"&y=\" + motorSpeed, true);\
                        xhttp.send();\
                      }\
                      \
                      function getTouchPos(canvas, evt) {\
                        var rect = canvas.getBoundingClientRect();\
                        return {\
                          x: (evt.touches[0].clientX - rect.left - centerX) / radius,\
                          y: (centerY - evt.touches[0].clientY + rect.top) / radius\
                        };\
                      }\
                      \
                      function getMousePos(canvas, evt) {\
                        var rect = canvas.getBoundingClientRect();\
                        return {\
                          x: (evt.clientX - rect.left - centerX) / radius,\
                          y: (centerY - evt.clientY + rect.top) / radius\
                        };\
                      }\
                      \
                      canvas.addEventListener('touchmove', function(e) {\
                        e.preventDefault();\
                        var pos = getTouchPos(canvas, e);\
                        var x = pos.x;\
                        var y = pos.y;\
                        drawJoystick(x, y);\
                        sendJoystickPosition(x, y);\
                      });\
                      \
                      canvas.addEventListener('touchend', function(e) {\
                        drawJoystick(0, 0);\
                        sendJoystickPosition(0, 0);\
                      });\
                      \
                      canvas.addEventListener('mousemove', function(e) {\
                        var pos = getMousePos(canvas, e);\
                        var x = pos.x;\
                        var y = pos.y;\
                        drawJoystick(x, y);\
                        sendJoystickPosition(x, y);\
                      });\
                      \
                      canvas.addEventListener('mouseleave', function(e) {\
                        drawJoystick(0, 0);\
                        sendJoystickPosition(0, 0);\
                      });\
                      \
                      function sendSlider1(value) {\
                        var xhttp = new XMLHttpRequest();\
                        xhttp.open(\"GET\", \"/slider1?value=\" + value, true);\
                        xhttp.send();\
                      }\
                      \
                      function sendSlider2(value) {\
                        var xhttp = new XMLHttpRequest();\
                        xhttp.open(\"GET\", \"/slider2?value=\" + value, true);\
                        xhttp.send();\
                      }\
                    </script>\
                  </body>\
                </html>";
  
  server.send(200, "text/html; charset=UTF-8", html);
}

void handleJoystick() {

  if (server.hasArg("x") && server.hasArg("y")) {
    int servoAngleHorizontal = server.arg("x").toInt();
    int motorSpeed = server.arg("y").toInt();


    if (servoAngleHorizontal < 0) {
      servoAngleHorizontal = 0;
    } else if (servoAngleHorizontal > 180) {
      servoAngleHorizontal = 180;
    }
    servoHorizontal.write(servoAngleHorizontal);
    currentServoAngleHorizontal = servoAngleHorizontal;


    if (motorSpeed > 0) {
      analogWrite(motorPin1, motorSpeed);
      analogWrite(motorPin2, 0);
    } else if (motorSpeed < 0) {
      analogWrite(motorPin1, 0);
      analogWrite(motorPin2, -motorSpeed);
    } else {
      analogWrite(motorPin1, 0);
      analogWrite(motorPin2, 0);
    }
    currentMotorSpeed = motorSpeed;


    server.send(200, "text/plain", "OK");
  } else {
    server.send(200, "text/plain", "Отсутствуют параметры x и y");
  }
}

void handleSlider1() {

  if (server.hasArg("value")) {
    int value = server.arg("value").toInt();


    if (value < 0) {
      value = 0;
    } else if (value > 180) {
      value = 180;
    }
    servo1.write(value);
    currentServoAngle1 = value;


    server.send(200, "text/plain", "OK");
  } else {
    server.send(200, "text/plain", "Отсутствует параметр value");
  }
}

void handleSlider2() {

  if (server.hasArg("value")) {
    int value = server.arg("value").toInt();


    if (value < 0) {
      value = 0;
    } else if (value > 180) {
      value = 180;
    }
    servo2.write(value);
    currentServoAngle2 = value;


    server.send(200, "text/plain", "OK");
  } else {
    server.send(200, "text/plain", "Отсутствует параметр value");
  }
}

void handleCapture() {
  // Захват изображения с камеры
  camera_fb_t * fb = esp_camera_fb_get();
  if(!fb) {
    Serial.println("Ошибка захвата изображения");
    server.send(500, "text/plain", "Ошибка захвата изображения");
    return;
  }


  server.send_P(200, "image/jpeg", (const char *)fb->buf, fb->len);
  esp_camera_fb_return(fb);
}
