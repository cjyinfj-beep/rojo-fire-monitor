/*
 * ESP32-S3 消防监测系统 - v8（修复跨唤醒静音重置问题）
 * 
 * 修复内容：
 * - v7 用 millis() 记录静音时间，但 Deep-Sleep 后 millis() 从 0 开始，导致误判超时
 * - v8 改用 bootCount 记录静音开始，不受 Deep-Sleep 影响
 * 
 * 核心功能：
 * 1. 用户关闭蜂鸣器/LED 后，跨 Deep-Sleep 保持（直到手动开启或30次唤醒后超时）
 * 2. 蜂鸣器和LED完全独立控制
 * 3. 颜色滤波：连续3次相同颜色才确认
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

// ==================== 颜色滤波配置 ====================
#define COLOR_FILTER_COUNT      3

// ==================== 静音超时配置 ====================
// 用 bootCount 计数，30次唤醒后自动取消（约30分钟，每次唤醒60秒）
#define SILENCE_TIMEOUT_BOOTS 30

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

// ==================== RTC 内存变量（Deep-Sleep 不掉电）====================
RTC_DATA_ATTR int bootCount = 0;
RTC_DATA_ATTR bool userSilencedBuzzer = false;
RTC_DATA_ATTR bool userSilencedLed = false;
RTC_DATA_ATTR uint32_t silenceBootCount = 0;  // 静音开始时的 bootCount

// ==================== 运行时变量 - 传感器 ====================
float distance_cm = 0.0;
float temperature_c = 0.0;
uint16_t r = 0, g = 0, b = 0, c = 0;
int raw_color = -1;

// ==================== 运行时变量 - 颜色滤波确认值 ====================
uint16_t confirmed_r = 0, confirmed_g = 0, confirmed_b = 0;
int confirmed_color = -1;
int colorHistory[COLOR_FILTER_COUNT] = {-1, -1, -1};
int colorHistoryIndex = 0;

// ==================== 运行时变量 - 报警状态 ====================
bool alarmTriggered = false;
int alarmPriority = 0;
String alarmReason = "";

WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);
esp_sleep_wakeup_cause_t wakeupReason;

// ==================== 函数声明 ====================
float measureDistance();
void readColor();
void readTemperature();
int detectColor(uint16_t rv, uint16_t gv, uint16_t bv);
void updateColorFilter();
const char* getColorText(int color);

void checkSilenceTimeout();
void updateDisplay();
void updateLEDs();
void checkAlarm();

void beep(int duration_ms, int freq = 2000);
void buzzerAlarm(int times = 3);
void buzzerAlarmYellow(int times = 3);
void buzzerContinuousOn(int freq = 2000);
void buzzerContinuousOff();

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
  Serial.println("消防监测系统 - v8 修复跨唤醒静音版");
  Serial.print("启动次数: "); Serial.println(bootCount);
  printWakeupReason();
  Serial.println("========================================\n");

  // 关键修复：检查静音是否超时（用 bootCount 计数，不受 millis() 重启影响）
  checkSilenceTimeout();

  // 重置颜色滤波器
  for (int i = 0; i < COLOR_FILTER_COUNT; i++) {
    colorHistory[i] = -1;
  }
  colorHistoryIndex = 0;
  confirmed_color = -1;

  powerOnPeripherals();

  Serial.println("[采集] 开始传感器数据采集...");
  distance_cm = measureDistance();
  readTemperature();

  // 连续读取颜色3次进行滤波初始化
  Serial.println("[颜色滤波] 连续采样3次进行初始化...");
  for (int i = 0; i < COLOR_FILTER_COUNT; i++) {
    readColor();
    raw_color = detectColor(r, g, b);
    colorHistory[i] = raw_color;
    Serial.print("  采样#"); Serial.print(i+1); Serial.print(": ");
    Serial.println(getColorText(raw_color));
    if (i < COLOR_FILTER_COUNT - 1) delay(100);
  }
  
  if (colorHistory[0] == colorHistory[1] && colorHistory[1] == colorHistory[2]) {
    confirmed_color = colorHistory[0];
    confirmed_r = r; confirmed_g = g; confirmed_b = b;
    Serial.println("[颜色滤波] 初始化确认: 3次一致");
  } else {
    confirmed_color = raw_color;
    confirmed_r = r; confirmed_g = g; confirmed_b = b;
    Serial.println("[颜色滤波] 初始化: 3次不一致，使用最后一次");
  }
  colorHistoryIndex = 0;

  Serial.println("----------------------------------------");
  Serial.print("距离: "); Serial.print(distance_cm); Serial.println(" cm");
  Serial.print("温度: "); Serial.print(temperature_c); Serial.println(" C");
  Serial.print("颜色(实时): R:"); Serial.print(r);
  Serial.print(" G:"); Serial.print(g);
  Serial.print(" B:"); Serial.print(b);
  Serial.print(" C:"); Serial.println(c);
  Serial.print("颜色判断(实时): "); Serial.println(getColorText(raw_color));
  Serial.print("颜色判断(确认): "); Serial.println(getColorText(confirmed_color));
  if (userSilencedBuzzer) {
    Serial.print("[状态] 蜂鸣器: 用户静音中 (已保持 ");
    Serial.print(bootCount - silenceBootCount);
    Serial.println(" 次唤醒)");
  }
  if (userSilencedLed) {
    Serial.print("[状态] LED: 用户关闭中 (已保持 ");
    Serial.print(bootCount - silenceBootCount);
    Serial.println(" 次唤醒)");
  }
  Serial.println("----------------------------------------\n");

  checkAlarm();
  updateDisplay();

  // 初始报警触发
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

  // ====== 报警保持循环 ======
  if (alarmTriggered) {
    Serial.println("[低功耗] 报警状态，保持清醒30秒...");
    Serial.println("[控制] 蜂鸣器和LED独立控制，互不影响");
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
            Serial.println("[命令] 用户关闭蜂鸣器 → 跨唤醒保持静音");
            userSilencedBuzzer = true;
            silenceBootCount = bootCount;  // 记录当前 bootCount
            buzzerContinuousOff();
            continuousOn = false;
            ackCommandHTTP("buzzer_off");
            updateDisplay();
            
          } else if (cmd == "buzzer_on") {
            Serial.println("[命令] 用户开启蜂鸣器");
            userSilencedBuzzer = false;
            silenceBootCount = 0;
            ackCommandHTTP("buzzer_on");
            updateDisplay();
            
          } else if (cmd == "led_off") {
            Serial.println("[命令] 用户关闭LED → 跨唤醒保持关闭");
            userSilencedLed = true;
            silenceBootCount = bootCount;
            digitalWrite(PIN_LED_RED, LOW);
            digitalWrite(PIN_LED_YELLOW, LOW);
            ackCommandHTTP("led_off");
            
          } else if (cmd == "led_on") {
            Serial.println("[命令] 用户开启LED");
            userSilencedLed = false;
            silenceBootCount = 0;
            updateLEDs();
            ackCommandHTTP("led_on");
          }
        }
      }

      // === 2. MQTT 轮询（备用）===
      if (mqttConnected) {
        mqttClient.loop();
      }

      // === 3. 重新采集传感器数据 ===
      distance_cm = measureDistance();
      readTemperature();
      readColor();
      raw_color = detectColor(r, g, b);
      updateColorFilter();

      // === 4. 使用确认颜色重新评估报警 ===
      checkAlarm();
      updateLEDs();

      if (!alarmTriggered) {
        Serial.println("[低功耗] 报警已解除, 退出告警循环");
        // 报警解除，重置用户静音状态（下次新报警可以重新响应）
        userSilencedBuzzer = false;
        userSilencedLed = false;
        silenceBootCount = 0;
        Serial.println("[静音] 报警已解除，重置用户静音状态");
        break;
      }

      updateDisplay();

      // === 5. 蜂鸣器控制 ===
      if (alarmPriority == 2) {
        if (!userSilencedBuzzer && !continuousOn) {
          buzzerContinuousOn(2000);
          continuousOn = true;
          Serial.println("[蜂鸣器] 持续响 ON");
        } else if (userSilencedBuzzer && continuousOn) {
          buzzerContinuousOff();
          continuousOn = false;
          Serial.println("[蜂鸣器] 用户静音 → 持续响 OFF");
        }
        delay(1000);
        
      } else if (alarmPriority == 1) {
        if (continuousOn) {
          buzzerContinuousOff();
          continuousOn = false;
        }
        if (!userSilencedBuzzer && millis() - lastBeepTime >= 8000) {
          lastBeepTime = millis();
          buzzerAlarmYellow(2);
        }
        delay(3000);
      }

      // === 6. 每10秒持续上报 ===
      if (millis() - lastReportTime >= 10000) {
        lastReportTime = millis();
        if (mqttConnected) {
          if (!mqttClient.connected()) mqttClient.connect(MQTT_CLIENT_ID, "", "");
          if (mqttClient.connected()) {
            publishSensorData();
          }
        }
        if (WiFi.status() == WL_CONNECTED) {
          publishSensorDataHTTP();
        }
      }
    }

    if (continuousOn) {
      buzzerContinuousOff();
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
// 静音超时检查（关键修复：用 bootCount 代替 millis()）
// ============================================================================
void checkSilenceTimeout() {
  if (!userSilencedBuzzer && !userSilencedLed) return;
  
  // 用 bootCount 差值判断超时，不受 Deep-Sleep 后 millis() 重启影响
  if (silenceBootCount > 0 && (bootCount - silenceBootCount) > SILENCE_TIMEOUT_BOOTS) {
    Serial.println("[静音超时] 30次唤醒后自动取消静音状态");
    userSilencedBuzzer = false;
    userSilencedLed = false;
    silenceBootCount = 0;
  } else if (userSilencedBuzzer || userSilencedLed) {
    Serial.print("[静音状态] 已保持 ");
    Serial.print(bootCount - silenceBootCount);
    Serial.println(" 次唤醒");
  }
}

// ============================================================================
// 颜色检测与滤波
// ============================================================================
int detectColor(uint16_t rv, uint16_t gv, uint16_t bv) {
  if (rv == 0 && gv == 0 && bv == 0) return -1;
  if ((int)rv - (int)bv > 100 && (int)gv - (int)bv > 100 && abs((int)rv - (int)gv) <= 200) return 1;
  if (rv > gv) return 2;
  return 0;
}

void updateColorFilter() {
  colorHistory[colorHistoryIndex] = raw_color;
  colorHistoryIndex = (colorHistoryIndex + 1) % COLOR_FILTER_COUNT;
  
  bool allValid = true;
  for (int i = 0; i < COLOR_FILTER_COUNT; i++) {
    if (colorHistory[i] == -1) {
      allValid = false;
      break;
    }
  }
  if (!allValid) return;
  
  bool allSame = true;
  for (int i = 1; i < COLOR_FILTER_COUNT; i++) {
    if (colorHistory[i] != colorHistory[0]) {
      allSame = false;
      break;
    }
  }
  
  if (allSame && confirmed_color != colorHistory[0]) {
    Serial.print("[颜色滤波] 确认变化: ");
    Serial.print(getColorText(confirmed_color));
    Serial.print(" -> ");
    Serial.println(getColorText(colorHistory[0]));
    confirmed_color = colorHistory[0];
    confirmed_r = r;
    confirmed_g = g;
    confirmed_b = b;
  }
}

const char* getColorText(int color) {
  switch(color) {
    case 0: return "Green";
    case 1: return "Yellow";
    case 2: return "Red";
    default: return "Unknown";
  }
}

// ============================================================================
// 报警判断
// ============================================================================
void checkAlarm() {
  alarmTriggered = false;
  alarmPriority = 0;
  alarmReason = "";

  if (temperature_c < -20.0 || temperature_c > 60.0) {
    alarmTriggered = true;
    alarmPriority = 2;
    alarmReason += (temperature_c < -20.0) ? "LOW_TEMP;" : "HIGH_TEMP;";
  }
  if (confirmed_color == 2) {
    alarmTriggered = true;
    alarmPriority = 2;
    alarmReason += "RED_COLOR;";
  }
  if (distance_cm > 20.0) {
    alarmTriggered = true;
    alarmPriority = 2;
    alarmReason += "FAR_DISTANCE;";
  }
  if (confirmed_color == 1 && alarmPriority < 2) {
    alarmTriggered = true;
    alarmPriority = 1;
    alarmReason += "YELLOW_COLOR;";
  }
}

// ============================================================================
// OLED 显示
// ============================================================================
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
  if (raw_color == 0) display.print(F("Green"));
  else if (raw_color == 1) display.print(F("Yellow"));
  else if (raw_color == 2) display.print(F("Red"));
  else display.print(F("Unknown"));

  display.setCursor(0, 30);
  display.print(F("R:")); display.print(r);
  display.print(F(" G:")); display.print(g);

  display.setCursor(0, 40);
  display.print(F("B:")); display.print(b);
  display.print(F(" C:")); display.print(c);

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
    if (userSilencedBuzzer && userSilencedLed) {
      display.print(F("ALARM:ALL_OFF"));
    } else if (userSilencedBuzzer) {
      display.print(F("ALARM:BUZ_OFF"));
    } else if (userSilencedLed) {
      display.print(F("ALARM:LED_OFF"));
    } else if (temperature_c < -20.0 || temperature_c > 60.0) {
      display.print(F("ALARM:TEMP!"));
    } else if (confirmed_color == 2) {
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

// ============================================================================
// LED 更新
// ============================================================================
void updateLEDs() {
  if (userSilencedLed) {
    digitalWrite(PIN_LED_YELLOW, LOW);
    digitalWrite(PIN_LED_RED, LOW);
    return;
  }

  if (raw_color == 1) {
    digitalWrite(PIN_LED_YELLOW, HIGH);
    digitalWrite(PIN_LED_RED, LOW);
  } else if (raw_color == 2) {
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
// MQTT 回调
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
    silenceBootCount = bootCount;
    buzzerContinuousOff();
  } else if (message.equals("buzzer_on")) {
    userSilencedBuzzer = false;
    silenceBootCount = 0;
  } else if (message.equals("led_off")) {
    userSilencedLed = true;
    silenceBootCount = bootCount;
    digitalWrite(PIN_LED_RED, LOW);
    digitalWrite(PIN_LED_YELLOW, LOW);
  } else if (message.equals("led_on")) {
    userSilencedLed = false;
    silenceBootCount = 0;
    updateLEDs();
  }
}

// ============================================================================
// 低功耗管理
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
    display.println(F("Fire Monitor v8"));
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
    DEVICE_ID, distance_cm, temperature_c, 
    confirmed_r, confirmed_g, confirmed_b,
    getColorText(confirmed_color),
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
    DEVICE_ID, distance_cm, temperature_c,
    confirmed_r, confirmed_g, confirmed_b,
    getColorText(confirmed_color),
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
