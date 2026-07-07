import os
import json
import time
from datetime import datetime
from flask import Flask, request, jsonify
from flask_cors import CORS
import psycopg2
import psycopg2.extras
import paho.mqtt.client as mqtt

app = Flask(__name__)
CORS(app, resources={r"/api/*": {"origins": "*"}})

DATABASE_URL = os.environ.get("DATABASE_URL", "postgresql://user:password@localhost:5432/rojo_fire")
EMQX_HOST = os.environ.get("EMQX_HOST", "localhost")
EMQX_PORT = int(os.environ.get("EMQX_PORT", "1883"))

device_cache = {}

pending_commands = {}

def cleanup_expired_commands():
    now = time.time()
    expired = [k for k, v in pending_commands.items() if now - v["timestamp"] > 60]
    for k in expired:
        del pending_commands[k]
        print(f'[命令队列] 清理过期命令: {k}')

def get_db():
    return psycopg2.connect(DATABASE_URL)

def init_db():
    conn = get_db()
    cur = conn.cursor()
    cur.execute("""
    CREATE TABLE IF NOT EXISTS devices (
        id VARCHAR(32) PRIMARY KEY,
        name VARCHAR(64) DEFAULT '',
        location VARCHAR(128) DEFAULT 'A栋 1F 大厅',
        temperature FLOAT DEFAULT 0,
        distance FLOAT DEFAULT 0,
        color_r INT DEFAULT 0,
        color_g INT DEFAULT 0,
        color_b INT DEFAULT 0,
        color_status VARCHAR(16) DEFAULT 'Unknown',
        alarm BOOLEAN DEFAULT FALSE,
        alarm_reason VARCHAR(255) DEFAULT '',
        boot_count INT DEFAULT 0,
        last_seen TIMESTAMP DEFAULT NOW()
    );
    """)
    cur.execute("""
    CREATE TABLE IF NOT EXISTS sensor_history (
        id SERIAL PRIMARY KEY,
        device_id VARCHAR(32) NOT NULL,
        temperature FLOAT DEFAULT 0,
        distance FLOAT DEFAULT 0,
        color_r INT DEFAULT 0,
        color_g INT DEFAULT 0,
        color_b INT DEFAULT 0,
        color_status VARCHAR(16) DEFAULT 'Unknown',
        alarm BOOLEAN DEFAULT FALSE,
        alarm_reason VARCHAR(255) DEFAULT '',
        created_at TIMESTAMP DEFAULT NOW()
    );
    CREATE INDEX IF NOT EXISTS idx_history_device_time ON sensor_history(device_id, created_at DESC);
    """)
    cur.execute("""
    CREATE TABLE IF NOT EXISTS alarms (
        id SERIAL PRIMARY KEY,
        device_id VARCHAR(32) NOT NULL,
        alarm_type VARCHAR(32) DEFAULT '',
        detail TEXT DEFAULT '',
        is_resolved BOOLEAN DEFAULT FALSE,
        created_at TIMESTAMP DEFAULT NOW(),
        resolved_at TIMESTAMP
    );
    CREATE INDEX IF NOT EXISTS idx_alarms_device ON alarms(device_id, created_at DESC);
    """)
    conn.commit()
    cur.close()
    conn.close()
    print("数据库初始化完成")

@app.route('/api/mqtt/webhook', methods=['POST'])
def mqtt_webhook():
    data = request.get_json(force=True)
    payload = data.get('payload', data)
    device_id = payload.get('id', 'EX-001')
    temperature = float(payload.get('temperature', 0) or 0)
    distance = float(payload.get('distance', 0) or 0)
    color_r = int(payload.get('color_r', 0) or 0)
    color_g = int(payload.get('color_g', 0) or 0)
    color_b = int(payload.get('color_b', 0) or 0)
    color_status = payload.get('color_status', 'Unknown')
    alarm = bool(payload.get('alarm', False))
    alarm_reason = payload.get('alarm_reason', '')
    boot_count = int(payload.get('boot_count', 0) or 0)

    conn = get_db()
    cur = conn.cursor()
    cur.execute("""
    INSERT INTO devices (id, name, temperature, distance, color_r, color_g, color_b,
                        color_status, alarm, alarm_reason, boot_count, last_seen)
    VALUES (%s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, NOW())
    ON CONFLICT (id) DO UPDATE SET
        temperature = EXCLUDED.temperature,
        distance = EXCLUDED.distance,
        color_r = EXCLUDED.color_r,
        color_g = EXCLUDED.color_g,
        color_b = EXCLUDED.color_b,
        color_status = EXCLUDED.color_status,
        alarm = EXCLUDED.alarm,
        alarm_reason = EXCLUDED.alarm_reason,
        boot_count = EXCLUDED.boot_count,
        last_seen = NOW();
    """, (device_id, device_id, temperature, distance, color_r, color_g, color_b,
          color_status, alarm, alarm_reason, boot_count))
    cur.execute("""
    INSERT INTO sensor_history (device_id, temperature, distance, color_r, color_g, color_b,
                                color_status, alarm, alarm_reason)
    VALUES (%s, %s, %s, %s, %s, %s, %s, %s, %s);
    """, (device_id, temperature, distance, color_r, color_g, color_b,
          color_status, alarm, alarm_reason))
    if alarm:
        cur.execute("""
        INSERT INTO alarms (device_id, alarm_type, detail)
        VALUES (%s, %s, %s);
        """, (device_id, 'SENSOR_ALERT', alarm_reason))
    conn.commit()
    cur.close()
    conn.close()

    device_cache[device_id] = {
        "id": device_id, "temperature": temperature, "distance": distance,
        "color_r": color_r, "color_g": color_g, "color_b": color_b,
        "color_status": color_status, "alarm": alarm,
        "alarm_reason": alarm_reason, "boot_count": boot_count,
        "lastSeen": int(time.time() * 1000)
    }
    return jsonify({"success": True, "device_id": device_id})

