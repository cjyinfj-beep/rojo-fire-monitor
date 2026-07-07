/*
 * ESP32-S3 消防监测系统 - v5（用户静音保持版）
 * 
 * 核心改进：
 * - 用户点击"关闭蜂鸣器/LED"后，在当前报警周期内保持关闭
 * - 直到传感器检测到新的报警条件变化，才自动恢复响应
 * - 避免"关闭1秒后又自动响"的问题
 * 
 * 静音逻辑：
 * - userSilencedBuzzer: 用户手动静音，报警原因变化前保持
 * - userSilencedLed: 用户手动关LED，报警原因变化前保持
 * - lastAlarmReason: 记录上次报警原因，用于检测"新的报警"
 */

#include <Wire.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_GFX.h>
#include <Adafruit_TCS34725.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <HTTPClient.h>
#include <driver/rtc_io.h>

// ==================== 引脚定义 ====================
#define PIN_TCS3472_INT     4
#define PIN_HCSR04_ECHO     5
#define PIN_I2C_SDA         8
#define PIN_TCS3472_LED     9
#define PIN_LED_YELLOW      10
#define PIN_LED_RED         11
#define PIN_HCSR04_TRIG     13
#define PIN_DS18B20_DQ      14
#define PIN_BUZZER          16
#define PIN_I2C_SCL         18

// ==================== WiFi 配置 ====================
const char* WIFI_SSID = "vivo";
const char* WIFI_PASSWORD = "1234567890";

// ==================== 后端配置 ====================
const char* SERVER_HOST = "139.196.153.51";
const char* SERVER_PORT = "80";
const char* DEVICE_ID = "EX-001";
const char* HTTP_SERVER_URL = "http://139.196.153.51/api/mqtt/webhook";

// ==================== MQTT 配置 ====================
const char* MQTT_SERVER = "139.196.153.51";
const int   MQTT_PORT = 1883;
const char* MQTT_CLIENT_ID = "esp32_fire_monitor_01";
const char* TOPIC_PUB = "sensor/data";

// ==================== 低功耗配置 ====================
#define SLEEP_INTERVAL_US       60000000ULL
#define ALARM_KEEP_AWAKE_MS     30000
#define WIFI_CONNECT_TIMEOUT_MS 10000
#define MQTT_CONNECT_TIMEOUT_MS 8000

// ==================== OLED 配置 ====================
#define SCREEN_WIDTH  128
#define SCREEN_HEIGHT 64
#define OLED_RESET    -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// ==================== 传感器 ====================
Adafruit_TCS34725 tcs = Adafruit_TCS34725(TCS34725_INTEGRATIONTIME_101MS, TCS34725_GAIN_4X);
OneWire oneWire(PIN_DS18B20_DQ);
DallasTemperature tempSensor(&oneWire);

// ==================== 蜂鸣器 PWM ====================
#define PWM_CHANNEL_BUZZER  0
#define PWM_FREQ_BUZZER     2000
#define PWM_RES_BUZZER      8

// ==================== RTC 内存变量 ====================
RTC_DATA_ATTR int bootCount = 0;

// ==================== 运行时变量 ====================
float distance_cm = 0.0;
float temperature_c = 0.0;
uint16_t r, g, b, c;
int detected_color = -1;
bool alarmTriggered = false;
int alarmPriority = 0;
String alarmReason = "";

// ====== 新增：用户静音状态（报警原因变化前保持）======
bool userSilencedBuzzer = false;   // 用户手动关闭了蜂鸣器
bool userSilencedLed = false;      // 用户手动关闭了LED
String lastAlarmReason = "";       // 上次报警原因（检测是否"新的报警"）

WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);
esp_sleep_wakeup_cause_t wakeupReason;

// ==================== 函数声明 ====================
float measureDistance();
void readColor();
void readTemperature();
void updateDisplay();
void beep(int duration_ms, int freq = 2000);
void buzzerAlarm(int times = 3);
void buzzerAlarmYellow(int times = 3);
void buzzerContinuousOn(int freq = 2000);
void buzzerContinuousOff();
int detectColor();
const char* getColorText(int color);
void checkAlarm();
void updateLEDs();

