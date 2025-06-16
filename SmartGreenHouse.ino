#include <Adafruit_Sensor.h>
#include <DHT.h>
#include <DHT_U.h>
#include <Wire.h>
#include <Adafruit_TSL2561_U.h>
#include <Stepper.h>

// Параметри Blynk
#define BLYNK_TEMPLATE_ID "TMPL4DszjRNTc"
#define BLYNK_TEMPLATE_NAME "Smart Greenhouse"
#define BLYNK_AUTH_TOKEN "Your_Token"

#include <WiFiClient.h>
#include <BlynkSimpleEsp32.h>

// Підключення DHT датчика
#define DHTPIN 32 
#define DHTTYPE DHT22

// Піни для двигуна
#define IN1 14
#define IN2 27 
#define IN3 26 
#define IN4 25

#define IN5 16
#define IN6 17 
#define IN7 18 
#define IN8 19 

// Піни для LED
#define LED_RED 23
#define LED_GREEN 13
#define LED_BLUE 4

// Кроки для Stepper
#define STEPS_PER_REV 2048

// Ваші WiFi-дані
char ssid[] = "YOUR_WIFI_SSID";
char pass[] = "YOUR_WIFI_PASSWORD";

Stepper stepper_h(STEPS_PER_REV, IN1, IN3, IN2, IN4);
Stepper stepper_l(STEPS_PER_REV, IN5, IN7, IN6, IN8);

BlynkTimer blynk_Timer;

DHT dht(DHTPIN, DHTTYPE);
Adafruit_TSL2561_Unified tsl = Adafruit_TSL2561_Unified(TSL2561_ADDR_FLOAT, 12345);

float temperature;
float humidity;
float lightLevel;

int is_less_humidity = 0;
int is_bigger_light = 0;
int delayDuration = 0;

float humidityThreshold = 70.0;
float lightThreshold = 400.0;
float temperatureThreshold = 5.0;

// Стани для крокових двигунів
bool stepper_h_state = false;
bool stepper_l_state = false;

// Синхронізація з Blynk
BLYNK_CONNECTED() {
  Serial.println("Sync with the Blynk app.");
  Blynk.syncVirtual(V3, V4); // Синхронізація станів перемикачів
}

// Функція для відправлення даних у Blynk
void Send_Sensor_Data() {
  Blynk.virtualWrite(V0, (int)temperature);
  Blynk.virtualWrite(V1, (int)humidity);
  Blynk.virtualWrite(V2, (int)lightLevel);
}

// Керування першим кроковим двигуном
BLYNK_WRITE(V3) {
  stepper_h_state = param.asInt(); // Отримання стану з перемикача
  if (stepper_h_state) {
    stepper_h.step(STEPS_PER_REV); // Один повний оберт
    Serial.println("Stepper H activated!");
  } else {
    Serial.println("Stepper H deactivated.");
  }
}

// Керування другим кроковим двигуном
BLYNK_WRITE(V4) {
  stepper_l_state = param.asInt(); // Отримання стану з перемикача
  if (stepper_l_state) {
    stepper_l.step(STEPS_PER_REV); // Один повний оберт
    Serial.println("Stepper L activated!");
  } else {
    Serial.println("Stepper L deactivated.");
  }
}

void setup() {
  Serial.begin(115200);
  Serial.println("------------Connecting to WiFi.");
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.begin(ssid, pass);

  int connecting_process_timed_out = 20 * 2; //--> 20 seconds.
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(500);
    if (connecting_process_timed_out > 0) connecting_process_timed_out--;
    if (connecting_process_timed_out == 0) {
      Serial.println("\nFailed to connect to WiFi. Restarting...");
      delay(1000);
      ESP.restart();
    }
  }

  if (!tsl.begin()) {
    Serial.println(F("No TSL2561 detected. Check wiring!"));
    while (1);
  }

  tsl.enableAutoRange(true);
  tsl.setIntegrationTime(TSL2561_INTEGRATIONTIME_402MS);
  Serial.println(F("TSL2561 initialized!"));

  dht.begin();
  stepper_h.setSpeed(10);
  stepper_l.setSpeed(10);
  pinMode(LED_RED, OUTPUT);
  pinMode(LED_GREEN, OUTPUT);
  pinMode(LED_BLUE, OUTPUT);

  Blynk.config(BLYNK_AUTH_TOKEN, "blynk.cloud", 80);
  if (WiFi.status() == WL_CONNECTED) {
    Blynk.connect();
  }

  // Додавання таймера для регулярної відправки даних
  blynk_Timer.setInterval(5000L, Send_Sensor_Data); // Відправка даних кожні 5 секунд
}

void loop() {
  sensors_event_t event;
  delay(1100);

  humidity = dht.readHumidity();
  temperature = dht.readTemperature();

  if (isnan(humidity) || isnan(temperature)) {
    Serial.println(F("Failed to read from DHT sensor!"));
    humidity = 0; 
    temperature = 0;
  }

  tsl.getEvent(&event);
  lightLevel = event.light ? event.light : -1;

  if (lightLevel != -1) {
    Serial.print(F("Light Level: "));
    Serial.print(lightLevel);
    Serial.println(F(" lux"));
  } else {
    Serial.println(F("Sensor overload or error!"));
  }

  Serial.print(F("Humidity: "));
  Serial.print(humidity);
  Serial.print(F("%  Temperature: "));
  Serial.print(temperature);
  Serial.println(F("°C"));

  if (humidity < humidityThreshold && is_less_humidity == 0) {
    is_less_humidity = 1;
    stepper_h.step(STEPS_PER_REV);
  } else if (humidity >= humidityThreshold && is_less_humidity == 1) {
    is_less_humidity = 0;
    stepper_h.step(-STEPS_PER_REV);
  }

  if (lightLevel > lightThreshold && is_bigger_light == 0) {
    is_bigger_light = 1;
    stepper_l.step(STEPS_PER_REV);
  } else if (lightLevel <= lightThreshold && is_bigger_light == 1) {
    is_bigger_light = 0;
    stepper_l.step(-STEPS_PER_REV);
  }

  if (temperature < temperatureThreshold) {
    digitalWrite(LED_BLUE, HIGH);
  } else {
    delayDuration = constrain(800 - temperature * 4, 50, 800);
    digitalWrite(LED_BLUE, HIGH);
    delay(delayDuration);
    digitalWrite(LED_BLUE, LOW);
  }

  Blynk.run();
  blynk_Timer.run();
}