@app.route('/api/devices')
def get_devices():
    conn = get_db()
    cur = conn.cursor(cursor_factory=psycopg2.extras.RealDictCursor)
    cur.execute("SELECT id, name, location, temperature, distance, color_r, color_g, color_b, color_status, alarm, alarm_reason, boot_count, last_seen FROM devices ORDER BY last_seen DESC;")
    rows = cur.fetchall()
    cur.close()
    conn.close()
    for r in rows:
        r['lastSeen'] = r['last_seen'].isoformat() if r['last_seen'] else None
        r['name'] = r['name'] or r['id']
        r['location'] = r['location'] or 'A栋 1F 大厅'
    return jsonify(list(rows))

@app.route('/api/devices/<device_id>')
def get_device(device_id):
    conn = get_db()
    cur = conn.cursor(cursor_factory=psycopg2.extras.RealDictCursor)
    cur.execute("SELECT id, name, location, temperature, distance, color_r, color_g, color_b, color_status, alarm, alarm_reason, boot_count, last_seen FROM devices WHERE id = %s;", (device_id,))
    row = cur.fetchone()
    cur.close()
    conn.close()
    if not row:
        return jsonify({"error": "设备不存在"}), 404
    row['lastSeen'] = row['last_seen'].isoformat() if row['last_seen'] else None
    row['name'] = row['name'] or row['id']
    row['location'] = row['location'] or 'A栋 1F 大厅'
    return jsonify(dict(row))

@app.route('/api/devices/<device_id>/history')
def get_history(device_id):
    hours = request.args.get('hours', 24, type=int)
    conn = get_db()
    cur = conn.cursor(cursor_factory=psycopg2.extras.RealDictCursor)
    cur.execute("SELECT temperature, distance, color_status, alarm, alarm_reason, created_at FROM sensor_history WHERE device_id = %s AND created_at > NOW() - INTERVAL '%s hours' ORDER BY created_at DESC LIMIT 500;", (device_id, hours))
    rows = cur.fetchall()
    cur.close()
    conn.close()
    for r in rows:
        r['time'] = r['created_at'].isoformat() if r['created_at'] else None
    return jsonify(list(rows))

@app.route('/api/devices/<device_id>/alarms')
def get_alarms(device_id):
    conn = get_db()
    cur = conn.cursor(cursor_factory=psycopg2.extras.RealDictCursor)
    cur.execute("SELECT id, alarm_type, detail, is_resolved, created_at, resolved_at FROM alarms WHERE device_id = %s ORDER BY created_at DESC LIMIT 100;", (device_id,))
    rows = cur.fetchall()
    cur.close()
    conn.close()
    return jsonify(list(rows))

@app.route('/api/devices/<device_id>/cmd', methods=['POST'])
def send_command(device_id):
    data = request.get_json()
    command = data.get('command')
    if not command:
        return jsonify({'error': '缺少command字段'}), 400
    
    pending_commands[device_id] = {
        'command': command,
        'timestamp': time.time(),
        'executed': False
    }
    
    print(f'[命令队列] {device_id}: {command}')
    
    try:
        client = mqtt.Client()
        client.connect(EMQX_HOST, EMQX_PORT, 60)
        client.publish(f'device/{device_id}/cmd', command)
        client.disconnect()
    except Exception as e:
        print(f'[MQTT] 可选发送失败: {e}')
    
    return jsonify({
        'success': True,
        'device_id': device_id,
        'command': command
    })

@app.route('/api/devices/<device_id>/cmd/pending', methods=['GET'])
def get_pending_command(device_id):
    cmd = pending_commands.get(device_id)
    if cmd and not cmd['executed']:
        return jsonify({
            'has_command': True,
            'command': cmd['command'],
            'timestamp': cmd['timestamp']
        })
    return jsonify({'has_command': False})

@app.route('/api/devices/<device_id>/cmd/ack', methods=['POST'])
def ack_command(device_id):
    data = request.get_json()
    command = data.get('command')
    
    cmd = pending_commands.get(device_id)
    if cmd and cmd['command'] == command:
        cmd['executed'] = True
        print(f'[命令队列] {device_id}: {command} 已确认执行')
        return jsonify({'success': True})
    
    return jsonify({'success': False, 'error': '命令不存在或已过期'})

@app.route('/api/health')
def health_check():
    return jsonify({"status": "ok", "time": datetime.now().isoformat()})

if __name__ == '__main__':
    init_db()
    port = int(os.environ.get("PORT", 10000))
    app.run(host='0.0.0.0', port=port, debug=False)