void setupWiFi();
void setupMQTT();
bool connectMQTT();
void mqttCallback(char* topic, byte* payload, unsigned int length);
void publishSensorData();
void publishSensorDataHTTP();

String checkCommandHTTP();
bool ackCommandHTTP(const char* command);

void enterDeepSleep();
void powerOffPeripherals();
void powerOnPeripherals();
void configureTCSInterrupt();
void clearTCSInterrupt();
void printWakeupReason();

// ==================== 初始化 ====================
void setup() {
  Serial.begin(115200);
  delay(100);

  bootCount++;
  wakeupReason = esp_sleep_get_wakeup_cause();

  Serial.println("\n========================================");
  Serial.println("消防监测系统 - v5 用户静音保持版");
  Serial.print("启动次数: "); Serial.println(bootCount);
  printWakeupReason();
  Serial.println("========================================\n");

  // 每次唤醒重置用户静音状态（新的采集周期）
  userSilencedBuzzer = false;
  userSilencedLed = false;
  lastAlarmReason = "";

  powerOnPeripherals();

  Serial.println("[采集] 开始传感器数据采集...");
  distance_cm = measureDistance();
  readColor();
  readTemperature();
  detected_color = detectColor();
  updateLEDs();

  Serial.println("----------------------------------------");
  Serial.print("距离: "); Serial.print(distance_cm); Serial.println(" cm");
  Serial.print("温度: "); Serial.print(temperature_c); Serial.println(" C");
  Serial.print("颜色 R:"); Serial.print(r);
  Serial.print(" G:"); Serial.print(g);
  Serial.print(" B:"); Serial.print(b);
  Serial.print(" C:"); Serial.println(c);
  Serial.print("颜色判断: "); Serial.println(getColorText(detected_color));
  Serial.println("----------------------------------------\n");

  checkAlarm();
  updateDisplay();

  // 报警初始触发（用户未静音则响）
  if (alarmTriggered) {
    if (alarmPriority == 2 && !userSilencedBuzzer) {
      Serial.println("[蜂鸣器] 高优先级报警: 蜂鸣器持续响");
      buzzerContinuousOn(2000);
    } else if (alarmPriority == 1 && !userSilencedBuzzer) {
      Serial.println("[蜂鸣器] 黄色预警慢节奏报警");
      buzzerAlarmYellow(3);
    }
  }

  // 网络连接 + 双协议上报
  Serial.println("[网络] 连接WiFi...");
  setupWiFi();
  bool wifiConnected = (WiFi.status() == WL_CONNECTED);
  bool mqttConnected = false;

  if (wifiConnected) {
    Serial.println("[网络] WiFi连接成功");
    setupMQTT();
    mqttConnected = connectMQTT();
    if (mqttConnected) {
      publishSensorData();
    }
    Serial.println("[网络] HTTP备用通道上报...");
    publishSensorDataHTTP();
  } else {
    Serial.println("[网络] WiFi连接超时");
  }

  // ====== 报警保持循环（核心逻辑）======
  if (alarmTriggered) {
    Serial.println("[低功耗] 报警状态，保持清醒30秒...");
    Serial.println("[控制] 支持远程命令: buzzer_off(保持静音) | led_off(保持关闭)");
    unsigned long alarmStart = millis();
    unsigned long lastBeepTime = 0;
    unsigned long lastReportTime = 0;
    unsigned long lastCmdCheckTime = 0;
    bool continuousOn = false;

    while (millis() - alarmStart < ALARM_KEEP_AWAKE_MS) {
      // === 1. HTTP 轮询查询远程命令（每秒1次）===
      if (millis() - lastCmdCheckTime >= 1000) {
        lastCmdCheckTime = millis();
        
        if (WiFi.status() == WL_CONNECTED) {
          String cmd = checkCommandHTTP();
          
          if (cmd == "buzzer_off") {
            Serial.println("[HTTP命令] 用户关闭蜂鸣器 → 保持静音直到新报警");
            userSilencedBuzzer = true;    // 关键：设置静音标志
            buzzerContinuousOff();
            continuousOn = false;
            ackCommandHTTP("buzzer_off");
            updateDisplay();
            
          } else if (cmd == "buzzer_on") {
            Serial.println("[HTTP命令] 用户开启蜂鸣器");
            userSilencedBuzzer = false;   // 取消静音
            buzzerContinuousOn(2000);
            continuousOn = true;
            ackCommandHTTP("buzzer_on");
            updateDisplay();
            
          } else if (cmd == "led_off") {
            Serial.println("[HTTP命令] 用户关闭LED → 保持关闭直到新报警");
            userSilencedLed = true;       // 关键：设置关闭标志
            digitalWrite(PIN_LED_RED, LOW);
            digitalWrite(PIN_LED_YELLOW, LOW);
            ackCommandHTTP("led_off");
            
          } else if (cmd == "led_on") {
            Serial.println("[HTTP命令] 用户开启LED");
            userSilencedLed = false;      // 取消关闭
            updateLEDs();
            ackCommandHTTP("led_on");
          }
        }
      }

      // === 2. MQTT 轮询（可选备用）===
      if (mqttConnected) {
        mqttClient.loop();
      }

      // === 3. 重新采集传感器数据 ===
      distance_cm = measureDistance();
      readColor();
      readTemperature();
      detected_color = detectColor();

      // === 4. 重新评估报警（关键：检测是否有"新的报警"）===
      String oldReason = alarmReason;
      checkAlarm();
      
      // 如果报警原因发生变化 → 新的报警触发 → 重置用户静音状态
      if (alarmReason != oldReason && alarmTriggered) {
        Serial.println("[报警] 检测到新的报警条件，重置用户静音状态");
        userSilencedBuzzer = false;
        userSilencedLed = false;
      }
      
      // 更新LED显示（尊重用户关闭指令）
      updateLEDs();

      // 如果报警已完全解除
      if (!alarmTriggered) {
        Serial.println("[低功耗] 报警已解除, 退出告警循环");
        break;
      }

      // === 5. 更新OLED显示 ===
      updateDisplay();

      // === 6. 蜂鸣器控制（尊重用户静音指令）===
      if (alarmPriority == 2) {
        if (!userSilencedBuzzer && !continuousOn) {
          // 用户未静音且蜂鸣器未开启 → 开启
          buzzerContinuousOn(2000);
          continuousOn = true;
          Serial.println("[蜂鸣器] 持续响 ON");
        } else if (userSilencedBuzzer && continuousOn) {
          // 用户已静音但蜂鸣器还在响 → 关闭
          buzzerContinuousOff();
          continuousOn = false;
          Serial.println("[蜂鸣器] 用户静音 → 持续响 OFF");
        }
        delay(1000);
        
      } else if (alarmPriority == 1) {
        if (continuousOn) {
          buzzerContinuousOff();
          continuousOn = false;
          Serial.println("[蜂鸣器] 持续响 OFF");
        }
        // 黄色预警：用户未静音才响
        if (!userSilencedBuzzer && millis() - lastBeepTime >= 8000) {
          lastBeepTime = millis();
          buzzerAlarmYellow(2);
        }
        delay(3000);
      }

      // === 7. 每10秒持续上报 ===
      if (millis() - lastReportTime >= 10000) {
        lastReportTime = millis();
        if (mqttConnected) {
          if (!mqttClient.connected()) mqttClient.connect(MQTT_CLIENT_ID, "", "");
          if (mqttClient.connected()) {
            publishSensorData();
            Serial.println("[MQTT] 报警期间持续上报");
          }
        }
        if (WiFi.status() == WL_CONNECTED) {
          publishSensorDataHTTP();
          Serial.println("[HTTP] 报警期间持续上报");
        }
      }
    }

    // 退出报警循环：确保关闭蜂鸣器
    if (continuousOn) {
      buzzerContinuousOff();
      Serial.println("[蜂鸣器] 退出告警循环: 持续响 OFF");
    }
  }

  // 关闭网络，进入睡眠
  if (mqttConnected) mqttClient.disconnect();
  if (wifiConnected) {
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
    Serial.println("[网络] WiFi已关闭");
  }

  Serial.println("[低功耗] 关闭外设，准备进入Deep-Sleep...");
  powerOffPeripherals();
  configureTCSInterrupt();
  enterDeepSleep();
}

