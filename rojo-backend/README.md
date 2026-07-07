# 消防监测系统 - 服务器部署指南

## 📦 文件清单

上传前，请确认 `rojo-backend` 文件夹包含以下文件：

```
rojo-backend/
├── app.py              # Flask 后端主程序
├── requirements.txt    # Python 依赖
├── install.sh          # 一键安装脚本
└── README.md           # 本文件
```

## 🚀 部署步骤（总共约 10 分钟）

### 第一步：重置服务器密码（2分钟）

1. 阿里云控制台 → 你的实例 `<YOUR_SERVER_NAME>`
2. 点击 **"重置密码"**
3. 设置一个密码（建议用你能记住的，比如 `Rojo1234!`）
4. 确认后等待实例重启（状态变回"运行中"）

### 第二步：连接服务器（1分钟）

在你的电脑上打开 **Git Bash**（右键桌面 → Git Bash Here），执行：

```bash
ssh root@<YOUR_SERVER_IP>
# 输入你刚才设置的密码
```

成功连接后，你会看到类似：
```
root@<YOUR_SERVER_NAME>:~#
```

### 第三步：上传代码到服务器（1分钟）

**不要关闭刚才的 SSH 窗口**，再开一个 Git Bash：

```bash
# 进入代码目录
cd /path/to/rojo-backend

# 用 SCP 上传到服务器
scp -r . root@<YOUR_SERVER_IP>:/root/rojo-backend
```

输入密码，等待上传完成。

> 💡 如果没有 Git Bash，可以用任何支持 SSH 的终端，如 PowerShell、VSCode 终端等。

### 第四步：运行一键安装脚本（5分钟）

**切回 SSH 窗口**（已经连上服务器的那个），执行：

```bash
cd /root/rojo-backend
chmod +x install.sh
sudo ./install.sh
```

然后等待脚本自动完成所有安装和配置。

### 第五步：阿里云放行 80 端口（1分钟）

1. 阿里云控制台 → 你的实例 `<YOUR_SERVER_NAME>`
2. 左侧菜单 → **"安全组"** 或 **"防火墙"**
3. 点击 **"添加规则"**
4. 配置：
   - **协议类型**：TCP
   - **端口范围**：`80`
   - **授权对象**：`0.0.0.0/0`
   - **备注**：HTTP
5. 保存

### 第六步：验证部署

在浏览器访问：
```
http://<YOUR_SERVER_IP>/api/health
```

如果返回 `{"status": "ok"}`，说明部署成功！🎉

---

## 🔧 配置 EMQX 规则引擎

部署成功后，还需要让 EMQX 把设备数据推送到后端：

1. 打开 EMQX Dashboard：`http://<YOUR_SERVER_IP>:18083`
2. 左侧菜单 → **数据集成** → **规则**
3. 点击 **创建**
4. **SQL** 填入：
   ```sql
   SELECT payload FROM "sensor/data" WHERE payload.id IS NOT NULL
   ```
5. 点击 **添加动作** → 选择 **HTTP 服务器**
   - **URL**：`http://127.0.0.1:10000/api/mqtt/webhook`
   - **方法**：`POST`
   - **Headers**：`Content-Type: application/json`
   - **Body**：`${payload}`
6. 保存规则

设备发数据后，访问 `http://<YOUR_SERVER_IP>/api/devices` 就能看到数据了。

---

## 📡 API 接口（小程序可用）

| 接口 | 方法 | 说明 |
|------|------|------|
| `/api/health` | GET | 健康检查 |
| `/api/devices` | GET | 获取所有设备最新数据 |
| `/api/devices/<id>` | GET | 获取单个设备详情 |
| `/api/devices/<id>/history?hours=24` | GET | 获取历史数据 |
| `/api/devices/<id>/alarms` | GET | 获取报警记录 |
| `/api/devices/<id>/cmd` | POST | 下发控制命令 |

---

## 📋 常用管理命令

```bash
# 查看服务运行状态
supervisorctl status rojo-backend

# 重启后端服务
supervisorctl restart rojo-backend

# 查看运行日志
tail -f /var/log/rojo.out.log

# 查看错误日志
tail -f /var/log/rojo.err.log

# 查看 Nginx 状态
systemctl status nginx

# 重启 Nginx
systemctl restart nginx
```

---

## ⚠️ 注意事项

1. **不要删除 `/root/rojo-backend` 文件夹**，否则服务会停止
2. **修改代码后需要重启服务**：`supervisorctl restart rojo-backend`
3. **数据库密码**默认是 `<YOUR_PASSWORD>`，生产环境建议修改
4. **小程序访问**需要在微信公众平台把 `http://<YOUR_SERVER_IP>` 加入白名单
