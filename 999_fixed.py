import os
import json
import time
import threading
from flask import Flask, request, jsonify, send_from_directory
from flask_cors import CORS
import requests
import paho.mqtt.client as mqtt
from flask_socketio import SocketIO, emit

BASE_DIR = os.path.dirname(os.path.abspath(__file__))

app = Flask(__name__)
CORS(app, resources={r"/api/*": {"origins": "*"}})
socketio = SocketIO(app, cors_allowed_origins="*")

# ========== EMQX MQTT 配置 ==========
# 修改方法: 直接改下面双引号里的值，或者设置环境变量 EMQX_HOST
# 例如: EMQX_HOST = "139.196.153.51"
EMQX_HOST = os.environ.get("EMQX_HOST", "139.196.153.51")
EMQX_PORT = int(os.environ.get("EMQX_PORT", "1883"))
EMQX_USER = os.environ.get("EMQX_USER", "")
EMQX_PASS = os.environ.get("EMQX_PASS", "")
MQTT_TOPIC_SUB = "sensor/data"
MQTT_TOPIC_PUB = "sensor/cmd"

# ========== 豆包大模型配置 ==========
ARK_API_KEY = "ark-4d0602ee-0170-446e-b32e-8ffd26358e7c-3d2b0"
MODEL_NAME = "doubao-1-5-pro-32k-250115"
ARK_URL = "https://ark.cn-beijing.volces.com/api/v3/chat/completions"

# ========== 设备数据存储 ==========
# 格式: { "EX-001": { "id": "EX-001", "temperature": 25, ... }, ... }
device_data = {}
device_lock = threading.Lock()

# ========== MQTT 回调 ==========

def on_connect(client, userdata, flags, rc):
    """连接成功回调"""
    if rc == 0:
        print("✅ MQTT 已连接到 EMQX")
        client.subscribe(MQTT_TOPIC_SUB)
        print(f"📡 已订阅: {MQTT_TOPIC_SUB}")
    else:
        print(f"❌ MQTT 连接失败，返回码: {rc}")


def on_message(client, userdata, msg):
    """收到传感器数据时触发"""
    global device_data

    try:
        payload = json.loads(msg.payload.decode('utf-8'))
        # 硬件数据格式: {distance, temperature, color_r, color_g, color_b, color_status, alarm, alarm_reason, boot_count}
        # 兼容有无id的情况
        device_id = payload.get("id", "EX-001")

        # 补充时间戳和位置信息
        payload["lastSeen"] = int(time.time() * 1000)
        if "location" not in payload:
            payload["location"] = "A栋 1F 大厅"
        # 如果没有id字段，补充一个
        if "id" not in payload:
            payload["id"] = device_id

        # 存储数据（线程安全）
        with device_lock:
            device_data[device_id] = payload

        print(f"📥 [{device_id}] 温度:{payload.get('temperature', '-')}°C "
              f"距离:{payload.get('distance', '-')}cm "
              f"颜色:{payload.get('color_status', '-')} "
              f"报警:{payload.get('alarm', '-')}")

        # 通过 WebSocket 推送给前端（如果有前端连接）
        socketio.emit('sensor_update', payload)

    except Exception as e:
        print(f"⚠️ 消息处理错误: {e}")


def on_disconnect(client, userdata, rc):
    """断开连接回调"""
    print(f"⚠️ MQTT 断开连接 (rc={rc})，5秒后重连...")


# ========== 初始化 MQTT 客户端 ==========

mqtt_client = mqtt.Client()
if EMQX_USER:
    mqtt_client.username_pw_set(EMQX_USER, EMQX_PASS)

mqtt_client.on_connect = on_connect
mqtt_client.on_message = on_message
mqtt_client.on_disconnect = on_disconnect


def mqtt_loop():
    """在后台线程中保持 MQTT 连接"""
    while True:
        try:
            mqtt_client.connect(EMQX_HOST, EMQX_PORT, 60)
            mqtt_client.loop_forever()
        except Exception as e:
            print(f"MQTT 连接异常: {e}")
            time.sleep(5)


# 启动 MQTT 后台线程（仅在配置了 EMQX_HOST 时启动）
if EMQX_HOST:
    mqtt_thread = threading.Thread(target=mqtt_loop, daemon=True)
    mqtt_thread.start()