void loop() {
  delay(1000);
}

// ============================================================================
// 报警判断（新增：检测报警原因是否变化）
// ============================================================================
void checkAlarm() {
  String currentReason = "";
  int currentPriority = 0;

  // 检测报警条件
  if (temperature_c < -20.0 || temperature_c > 60.0) {
    currentReason += (temperature_c < -20.0) ? "LOW_TEMP;" : "HIGH_TEMP;";
    currentPriority = 2;
  }
  if (detected_color == 2) {
    currentReason += "RED_COLOR;";
    currentPriority = 2;
  }
  if (distance_cm > 20.0) {
    currentReason += "FAR_DISTANCE;";
    currentPriority = 2;
  }
  if (detected_color == 1 && currentPriority < 2) {
    currentReason += "YELLOW_COLOR;";
    currentPriority = 1;
  }

  // 关键：如果报警原因发生变化，重置用户静音状态
  if (currentReason != lastAlarmReason) {
    if (!currentReason.isEmpty() && !lastAlarmReason.isEmpty()) {
      // 从一种报警变成另一种报警（或从无报警变成有报警）
      Serial.println("[报警] 报警原因变化，重置用户静音");
      userSilencedBuzzer = false;
      userSilencedLed = false;
    }
    lastAlarmReason = currentReason;
  }

  alarmTriggered = !currentReason.isEmpty();
  alarmPriority = currentPriority;
  alarmReason = currentReason;
}

