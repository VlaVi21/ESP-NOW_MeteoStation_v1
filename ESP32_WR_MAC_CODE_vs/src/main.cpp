// ESP32 Feather MAC: 24:dc:c3:44:34:5c
// Отримує дані через ESP-NOW і виводить на OLED

// --- ESP-NOW ---
#include <esp_now.h>
#include <WiFi.h>

// --- Джойстик і кнопка ---
#include <Arduino.h>
#include <ezButton.h>

// --- OLED ---
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// --- Параметри OLED ---
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define SCREEN_ADDRESS 0x3C

// --- Піни джойстика ---
#define VRX_PIN  39  // X-вісь
#define VRY_PIN  36  // Y-вісь
#define BUT_PIN  17  // Кнопка

// --- Пороги для напрямків ---
#define LEFT_THRESHOLD   1000
#define RIGHT_THRESHOLD  3000
#define UP_THRESHOLD     1000
#define DOWN_THRESHOLD   3000

// --- Прототипи функцій ---
void dispEspNowInfo();
void dispGPSData();
void dispAccelData();
void ButSet();
void JoySet();

// --- Змінні для керування екранами ---
int currentScreen = 0;   // 0 - MCU, 1 - GPS, 2 - Date/Time
const int totalScreens = 3;

// --- Структура даних (має збігатись з передавачем) ---
typedef struct {
    char textd[32]; 
    float x, y, t;          // Акселерометр + температура
    float lat, lon;         // GPS координати
    float speed;            // Швидкість
    int satellites;         // К-сть супутників
    float alt;              // Висота
    int year, month, day;   // Дата
    int hour, minute, second; // Час
} struct_message;

struct_message myData; // Буфер отриманих даних

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
ezButton button(BUT_PIN);  // Кнопка з антидребезгом

// --- Стан джойстика ---
enum State { IDLE, LEFT, RIGHT, UP, DOWN };
State lastState = IDLE;

// --- Обробка отриманих даних ---
void OnDataRecv(const uint8_t * mac, const uint8_t *incomingData, int len) {
  memcpy(&myData, incomingData, sizeof(myData));
  Serial.printf("Bytes received: %d\n", len);
  Serial.printf("Char: %s\n", myData.textd);
  Serial.printf("X: %.2f\n", myData.x);
  Serial.printf("Y: %.2f\n", myData.y);
  Serial.printf("Temp: %.1f\n", myData.t);
  Serial.printf("Lat: %.6f\n", myData.lat);
  Serial.printf("Lon: %.6f\n", myData.lon);
  Serial.printf("Speed: %.1f\n", myData.speed);
  Serial.printf("Satellites: %d\n", myData.satellites);
  Serial.printf("Height: %.1f\n", myData.alt);
  Serial.printf("Date: %02d/%02d/%04d\n", myData.day, myData.month, myData.year);
  Serial.printf("Time: %02d:%02d:%02d\n", myData.hour, myData.minute, myData.second);
  Serial.printf("Struct size: %d bytes\n\n", sizeof(myData));
}

void setup() {
  Wire.begin(21, 22);
  Serial.begin(115200);

  WiFi.mode(WIFI_STA); // Режим станції

  if (esp_now_init() != ESP_OK) {
    Serial.println("ESP-NOW init failed");
    return;
  }

  esp_now_register_recv_cb(OnDataRecv); // Реєстрація callback

  if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) { // Перевірка ініціалізації дисплею
    Serial.println(F("OLED init failed"));
    for (;;);
  }

  analogSetAttenuation(ADC_11db); // ADC до 3.3В
  button.setDebounceTime(50);     // Антидребезг кнопки

  display.clearDisplay();
  display.display();
}

void loop() {
  ButSet(); // Перевірка стану кнопки
  JoySet(); // Перевірка стану джойстика

  // Виводимо поточний екран
  if (currentScreen == 0)      dispEspNowInfo();
  else if (currentScreen == 1) dispGPSData();
  else if (currentScreen == 2) dispAccelData();
}