else:
    print("⚠️ EMQX_HOST 未配置，MQTT 功能已禁用")
    print("   如需启用，请修改代码中 EMQX_HOST = \"你的EMQX地址\"")
    mqtt_client = None


@app.route('/test_ai.html')
def test_ai_page():
    """AI 测试页面"""
    return send_from_directory(BASE_DIR, 'test_ai.html')


@app.route('/')
def index():
    """主页面"""
    return send_from_directory(BASE_DIR, '999.html')


@app.route('/api/devices')
def get_devices():
    """获取所有设备最新数据"""
    with device_lock:
        return jsonify(list(device_data.values()))


@app.route('/api/device/<device_id>')
def get_device(device_id):
    """获取单个设备数据"""
    with device_lock:
        if device_id in device_data:
            return jsonify(device_data[device_id])
        return jsonify({"error": "设备不存在"}), 404


@app.route('/api/ai', methods=['POST', 'OPTIONS'])
def ai_chat():
    """豆包大模型对话"""
    if request.method == 'OPTIONS':
        response = jsonify({})
        response.headers.add('Access-Control-Allow-Origin', '*')
        response.headers.add('Access-Control-Allow-Headers', 'Content-Type')
        response.headers.add('Access-Control-Allow-Methods', 'POST')
        return response

    data = request.get_json(force=True)
    user_query = data.get('query', '')
    sensor_data = data.get('sensor_data', {})

    # 如果没有传 sensor_data，自动取最新数据
    if not sensor_data and device_data:
        latest = max(device_data.values(), key=lambda x: x.get("lastSeen", 0))
        sensor_data = latest

    sensor_info = f"""当前消防监测设备数据：
- 设备ID: {sensor_data.get('id', 'EX-001')}
- 位置: {sensor_data.get('location', 'A栋 1F 大厅')}
- 状态: {sensor_data.get('status', 'normal')}
- 颜色: {sensor_data.get('colorSensor', '未知')}
- 蜂鸣报警: {sensor_data.get('buzzer', False)}
- 超声波距离: {sensor_data.get('ultrasonicDist', 0)} cm
- 温度: {sensor_data.get('temperature', 0)} °C
- 压力: {sensor_data.get('pressure', 0)} MPa
- 湿度: {sensor_data.get('humidity', 0)}%
- 电池: {sensor_data.get('battery', 0)}%"""

    headers = {
        "Authorization": f"Bearer {ARK_API_KEY}",
        "Content-Type": "application/json"
    }

    payload = {
        "model": MODEL_NAME,
        "messages": [
            {
                "role": "system",
                "content": "你是消防监测系统的AI助手。用户会给你传感器数值，请根据这些数值回答用户问题。规则：1.只使用用户提供的数值，不要编造数据 2.不要对颜色做任何类型判断（颜色只是颜色传感器的读数） 3.用中文回答，简洁专业 4.如果用户问具体数值，直接告诉用户"
            },
            {"role": "user", "content": f"{sensor_info}\n\n用户问题：{user_query}"}
        ],
        "stream": False,
        "temperature": 0.3,
        "max_tokens": 800
    }

    try:
        resp = requests.post(ARK_URL, json=payload, headers=headers, timeout=30)
        result = resp.json()
        ai_text = result['choices'][0]['message']['content']
        return jsonify({"success": True, "reply": ai_text})
    except Exception as e:
        print(f"豆包API错误: {e}")
        return jsonify({"success": False, "error": str(e)})


# ========== WebSocket 事件（前端可选连接） ==========

@socketio.on('connect')
def handle_connect():
    """前端 WebSocket 连接时发送当前所有设备数据"""
    print(f"🌐 前端已连接 WebSocket")
    with device_lock:
        for device in device_data.values():
            emit('sensor_update', device)


# ========== 主入口 ==========

if __name__ == '__main__':
    print("=" * 50)
    print("🔥 消防监测系统（EMQX + 豆包AI）")
    if EMQX_HOST:
        print(f"📡 EMQX: {EMQX_HOST}:{EMQX_PORT}")
    else:
        print("📡 EMQX: 未配置（MQTT功能禁用）")
    print("📱 打开: http://localhost:5000")
    print("=" * 50)
    socketio.run(app, host='0.0.0.0', port=5000, debug=True, use_reloader=False, allow_unsafe_werkzeug=True)