// ============================================================================
// LED 更新（新增：尊重用户关闭指令）
// ============================================================================
void updateLEDs() {
  // 如果用户手动关闭了LED，保持关闭（直到新的报警触发）
  if (userSilencedLed) {
    digitalWrite(PIN_LED_YELLOW, LOW);
    digitalWrite(PIN_LED_RED, LOW);
    return;
  }

  // 正常LED逻辑
  if (detected_color == 1) {
    digitalWrite(PIN_LED_YELLOW, HIGH);
    digitalWrite(PIN_LED_RED, LOW);
  } else if (detected_color == 2) {
    digitalWrite(PIN_LED_YELLOW, LOW);
    digitalWrite(PIN_LED_RED, HIGH);
  } else {
    digitalWrite(PIN_LED_YELLOW, LOW);
    digitalWrite(PIN_LED_RED, LOW);
  }
}

// ============================================================================
// HTTP 命令查询
// ============================================================================
String checkCommandHTTP() {
  HTTPClient http;
  char url[128];
  snprintf(url, sizeof(url), "http://%s:%s/api/devices/%s/cmd/pending", 
           SERVER_HOST, SERVER_PORT, DEVICE_ID);
  
  http.begin(url);
  http.setTimeout(2000);
  int httpCode = http.GET();
  String command = "";
  
  if (httpCode == 200) {
    String response = http.getString();
    if (response.indexOf("\"has_command\":true") >= 0 || 
        response.indexOf("\"has_command\": true") >= 0) {
      int cmdStart = response.indexOf("\"command\":\"");
      if (cmdStart < 0) cmdStart = response.indexOf("\"command\": \"");
      if (cmdStart >= 0) {
        cmdStart = response.indexOf("\"", cmdStart + 10) + 1;
        int cmdEnd = response.indexOf("\"", cmdStart);
        if (cmdEnd > cmdStart) {
          command = response.substring(cmdStart, cmdEnd);
          Serial.print("[HTTP轮询] 收到命令: ");
          Serial.println(command);
        }
      }
    }
  }
  http.end();
  return command;
}

