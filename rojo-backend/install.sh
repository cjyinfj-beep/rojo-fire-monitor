#!/bin/bash
# ============================================================
# 消防监测系统 - 一键部署脚本
# 适用于: Ubuntu 22.04/24.04 (阿里云轻量应用服务器)
# 运行方式: chmod +x install.sh && sudo ./install.sh
# ============================================================

set -e  # 遇到错误立即退出

echo "=========================================="
echo "🔥 消防监测系统 - 一键部署"
echo "=========================================="

# ---------- 配置 ----------
DB_USER="rojo"
DB_PASS="rojo123"
DB_NAME="rojo_fire"
APP_DIR="/root/rojo-backend"
APP_PORT=10000
EMQX_HOST="139.196.153.51"

# ---------- 0. 检查是否 root ----------
if [ "$EUID" -ne 0 ]; then
    echo "❌ 请用 root 用户运行: sudo ./install.sh"
    exit 1
fi

# ---------- 1. 更新系统 ----------
echo ""
echo "[1/8] 更新系统..."
apt update -y
apt upgrade -y

# ---------- 2. 安装依赖 ----------
echo ""
echo "[2/8] 安装基础软件 (Python3, PostgreSQL, Nginx, Supervisor)..."
apt install -y \
    python3 python3-pip python3-venv \
    postgresql postgresql-contrib \
    nginx supervisor \
    curl wget git

# ---------- 3. 配置 PostgreSQL ----------
echo ""
echo "[3/8] 配置 PostgreSQL 数据库..."

# 启动 PostgreSQL
systemctl enable postgresql
systemctl start postgresql

# 等待 PostgreSQL 启动
sleep 2

# 创建数据库和用户（如果不存在）
sudo -u postgres psql <<EOF
DO \$\$
BEGIN
    IF NOT EXISTS (SELECT FROM pg_roles WHERE rolname = '${DB_USER}') THEN
        CREATE USER ${DB_USER} WITH PASSWORD '${DB_PASS}';
    END IF;
END
\$\$;

SELECT 'CREATE DATABASE ${DB_NAME} OWNER ${DB_USER}' 
WHERE NOT EXISTS (SELECT FROM pg_database WHERE datname = '${DB_NAME}')\gexec

GRANT ALL PRIVILEGES ON DATABASE ${DB_NAME} TO ${DB_USER};
EOF

echo "✅ 数据库配置完成: ${DB_NAME}"

# ---------- 4. 配置 Python 环境 ----------
echo ""
echo "[4/8] 配置 Python 虚拟环境..."

cd /root

# 检查代码目录是否存在
if [ ! -d "$APP_DIR" ]; then
    echo "❌ 未找到代码目录 $APP_DIR"
    echo "   请先把 rojo-backend 文件夹上传到 /root/ 目录"
    echo "   例如: scp -r rojo-backend root@139.196.153.51:/root/"
    exit 1
fi

cd $APP_DIR

# 创建虚拟环境
python3 -m venv venv
source venv/bin/activate

# 安装依赖
pip install --upgrade pip
pip install -r requirements.txt

echo "✅ Python 依赖安装完成"

# ---------- 5. 初始化数据库表 ----------
echo ""
echo "[5/8] 初始化数据库表..."

# 运行一次 app.py 来初始化表（会调用 init_db()）
export DATABASE_URL="postgresql://${DB_USER}:${DB_PASS}@localhost:5432/${DB_NAME}"
python3 -c "
import sys
sys.path.insert(0, '$APP_DIR')
from app import init_db
try:
    init_db()
    print('✅ 数据库表初始化成功')
except Exception as e:
    print(f'⚠️ 数据库初始化警告: {e}')
    print('   如果表已存在则无需担心')
"

# ---------- 6. 配置 Nginx ----------
echo ""
echo "[6/8] 配置 Nginx 反向代理..."

cat > /etc/nginx/sites-available/rojo <<'NGINX_EOF'
server {
    listen 80;
    server_name _;

    client_max_body_size 20M;

    location / {
        proxy_pass http://127.0.0.1:10000;
        proxy_http_version 1.1;
        proxy_set_header Host $host;
        proxy_set_header X-Real-IP $remote_addr;
        proxy_set_header X-Forwarded-For $proxy_add_x_forwarded_for;
        proxy_set_header X-Forwarded-Proto $scheme;
    }

    location /api/health {
        access_log off;
    }
}
NGINX_EOF

# 启用配置
rm -f /etc/nginx/sites-enabled/default
ln -sf /etc/nginx/sites-available/rojo /etc/nginx/sites-enabled/rojo

# 测试并重载
nginx -t
systemctl reload nginx
systemctl enable nginx

echo "✅ Nginx 配置完成"

# ---------- 7. 配置 Supervisor ----------
echo ""
echo "[7/8] 配置 Supervisor 后台运行..."

cat > /etc/supervisor/conf.d/rojo.conf <<SUP_EOF
[program:rojo-backend]
directory=${APP_DIR}
command=${APP_DIR}/venv/bin/python ${APP_DIR}/app.py
autostart=true
autorestart=true
user=root
stderr_logfile=/var/log/rojo.err.log
stdout_logfile=/var/log/rojo.out.log
environment=DATABASE_URL="postgresql://${DB_USER}:${DB_PASS}@localhost:5432/${DB_NAME}",EMQX_HOST="${EMQX_HOST}"
SUP_EOF

supervisorctl reread
supervisorctl update
supervisorctl start rojo-backend || supervisorctl restart rojo-backend

echo "✅ Supervisor 配置完成"

# ---------- 8. 防火墙放行 ----------
echo ""
echo "[8/8] 配置防火墙..."

# UFW 放行
ufw allow OpenSSH
ufw allow 80/tcp
ufw allow 5000/tcp
ufw --force enable

echo "✅ 防火墙配置完成"

# ---------- 完成 ----------
echo ""
echo "=========================================="
echo "🎉 部署完成！"
echo "=========================================="
echo ""
echo "📡 API 地址:"
echo "   本机: http://127.0.0.1:${APP_PORT}"
echo "   公网: http://${EMQX_HOST}"
echo ""
echo "🔍 验证命令:"
echo "   curl http://127.0.0.1:${APP_PORT}/api/health"
echo ""
echo "📋 常用管理命令:"
echo "   查看日志:   tail -f /var/log/rojo.out.log"
echo "   查看错误:   tail -f /var/log/rojo.err.log"
echo "   重启服务:   supervisorctl restart rojo-backend"
echo "   查看状态:   supervisorctl status rojo-backend"
echo ""
echo "⚠️  下一步：去阿里云控制台放行 80 端口"
echo "   控制台 → 安全组/防火墙 → 添加规则:"
echo "   - 协议: TCP"
echo "   - 端口: 80"
echo "   - 授权对象: 0.0.0.0/0"
echo ""
echo "⚠️  配置 EMQX 规则引擎:"
echo "   1. 打开 http://${EMQX_HOST}:18083"
echo "   2. 数据集成 → 规则 → 创建"
echo "   3. SQL: SELECT payload FROM \"sensor/data\" WHERE payload.id IS NOT NULL"
echo "   4. 动作 → HTTP 服务器"
echo "   5. URL: http://127.0.0.1:${APP_PORT}/api/mqtt/webhook"
echo "   6. 方法: POST, Body: \${payload}"
echo "=========================================="
