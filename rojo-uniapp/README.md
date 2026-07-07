# 消防监测小程序（UniApp）

消防监测系统的小程序前端，基于 UniApp 开发，一套代码可同时编译为微信小程序、H5、App。

## 📁 项目结构

```
rojo-uniapp/
├── main.js                 # 应用入口
├── App.vue                 # 根组件
├── manifest.json           # 应用配置（小程序 AppID 等）
├── pages.json              # 页面路由和 TabBar 配置
├── utils/
│   └── request.js          # API 请求封装
├── pages/
│   ├── index/
│   │   └── index.vue       # 设备列表首页
│   ├── detail/
│   │   └── detail.vue      # 设备详情（实时数据+趋势图+控制）
│   └── alarms/
│       └── alarms.vue      # 报警记录
└── static/                 # 静态资源（图标等）
```

## 🚀 开发环境准备

### 1. 安装 HBuilderX（推荐）

1. 下载 [HBuilderX](https://www.dcloud.io/hbuilderx.html)
2. 安装并打开

### 2. 导入项目

1. HBuilderX → 文件 → 导入 → 从本地目录导入
2. 选择 `rojo-uniapp` 文件夹

### 3. 配置小程序 AppID

1. 打开 `manifest.json`
2. 找到 `"mp-weixin"` → `"appid"`
3. 填入你的微信小程序 AppID（去[微信公众平台](https://mp.weixin.qq.com)获取）

```json
"mp-weixin": {
  "appid": "wx1234567890abcdef",
  ...
}
```

### 4. 配置 API 地址

打开 `utils/request.js`，修改后端地址：

```javascript
// 开发测试（需开启不校验域名）
const BASE_URL = 'http://<YOUR_SERVER_IP>';

// 正式部署（配置 SSL 证书后）
// const BASE_URL = 'https://你的域名';
```

## 🧪 运行调试

### 微信小程序（推荐）

1. HBuilderX → 运行 → 运行到小程序模拟器 → 微信开发者工具
2. 首次运行会提示安装微信开发者工具，按指引安装
3. 微信开发者工具打开后：
   - **必须勾选**：详情 → 本地设置 → **「不校验合法域名、web-view...」**
   - 因为当前后端是 HTTP，小程序默认只允许 HTTPS

### H5 预览

HBuilderX → 运行 → 运行到浏览器 → Chrome

H5 没有 HTTPS 限制，可以直接访问 `http://<YOUR_SERVER_IP>`

## 📱 页面功能

| 页面 | 功能 |
|------|------|
| **首页** | 设备列表卡片、实时数据显示、5秒自动刷新、下拉刷新 |
| **详情页** | 设备实时数据、历史趋势图（Canvas）、控制命令下发 |
| **报警页** | 报警记录列表、筛选（全部/待处理/已处理）、标记处理 |

## 🔧 关键功能说明

### 趋势图

详情页使用 Canvas 自绘折线图，**无需引入 echarts/ucharts**：
- 红色线：温度变化
- 蓝色线：距离变化
- 支持 24小时 / 3天 / 7天 切换

### 控制命令

详情页可以下发控制指令到设备：
- 开启/关闭蜂鸣器
- 开启/关闭 LED

设备端需要订阅 `sensor/cmd` 主题并解析命令执行。

### 自动刷新

- 首页每 **5秒** 自动轮询设备列表
- 详情页每 **5秒** 自动刷新实时数据

## ⚠️ 上线前必做

### 1. 配置 HTTPS（必须）

小程序正式版**强制要求 HTTPS**，需要在服务器配置 SSL 证书：

```bash
# 在服务器上安装 certbot（免费证书）
sudo apt install certbot python3-certbot-nginx
sudo certbot --nginx -d 你的域名

# 或者在阿里云申请免费证书，手动配置 Nginx
```

配置完成后，修改 `utils/request.js`：
```javascript
const BASE_URL = 'https://你的域名';
```

### 2. 配置服务器域名白名单

1. 登录[微信公众平台](https://mp.weixin.qq.com)
2. 你的小程序 → 开发 → 开发管理 → 服务器域名
3. 添加：
   - `request合法域名`：`https://你的域名`
   - `uploadFile合法域名`：`https://你的域名`
   - `downloadFile合法域名`：`https://你的域名`

### 3. 关闭调试模式

微信开发者工具 → 取消勾选「不校验合法域名」

## 📡 API 接口对应

| 小程序调用 | 后端接口 |
|-----------|---------|
| `getDevices()` | `GET /api/devices` |
| `getDevice(id)` | `GET /api/devices/<id>` |
| `getDeviceHistory(id, hours)` | `GET /api/devices/<id>/history?hours=24` |
| `getDeviceAlarms(id)` | `GET /api/devices/<id>/alarms` |
| `sendCommand(id, cmd)` | `POST /api/devices/<id>/cmd` |

## 🎨 自定义样式

所有页面的样式都在 `<style scoped>` 中，可以直接修改：

- 主题色：搜索 `#ff6b6b` 替换为你想要的颜色
- 卡片圆角：搜索 `border-radius: 24rpx` 修改
- 字体大小：搜索 `font-size` 调整

## 🐛 常见问题

### 1. 请求失败 / 网络错误

**原因：** 小程序不允许 HTTP 请求

**解决：**
- 开发时：微信开发者工具 → 详情 → 勾选「不校验合法域名」
- 上线时：必须配置 HTTPS + 域名白名单

### 2. 真机预览看不到数据

**原因：** 真机没有开启调试模式

**解决：** 预览二维码 → 右上角菜单 → 打开调试

### 3. Canvas 趋势图不显示

**原因：** 历史数据不足（需要至少2个点才能画线）

**解决：** 等设备多上报几条数据，或检查 EMQX 规则引擎是否配置正确

## 📦 打包发布

### 微信小程序

1. HBuilderX → 发行 → 小程序-微信
2. 填写小程序 AppID
3. 点击发行，会生成 `unpackage/dist/build/mp-weixin`
4. 用微信开发者工具打开该目录
5. 微信开发者工具 → 上传 → 填写版本号 → 上传
6. 登录微信公众平台 → 版本管理 → 提交审核

### H5

1. HBuilderX → 发行 → 网站-H5
2. 生成的 `unpackage/dist/build/h5` 可以部署到任何静态服务器

## 📞 技术支持

如有问题，检查：
1. 后端是否正常运行：`curl http://<YOUR_SERVER_IP>/api/health`
2. EMQX 规则引擎是否配置正确
3. 小程序 AppID 是否正确配置