// ============================================================================
// HTTP 命令确认
// ============================================================================
bool ackCommandHTTP(const char* command) {
  HTTPClient http;
  char url[128];
  snprintf(url, sizeof(url), "http://%s:%s/api/devices/%s/cmd/ack", 
           SERVER_HOST, SERVER_PORT, DEVICE_ID);
  char payload[128];
  snprintf(payload, sizeof(payload), "{\"command\":\"%s\"}", command);
  
  http.begin(url);
  http.addHeader("Content-Type", "application/json");
  http.setTimeout(2000);
  int httpCode = http.POST(payload);
  bool success = (httpCode == 200);
  http.end();
  return success;
}

// ============================================================================
// MQTT 回调（保留备用）
// ============================================================================
void mqttCallback(char* topic, byte* payload, unsigned int length) {
  String message = "";
  for (unsigned int i = 0; i < length; i++) {
    message += (char)payload[i];
  }
  Serial.print("[MQTT] 收到 ["); Serial.print(topic); Serial.print("] → ");
  Serial.println(message);

  if (message.equals("buzzer_off")) {
    userSilencedBuzzer = true;
    buzzerContinuousOff();
    Serial.println("[MQTT命令] 蜂鸣器静音");
  } else if (message.equals("buzzer_on")) {
    userSilencedBuzzer = false;
    buzzerContinuousOn(2000);
  } else if (message.equals("led_off")) {
    userSilencedLed = true;
    digitalWrite(PIN_LED_RED, LOW);
    digitalWrite(PIN_LED_YELLOW, LOW);
  } else if (message.equals("led_on")) {
    userSilencedLed = false;
    updateLEDs();
  }
}

// ============================================================================
// 低功耗管理（不变）
// ============================================================================
void printWakeupReason() {
  switch(wakeupReason) {
    case ESP_SLEEP_WAKEUP_EXT0:
      Serial.println("[唤醒] 原因: TCS3472颜色异常中断");
      break;
    case ESP_SLEEP_WAKEUP_TIMER:
      Serial.println("[唤醒] 原因: RTC定时器唤醒");
      break;
    default:
      Serial.println("[唤醒] 原因: 首次上电/复位");
      break;
  }
}

void configureTCSInterrupt() {
  Serial.println("[TCS3472] 配置中断唤醒阈值...");
  Wire.setPins(PIN_I2C_SDA, PIN_I2C_SCL);
  Wire.begin();
  if (!tcs.begin()) {
    Serial.println("[TCS3472] 传感器未响应，跳过");
    return;
  }
  uint16_t lowThreshold = 0x0000;
  uint16_t highThreshold = 0xFF00;
  Wire.beginTransmission(TCS34725_ADDRESS);
  Wire.write(0xA4);
  Wire.write(lowThreshold & 0xFF);
  Wire.write((lowThreshold >> 8) & 0xFF);
  Wire.write(highThreshold & 0xFF);
  Wire.write((highThreshold >> 8) & 0xFF);
  Wire.endTransmission();
  Wire.beginTransmission(TCS34725_ADDRESS);
  Wire.write(TCS34725_PERS);
  Wire.write(0x01);
  Wire.endTransmission();
  clearTCSInterrupt();
  Wire.beginTransmission(TCS34725_ADDRESS);
  Wire.write(TCS34725_ENABLE | TCS34725_COMMAND_BIT);
  Wire.write(TCS34725_ENABLE_PON | TCS34725_ENABLE_AEN | TCS34725_ENABLE_AIEN);
  Wire.endTransmission();
  pinMode(PIN_TCS3472_LED, OUTPUT);
  digitalWrite(PIN_TCS3472_LED, LOW);
  rtc_gpio_pullup_en((gpio_num_t)PIN_TCS3472_INT);
  rtc_gpio_pulldown_dis((gpio_num_t)PIN_TCS3472_INT);
  esp_sleep_enable_ext0_wakeup((gpio_num_t)PIN_TCS3472_INT, 0);
  Serial.println("[TCS3472] 中断唤醒已配置");
}

