#include <WiFi.h>
#include <WebServer.h>
#include <ESP32Servo.h>
#include "esp_camera.h"
#include <SPIFFS.h>
#include <ArduinoJson.h>

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
const char* ssid = "Pixel_3763";
const char* password = "***********";

// Параметры плода
const int minSizeThreshold = 4400;  // Минимальное количество пикселей для принятия объекта
const int maxSizeThreshold = 12000;  // Максимальное количество пикселей для принятия объекта
const int minDiameter = 76;         // Минимальный диаметр в пикселях (приблизительно)
const int maxDiameter = 122;         // Максимальный диаметр в пикселях (приблизительно)

bool fruitDetected = false;

// Структура для хранения команд маршрута
struct Command {
  int servoHorizontalAngle;
  int servo1Angle;
  int servo2Angle;
  int motorSpeed;
};

std::vector<Command> routeCommands;
bool recordingRoute = false;

void setup() {
  // Инициализация SPIFFS
  if (!SPIFFS.begin(true)) {
    Serial.println("Ошибка монтирования SPIFFS");
    return;
  }

  // Присоединяем сервоприводы к пинам
  servoHorizontal.attach(servoPinHorizontal);
  servo1.attach(servoPin1);
  servo2.attach(servoPin2);
  
  // Устанавливаем начальные углы сервоприводов
  servoHorizontal.write(90);
  servo1.write(90);
  servo2.write(90);

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
  config.pixel_format = PIXFORMAT_RGB565; // Используем RGB формат для обработки

  if (psramFound()) {
    config.frame_size = FRAMESIZE_QVGA;
    config.jpeg_quality = 15;
    config.fb_count = 1;
  } else {
    config.frame_size = FRAMESIZE_QVGA;
    config.jpeg_quality = 15;
    config.fb_count = 1;
  }
  if(psramFound()){
    config.fb_location = CAMERA_FB_IN_PSRAM; // Использовать PSRAM
    config.grab_mode = CAMERA_GRAB_LATEST; // Минимизировать задержку
} else {
    Serial.println("PSRAM не найден.");
}
  // Инициализация камеры
  if (esp_camera_init(&config) != ESP_OK) {
    Serial.println("Ошибка инициализации камеры");
    return;
  }

  // Инициализация веб-сервера
  server.on("/", handleRoot);
  server.on("/joystick", handleJoystick);
  server.on("/capture", HTTP_GET, handleCapture);
  server.on("/slider1", handleSlider1);
  server.on("/slider2", handleSlider2);
  server.on("/fruitStatus", HTTP_GET, handleFruitStatus);
  server.on("/startRecord", handleStartRecord);
  server.on("/stopRecord", handleStopRecord);
  server.on("/playRoute", handlePlayRoute);
  server.begin();
  Serial.println("HTTP сервер запущен");

  // Чтение маршрута из файла при старте
  loadRouteFromFile();
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
                    <br>\
                    <button onclick=\"startRecord()\">Начать запись маршрута</button>\
                    <button onclick=\"stopRecord()\">Остановить запись маршрута</button>\
                    <button onclick=\"playRoute()\">Воспроизвести маршрут</button>\
		    <h3 id=\"fruitStatus\">Статус плода: неизвестно</h3>\
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
                        var servoHorizontal = Math.round((x + 1) / 2 * maxServoAngle);\
                        var motorSpeed = Math.round((y + 1) / 2 * maxMotorSpeed);\
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
                        var x = Math.max(-1, Math.min(1, pos.x));\
                        var y = Math.max(-1, Math.min(1, pos.y));\
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
                        var x = Math.max(-1, Math.min(1, pos.x));\
                        var y = Math.max(-1, Math.min(1, pos.y));\
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
                      \
                      function startRecord() {\
                        var xhttp = new XMLHttpRequest();\
                        xhttp.open(\"GET\", \"/startRecord\", true);\
                        xhttp.send();\
                      }\
                      \
                      function stopRecord() {\
                        var xhttp = new XMLHttpRequest();\
                        xhttp.open(\"GET\", \"/stopRecord\", true);\
                        xhttp.send();\
                      }\
                      \
                      function playRoute() {\
                        var xhttp = new XMLHttpRequest();\
                        xhttp.open(\"GET\", \"/playRoute\", true);\
                        xhttp.send();\
                      }\
                      function updateFruitStatus() {\
                        var xhttp = new XMLHttpRequest();\
                        xhttp.onreadystatechange = function() {\
                          if (this.readyState == 4 && this.status == 200) {\
                            document.getElementById(\"fruitStatus\").innerHTML = this.responseText;\
                          }\
                        };\
                        xhttp.open(\"GET\", \"/fruitStatus\", true);\
                        xhttp.send();\
                      }\
                      \
                      setInterval(updateFruitStatus, 5000); // Обновляем статус плода каждые 5 секунд\
                    </script>\
                  </body>\
                </html>";
  
  server.send(200, "text/html; charset=UTF-8", html);
}

void handleJoystick() {
  if (server.hasArg("x") && server.hasArg("y")) {
    int servoAngleHorizontal = server.arg("x").toInt();
    int motorSpeed = server.arg("y").toInt();
    servoHorizontal.write(servoAngleHorizontal);

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

    // Записываем команду в маршрут, если запись активна
    if (recordingRoute) {
      Command cmd;
      cmd.servoHorizontalAngle = servoAngleHorizontal;
      cmd.servo1Angle = servo1.read();
      cmd.servo2Angle = servo2.read();
      cmd.motorSpeed = motorSpeed;
      routeCommands.push_back(cmd);
      saveRouteToFile(); // Сохраняем маршрут в файл
    }

    server.send(200, "text/plain", "OK");
  } else {
    server.send(200, "text/plain", "Отсутствуют параметры x и y");
  }
}

void handleSlider1() {
  if (server.hasArg("value")) {
    int value = server.arg("value").toInt();
    servo1.write(value);
    server.send(200, "text/plain", "OK");
  } else {
    server.send(200, "text/plain", "Отсутствует параметр value");
  }
}

void handleSlider2() {
  if (server.hasArg("value")) {
    int value = server.arg("value").toInt();
    servo2.write(value);
    server.send(200, "text/plain", "OK");
  } else {
    server.send(200, "text/plain", "Отсутствует параметр value");
  }
}

void handleCapture() {
  Serial.println("Attempting to capture image...");
  camera_fb_t * fb = esp_camera_fb_get();
  if (!fb) {
    Serial.println("Ошибка захвата изображения");
    server.send(500, "text/plain", "Ошибка захвата изображения");
    return;
  }

  fruitDetected = detectFruit(fb);

  if (fruitDetected) {
    Serial.println("Созревший плод обнаружен");
  } else {
    Serial.println("Созревший плод не обнаружен");
  }

  server.send_P(200, "image/jpeg", (const char *)fb->buf, fb->len);
  esp_camera_fb_return(fb);
}

bool detectFruit(camera_fb_t * fb) {
  uint16_t *image_data = (uint16_t *)fb->buf;
  int width = fb->width;
  int height = fb->height;
  bool found = false;

  for (int y = 0; y < height; y++) {
    for (int x = 0; x < width; x++) {
      if (isRed(image_data, x, y, width, height)) {
        if (isWithinSize(x, y, width, height, image_data)) {
          found = true;
          break;
        }
      }
    }
    if (found) break;
  }

  return found;
}

bool isRed(uint16_t *image_data, int x, int y, int width, int height) {
  uint16_t color = image_data[y * width + x];
  int r = (color >> 11) & 0x1F;
  int g = (color >> 5) & 0x3F;
  int b = color & 0x1F;

  r = (r * 255) / 31;
  g = (g * 255) / 63;
  b = (b * 255) / 31;

  return r > 150 && g < 100 && b < 100;
}

bool isWithinSize(int x, int y, int width, int height, uint16_t *image_data) {
  int count = 0;
  int radius = maxDiameter; // Диаметр в пикселях для проверки округлости

  for (int dy = -radius; dy <= radius; dy++) {
    for (int dx = -radius; dx <= radius; dx++) {
      if (dx * dx + dy * dy <= radius * radius) {
        int nx = x + dx;
        int ny = y + dy;

        if (nx >= 0 && ny >= 0 && nx < width && ny < height) {
          uint16_t color = image_data[ny * width + nx];
          if (isRed(image_data, nx, ny, width, height)) {
            count++;
          }
        }
      }
    }
  }

  return count > minSizeThreshold && count < maxSizeThreshold;
}

void handleFruitStatus() {
  if (fruitDetected) {
    server.send(200, "text/plain", "Созревший плод обнаружен!");
  } else {
    server.send(200, "text/plain", "Созревший плод не обнаружен.");
  }
}

void handleStartRecord() {
  routeCommands.clear();
  recordingRoute = true;
  server.send(200, "text/plain", "Запись маршрута начата");
}

void handleStopRecord() {
  recordingRoute = false;
  saveRouteToFile(); // Сохраняем маршрут в файл при остановке записи
  server.send(200, "text/plain", "Запись маршрута остановлена");
}

void handlePlayRoute() {
  server.send(200, "text/plain", "Воспроизведение маршрута начато");
  for (const Command& cmd : routeCommands) {
    servoHorizontal.write(cmd.servoHorizontalAngle);
    servo1.write(cmd.servo1Angle);
    servo2.write(cmd.servo2Angle);
    if (cmd.motorSpeed > 0) {
      analogWrite(motorPin1, cmd.motorSpeed);
      analogWrite(motorPin2, 0);
    } else if (cmd.motorSpeed < 0) {
      analogWrite(motorPin1, 0);
      analogWrite(motorPin2, -cmd.motorSpeed);
    } else {
      analogWrite(motorPin1, 0);
      analogWrite(motorPin2, 0);
    }
    delay(10); // Задержка между командами
  }
}

void saveRouteToFile() {
  File file = SPIFFS.open("/route.json", FILE_WRITE);
  if (!file) {
    Serial.println("Ошибка открытия файла для записи");
    return;
  }

  DynamicJsonDocument doc(1024);
  JsonArray array = doc.to<JsonArray>();

  for (const Command& cmd : routeCommands) {
    JsonObject obj = array.createNestedObject();
    obj["servoHorizontalAngle"] = cmd.servoHorizontalAngle;
    obj["servo1Angle"] = cmd.servo1Angle;
    obj["servo2Angle"] = cmd.servo2Angle;
    obj["motorSpeed"] = cmd.motorSpeed;
  }

  if (serializeJson(doc, file) == 0) {
    Serial.println("Ошибка записи JSON в файл");
  }

  file.close();
}

void loadRouteFromFile() {
  File file = SPIFFS.open("/route.json", FILE_READ);
  if (!file) {
    Serial.println("Ошибка открытия файла для чтения");
    return;
  }

  DynamicJsonDocument doc(1024);
  DeserializationError error = deserializeJson(doc, file);
  if (error) {
    Serial.println("Ошибка разбора JSON");
    file.close();
    return;
  }

  JsonArray array = doc.as<JsonArray>();
  routeCommands.clear();

  for (JsonVariant v : array) {
    Command cmd;
    cmd.servoHorizontalAngle = v["servoHorizontalAngle"];
    cmd.servo1Angle = v["servo1Angle"];
    cmd.servo2Angle = v["servo2Angle"];
    cmd.motorSpeed = v["motorSpeed"];
    routeCommands.push_back(cmd);
  }

  file.close();
}
