import paho.mqtt.client as mqtt
import json

# MQTT 配置（与后端一致）
HOST = "139.196.153.51"
PORT = 1883
TOPIC = "sensor/data"

# 模拟设备数据
payload = {
    "id": "EX-001",
    "distance": 45,
    "temperature": 28,
    "color_r": 255,
    "color_g": 100,
    "color_b": 50,
    "color_status": "orange",
    "alarm": False,
    "alarm_reason": "",
    "boot_count": 1,
    "location": "A栋 1F 大厅"
}

client = mqtt.Client()
client.connect(HOST, PORT, 60)
client.publish(TOPIC, json.dumps(payload))
print(f"✅ 已发布模拟数据到 {TOPIC}")
print(f"   设备: {payload['id']}, 温度: {payload['temperature']}°C, 距离: {payload['distance']}cm")
client.disconnect()