void clearTCSInterrupt() {
  Wire.beginTransmission(TCS34725_ADDRESS);
  Wire.write(TCS34725_COMMAND_BIT | 0x66);
  Wire.endTransmission();
}

void enterDeepSleep() {
  esp_sleep_enable_timer_wakeup(SLEEP_INTERVAL_US);
  Serial.print("[低功耗] 进入Deep-Sleep，周期: ");
  Serial.print(SLEEP_INTERVAL_US / 1000000ULL);
  Serial.println(" 秒");
  Serial.println("========================================\n");
  delay(100);
  Serial.flush();
  esp_deep_sleep_start();
}

void powerOffPeripherals() {
  Serial.println("[电源] 关闭外设...");
  ledcDetachPin(PIN_BUZZER);
  pinMode(PIN_BUZZER, OUTPUT);
  digitalWrite(PIN_BUZZER, HIGH);
  digitalWrite(PIN_LED_YELLOW, LOW);
  digitalWrite(PIN_LED_RED, LOW);
  pinMode(PIN_LED_YELLOW, INPUT);
  pinMode(PIN_LED_RED, INPUT);
  digitalWrite(PIN_TCS3472_LED, LOW);
  pinMode(PIN_TCS3472_LED, INPUT);
  display.ssd1306_command(SSD1306_DISPLAYOFF);
  digitalWrite(PIN_HCSR04_TRIG, LOW);
  pinMode(PIN_HCSR04_TRIG, INPUT);
  pinMode(PIN_HCSR04_ECHO, INPUT);
  Wire.end();
  Serial.println("[电源] 外设已关闭");
}

void powerOnPeripherals() {
  Serial.println("[电源] 初始化外设...");
  Wire.setPins(PIN_I2C_SDA, PIN_I2C_SCL);
  Wire.begin();
  pinMode(PIN_TCS3472_LED, OUTPUT);
  digitalWrite(PIN_TCS3472_LED, HIGH);

  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("[OLED] 初始化失败!"));
  } else {
    Serial.println("[OLED] 初始化成功");
    display.ssd1306_command(SSD1306_DISPLAYON);
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 0);
    display.println(F("Fire Monitor v5"));
    display.print(F("Boot #")); display.println(bootCount);
    display.display();
    delay(300);
  }

  if (tcs.begin()) {
    Serial.println("[TCS3472] 初始化成功");
    tcs.setInterrupt(true);
    tcs.clearInterrupt();
    tcs.setIntegrationTime(TCS34725_INTEGRATIONTIME_101MS);
    tcs.setGain(TCS34725_GAIN_4X);
  } else {
    Serial.println("[TCS3472] 颜色传感器未找到!");
  }

  pinMode(PIN_TCS3472_INT, INPUT_PULLUP);
  pinMode(PIN_HCSR04_TRIG, OUTPUT);
  pinMode(PIN_HCSR04_ECHO, INPUT);
  digitalWrite(PIN_HCSR04_TRIG, LOW);
  tempSensor.begin();

  ledcSetup(PWM_CHANNEL_BUZZER, PWM_FREQ_BUZZER, PWM_RES_BUZZER);
  pinMode(PIN_BUZZER, OUTPUT);
  digitalWrite(PIN_BUZZER, HIGH);

  pinMode(PIN_LED_YELLOW, OUTPUT);
  pinMode(PIN_LED_RED, OUTPUT);
  digitalWrite(PIN_LED_YELLOW, LOW);
  digitalWrite(PIN_LED_RED, LOW);
  Serial.println("[电源] 所有外设初始化完成");
}

// ============================================================================
// WiFi & MQTT
// ============================================================================
void setupWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  unsigned long startAttempt = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - startAttempt < WIFI_CONNECT_TIMEOUT_MS) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();
}

void setupMQTT() {
  mqttClient.setServer(MQTT_SERVER, MQTT_PORT);
  mqttClient.setBufferSize(512);
  mqttClient.setCallback(mqttCallback);
}