// --- Функція для обробки рухів джойстика ---
void JoySet(){
  
  int xVal = analogRead(VRX_PIN);  // Читаємо значення осі X
  int yVal = analogRead(VRY_PIN);  // Читаємо значення осі Y

  State currentState = IDLE;       // Поточний стан джойстика, за замовчуванням — IDLE
  static uint32_t tmr;             // Локальний таймер для "debounce" (затримка між спрацюваннями)

  // Пріоритет перевірки: спочатку вісь X, потім вісь Y
  if (xVal < LEFT_THRESHOLD) currentState = LEFT;       // Якщо X нижче порога — рух вліво
  else if (xVal > RIGHT_THRESHOLD) currentState = RIGHT; // Якщо X вище порога — рух вправо
  else if (yVal < UP_THRESHOLD) currentState = UP;       // Якщо Y нижче порога — рух вгору
  else if (yVal > DOWN_THRESHOLD) currentState = DOWN;   // Якщо Y вище порога — рух вниз

  // Виконуємо код тільки якщо пройшло 50 мс з моменту останнього оновлення
  if (millis() - tmr >= 100){
    tmr = millis(); // Оновлюємо час останньої перевірки

    // Якщо стан змінився — виконуємо дію
    if (currentState != lastState) {
      if (currentState == LEFT) {
        Serial.println("LEFT pressed");   // Натиснуто вліво
        currentScreen--;
        if (currentScreen < 0) currentScreen = totalScreens - 1; // Зациклення
        Serial.printf("UP pressed -> Screen %d\n", currentScreen);
      }
      else if (currentState == RIGHT) {
        Serial.println("RIGHT pressed");  // Натиснуто вправо
        currentScreen++;
        if (currentScreen >= totalScreens) currentScreen = 0; // Зациклення
        Serial.printf("DOWN pressed -> Screen %d\n", currentScreen);
      }
      else if (currentState == UP) {
        Serial.println("UP pressed");     // Натиснуто вгору
      }
      else if (currentState == DOWN) {
        Serial.println("DOWN pressed");   // Натиснуто вниз
      }
      lastState = currentState; // Оновлюємо останній стан
    }
  }
}


// --- Функція для обробки кнопки ---
void ButSet(){
  button.loop(); // Оновлюємо стан кнопки (потрібно викликати в кожній ітерації loop)

  int btnState = button.getState(); // Зчитуємо поточний стан кнопки (0 або 1) — не використовується далі, але може бути корисно

  if(button.isPressed()) {// Якщо кнопка була натиснута
    Serial.println("The button is pressed");
    currentScreen = 0;    // Перемикаємо на екран з датою і часом
  }         

  if(button.isReleased()){// Якщо кнопка була відпущена
    Serial.println("The button is released");
  }         
}

// --- Вивід основної інформації ---
void dispEspNowInfo() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(0, 0);

  display.println("ESP-NOW Data:");
  display.printf("Text: %s\n", myData.textd);
    display.println("Date:");
  if (myData.year > 2000)
    display.printf("%02d/%02d/%04d\n", myData.day, myData.month, myData.year);
  else
    display.println("No date");

  display.println("Time:");
  if (myData.hour >= 0)
    display.printf("%02d:%02d:%02d\n", myData.hour, myData.minute, myData.second);
  else
    display.println("No time");

  display.printf("Temp: %.1f C\n", myData.t);

  display.printf("Struct size: %d\n", sizeof(myData));

  display.display();
}

// --- Вивід GPS ---
void dispGPSData() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(0, 0);

  display.println("GPS Data:");
  display.printf("Lat: %.6f\n", myData.lat);
  display.printf("Lon: %.6f\n", myData.lon);
  display.printf("Height: %.1f m\n", myData.alt);
  display.printf("Speed: %.1f km/h\n", myData.speed);
  display.printf("Sat: %d\n", myData.satellites);

  display.display();
}

// --- Вивід координат з MCU ---
void dispAccelData() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(0, 0);

  display.println("MCU - 6050 Data:");
  display.printf("X: %.2f\n", myData.x);
  display.printf("Y: %.2f\n", myData.y);

  display.display();
}


