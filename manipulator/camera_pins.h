// Определение GPIO пинов для камеры OV2640 на ESP32 Wrover Dev
#define PWDN_GPIO_NUM -1  // Если пин PWDN не используется, установите в -1
#define RESET_GPIO_NUM -1  // Если пин RESET не используется, установите в -1
#define XCLK_GPIO_NUM 21  // Пин XCLK
#define SIOD_GPIO_NUM 26  // Пин SDA
#define SIOC_GPIO_NUM 27  // Пин SCL

#define Y9_GPIO_NUM 35  // Пин D7
#define Y8_GPIO_NUM 34  // Пин D6
#define Y7_GPIO_NUM 39  // Пин D5
#define Y6_GPIO_NUM 36  // Пин D4
#define Y5_GPIO_NUM 19  // Пин D3
#define Y4_GPIO_NUM 18  // Пин D2
#define Y3_GPIO_NUM  5  // Пин D1
#define Y2_GPIO_NUM  4  // Пин D0

#define VSYNC_GPIO_NUM 25  // Пин VSYNC
#define HREF_GPIO_NUM 23  // Пин HREF
#define PCLK_GPIO_NUM 22  // Пин PCLK