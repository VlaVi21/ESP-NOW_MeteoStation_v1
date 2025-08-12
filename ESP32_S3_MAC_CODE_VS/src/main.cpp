//ESP32 S3
// 98:3d:ae:f2:cf:64
//Передавач

//Бібли для ESP-NOW
#include <esp_now.h>
#include <WiFi.h>

#include <Wire.h>
#include <Adafruit_Sensor.h> //MCU6050
#include <Adafruit_MPU6050.h>

#include <TinyGPS++.h> //GPS


//GPS
#define RXD2 4
#define TXD2 5
#define GPS_BAUD 9600

TinyGPSPlus gps; //GPS
HardwareSerial gpsSerial(2);

uint8_t broadcastAddress[] = {0x24, 0xDC, 0xC3, 0x44, 0x34, 0x5C}; //MAC-адреса плати приймача

Adafruit_MPU6050 mpu; //Змінна для MCU

//Структура для передачі даних
typedef struct struct_message {
  char textd[32];
  float x, y, t;
  float lat, lon;
  float speed;
  int satellites;
  float alt;
  int year, month, day;
  int hour, minute, second;
} struct_message;

struct_message myData; //Збереження значень змінних

esp_now_peer_info_t peerInfo; //Інформація про одноранговий вузл

//Функція зворотного виклику, яка буде виконана під час надсилання повідомлення. 
//У цьому випадку ця функція просто виводить, чи було повідомлення успішно доставлено, чи ні.
void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  Serial.print("\r\nLast Packet Send Status:\t");
  Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Delivery Success" : "Delivery Fail");
}

void setup() {
  Serial.begin(115200);
  Serial.println("Starting...");

  WiFi.mode(WIFI_STA); //Налаштування пристрою, як станції Wi-Fi

  //Ініціалізація ESP-NOW:
  if (esp_now_init() != ESP_OK) {
  Serial.println("Error initializing ESP-NOW");
  return;
  }

  //Після успішної ініціалізації ESP-NOW зареєструйте функцію зворотного виклику, яка буде викликана під час надсилання повідомлення. 
  //У цьому випадку ми реєструємо для OnDataSent() функція, створена раніше.
  esp_now_register_send_cb(OnDataSent);

  //Після цього нам потрібно підключитися до іншого пристрою ESP-NOW для надсилання даних.
  //Register peer
  memcpy(peerInfo.peer_addr, broadcastAddress, 6);
  peerInfo.channel = 0;
  peerInfo.encrypt = false;

  //Add peer
  if (esp_now_add_peer(&peerInfo) != ESP_OK){
    Serial.println("Failed to add peer");
    return;
  }

  Wire.begin(7, 6); //SDA, SCL
  Serial.println("Wire initialized");

  // Ініціалізація MPU6050
  Serial.println("MPU6050 OLED demo");
  if (!mpu.begin()) {
    Serial.println("MPU6050 init failed, continuing...");
  } else {
    Serial.println("Found MPU6050 sensor");
  }

  //GPS
  gpsSerial.begin(GPS_BAUD, SERIAL_8N1, RXD2, TXD2);
  Serial.println("GPS Initialized");
}

void loop() {
  unsigned long start = millis();
  
  // Читаємо GPS дані протягом 1 секунди
  while (millis() - start < 1000) {
    while (gpsSerial.available() > 0) {
      gps.encode(gpsSerial.read());
    }
  }

 // MPU6050
  sensors_event_t a, g, temp;
  mpu.getEvent(&a, &g, &temp);

  // Наповнення структури
  strcpy(myData.textd, "Hello Ukraine");

  //MCU
  myData.x = a.acceleration.x; 
  myData.y = a.acceleration.y;
  myData.t = temp.temperature;

  //GPS
  myData.lat = gps.location.isValid() ? gps.location.lat() : 0.0;
  myData.lon = gps.location.isValid() ? gps.location.lng() : 0.0;
  myData.speed = gps.speed.isValid() ? gps.speed.kmph() : 0.0;
  myData.satellites = gps.satellites.isValid() ? gps.satellites.value() : -1;
  myData.alt = gps.altitude.isValid() ? gps.altitude.meters() : -1.0;

  if (gps.date.isValid() && gps.satellites.value() >= 4) {
    myData.day = gps.date.day();
    myData.month = gps.date.month();
    myData.year = gps.date.year();
  } else {
    myData.day = myData.month = myData.year = -1;
  }

  if (gps.time.isValid() && gps.satellites.value() >= 4) {
    int correctedHour = gps.time.hour() + 3;
    if (correctedHour >= 24) correctedHour -= 24;

    myData.hour = correctedHour;
    myData.minute = gps.time.minute();
    myData.second = gps.time.second();
  } else {
    myData.hour = myData.minute = myData.second = -1;
  }
  
  //Відправляємо повідомлення 
  esp_err_t result = esp_now_send(broadcastAddress, (uint8_t *) &myData, sizeof(myData));

  //Перевірка, чи повідомленя успішно надіслане
  if (result == ESP_OK) {
    Serial.println("Sent with success");
  }
  else {
    Serial.println("Error sending the data");
  }

  delay(1000); //Можна змінювати для швидкої передачі даних, мені і 1 секунди вистачає
}