bool connectMQTT() {
  Serial.print("[MQTT] 连接服务器...");
  unsigned long startAttempt = millis();
  while (!mqttClient.connected() && millis() - startAttempt < MQTT_CONNECT_TIMEOUT_MS) {
    if (mqttClient.connect(MQTT_CLIENT_ID, "", "")) {
      Serial.println("成功!");
      mqttClient.subscribe("device/cmd");
      mqttClient.subscribe("device/EX-001/cmd");
      mqttClient.subscribe("device/+/cmd");
      return true;
    }
    Serial.print(".");
    delay(1000);
  }
  Serial.println("失败!");
  return false;
}

void publishSensorData() {
  char payload[256];
  snprintf(payload, sizeof(payload),
    "{"
    "\"device_id\":\"%s\","
    "\"distance\":%.1f,"
    "\"temperature\":%.1f,"
    "\"color_r\":%d,"
    "\"color_g\":%d,"
    "\"color_b\":%d,"
    "\"color_status\":\"%s\","
    "\"alarm\":%s,"
    "\"alarm_reason\":\"%s\","
    "\"boot_count\":%d"
    "}",
    DEVICE_ID, distance_cm, temperature_c, r, g, b,
    getColorText(detected_color),
    alarmTriggered ? "true" : "false",
    alarmReason.c_str(), bootCount
  );
  Serial.print("[MQTT] 上报: ");
  Serial.println(payload);
  mqttClient.publish(TOPIC_PUB, payload);
}

void publishSensorDataHTTP() {
  if (WiFi.status() != WL_CONNECTED) return;
  HTTPClient http;
  char payload[512];
  snprintf(payload, sizeof(payload),
    "{"
    "\"device_id\":\"%s\","
    "\"distance\":%.1f,"
    "\"temperature\":%.1f,"
    "\"color_r\":%d,"
    "\"color_g\":%d,"
    "\"color_b\":%d,"
    "\"color_status\":\"%s\","
    "\"alarm\":%s,"
    "\"alarm_reason\":\"%s\","
    "\"boot_count\":%d,"
    "\"location\":\"灭火器位置\""
    "}",
    DEVICE_ID, distance_cm, temperature_c, r, g, b,
    getColorText(detected_color),
    alarmTriggered ? "true" : "false",
    alarmReason.c_str(), bootCount
  );
  http.begin(HTTP_SERVER_URL);
  http.addHeader("Content-Type", "application/json");
  http.setTimeout(5000);
  int httpCode = http.POST(payload);
  Serial.printf("[HTTP] 响应码: %d\n", httpCode);
  http.end();
}

// ============================================================================
// 传感器驱动
// ============================================================================
float measureDistance() {
  digitalWrite(PIN_HCSR04_TRIG, LOW);
  delayMicroseconds(2);
  digitalWrite(PIN_HCSR04_TRIG, HIGH);
  delayMicroseconds(10);
  digitalWrite(PIN_HCSR04_TRIG, LOW);
  long duration = pulseIn(PIN_HCSR04_ECHO, HIGH, 50000);
  if (duration == 0) return -1.0;
  float distance = (duration * 0.034) / 2.0;
  if (distance < 2.0 || distance > 400.0) return -1.0;
  return distance;
}

void readColor() {
  digitalWrite(PIN_TCS3472_LED, HIGH);
  delay(10);
  tcs.getRawData(&r, &g, &b, &c);
  if (r >= 0xFF00 && g >= 0xFF00 && b >= 0xFF00) {
    Serial.println("[TCS3472] 警告: 数据饱和");
    r = g = b = c = 0;
    return;
  }
  if (r == 0 && g == 0 && b == 0 && c == 0) {
    Serial.println("[TCS3472] 警告: 数据全零");
    return;
  }
}

void readTemperature() {
  tempSensor.requestTemperatures();
  temperature_c = tempSensor.getTempCByIndex(0);
  if (temperature_c == DEVICE_DISCONNECTED_C || temperature_c == 85.0) {
    Serial.println("[DS18B20] 读取失败");
    temperature_c = -127.0;
  }
}

int detectColor() {
  if (r == 0 && g == 0 && b == 0) return -1;
  if ((int)r - (int)b > 100 && (int)g - (int)b > 100 && abs((int)r - (int)g) <= 200) return 1;
  if (r > g) return 2;
  return 0;
}

const char* getColorText(int color) {
  switch(color) {
    case 0: return "Green";
    case 1: return "Yellow";
    case 2: return "Red";
    default: return "Unknown";
  }
}

void updateDisplay() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);

  display.setCursor(0, 0);
  display.print(F("Dist:"));
  if (distance_cm < 0) display.print(F("Error"));
  else { display.print(distance_cm, 1); display.print(F("cm")); }

  display.setCursor(0, 10);
  display.print(F("Temp:"));
  if (temperature_c <= -100) display.print(F("Error"));
  else { display.print(temperature_c, 1); display.print(F("C")); }

  display.setCursor(0, 20);
  display.print(F("Color:"));
  if (detected_color == 0) display.print(F("Green"));
  else if (detected_color == 1) display.print(F("Yellow"));
  else if (detected_color == 2) display.print(F("Red"));
  else display.print(F("Unknown"));

  display.setCursor(0, 30);
  display.print(F("R:")); display.print(r);
  display.print(F(" G:")); display.print(g);

  display.setCursor(0, 40);
  display.print(F("B:")); display.print(b);
  display.print(F(" C:")); display.print(c);

  // 第6行：报警状态 + 用户静音提示
  display.setCursor(0, 52);
  if (!alarmTriggered) {
    bool sensorError = (distance_cm < 0) || (temperature_c <= -100);
    if (sensorError) {
      if (distance_cm < 0 && temperature_c <= -100) display.print(F("ERR:DIST+TEMP"));
      else if (distance_cm < 0) display.print(F("ERR:DISTANCE"));
      else display.print(F("ERR:TEMP"));
    } else {
      display.print(F("Boot#")); display.print(bootCount);
      display.print(F(" OK"));
    }
  } else if (alarmPriority == 2) {
    if (userSilencedBuzzer) {
      display.print(F("ALARM:MUTED"));  // 用户已静音
    } else if (temperature_c < -20.0 || temperature_c > 60.0) {
      display.print(F("ALARM:TEMP!"));
    } else if (detected_color == 2) {
      display.print(F("ALARM:RED!"));
    } else if (distance_cm > 20.0) {
      display.print(F("ALARM:MISSING!"));
    } else {
      display.print(F("ALARM:HIGH!"));
    }
  } else if (alarmPriority == 1) {
    display.print(F("WARN:YELLOW"));
  } else {
    display.print(F("WARN:UNKNOWN"));
  }
  display.display();
}

void beep(int duration_ms, int freq) {
  ledcAttachPin(PIN_BUZZER, PWM_CHANNEL_BUZZER);
  ledcWriteTone(PWM_CHANNEL_BUZZER, freq);
  delay(duration_ms);
  ledcDetachPin(PIN_BUZZER);
  pinMode(PIN_BUZZER, OUTPUT);
  digitalWrite(PIN_BUZZER, HIGH);
}

void buzzerAlarm(int times) {
  for (int i = 0; i < times; i++) {
    beep(150, 2000);
    delay(100);
  }
}

void buzzerAlarmYellow(int times) {
  for (int i = 0; i < times; i++) {
    beep(200, 1500);
    delay(800);
  }
}

void buzzerContinuousOn(int freq) {
  (void)freq;
  ledcDetachPin(PIN_BUZZER);
  pinMode(PIN_BUZZER, OUTPUT);
  digitalWrite(PIN_BUZZER, LOW);
}

void buzzerContinuousOff() {
  ledcDetachPin(PIN_BUZZER);
  pinMode(PIN_BUZZER, OUTPUT);
  digitalWrite(PIN_BUZZER, HIGH);
}
