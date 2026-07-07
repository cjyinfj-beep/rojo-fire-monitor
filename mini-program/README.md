# 🔥 消防监测系统 小程序

微信小程序前端项目，配合 Flask 后端使用。

## 📁 目录结构

```
mini-program/
├── app.js                  # 小程序入口
├── app.json                # 全局配置（页面路由、tabBar）
├── app.wxss                # 全局样式（暗黑主题）
├── sitemap.json            # 微信搜索索引
├── project.config.json     # 开发者工具项目配置
├── utils/
│   ├── config.js           # 环境配置（修改API地址）
│   └── api.js              # API 请求封装
├── images/
│   └── tabbar/             # 6个tabBar图标（81×81）
└── pages/
    ├── index/              # 首页 - 设备监控
    ├── alarm/              # 告警中心
    ├── ai/                 # AI助手对话
    └── privacy/            # 隐私政策（审批必需）
```

## 🚀 快速开始

### 1. 配置后端地址

编辑 `utils/config.js`：

```javascript
const ENV = 'dev';        // 开发环境
// const ENV = 'prod';    // 提交审核前改为 prod
```

生产环境必须配置 HTTPS 备案域名：
```javascript
prod: {
  API_BASE: 'https://你的域名.com'
}
```

### 2. 导入微信开发者工具

1. 打开**微信开发者工具**
2. 项目 → 导入项目 → 选择 `mini-program` 文件夹
3. 设置 → 勾选 **"不校验合法域名"**（开发环境）
4. 点击编译

### 3. 提交审核注意事项

| 项目 | 要求 | 状态 |
|------|------|------|
| 域名 | HTTPS + 备案 | 需自行配置 |
| appid | 真实小程序 appid | 需替换 `project.config.json` 中的 `touristappid` |
| 隐私政策 | 必需页面 | ✅ 已包含 |
| 类目 | 工具-设备管理 或 IT科技-物联网 | 自行选择 |

## 🛠 开发环境

- 小程序基础库：3.3.0+
- 后端服务：`999_fixed.py`（Flask + SocketIO + MQTT）
- 网络：局域网 `http://172.20.10.3:5000`（开发时）

## 📱 页面功能

| 页面 | 功能 | 路径 |
|------|------|------|
| 首页 | 展示所有设备实时数据 | `pages/index/index` |
| 告警 | 筛选显示异常设备 | `pages/alarm/alarm` |
| AI | 与豆包大模型对话，分析设备数据 | `pages/ai/ai` |
| 隐私政策 | 审批必需 | `pages/privacy/privacy` |

## 📝 后端 API

| 接口 | 方法 | 说明 |
|------|------|------|
| `/api/devices` | GET | 获取所有设备 |
| `/api/device/<id>` | GET | 获取单个设备 |
| `/api/ai` | POST | AI 对话（body: `{query, sensor_data}`）|

## ⚠️ 常见问题

**Q: 提示"无法访问"？**
A: 检查：1. Flask 服务是否启动 2. IP 地址是否变化 3. 防火墙是否放行 5000 端口

**Q: 提示"不允许使用 GET 方法"？**
A: `/api/ai` 只支持 POST，请检查小程序 `api.js` 中的请求方法。

**Q: 审批被拒？**
A: 确保：1. 使用 HTTPS 域名 2. 隐私政策页面完整 3. 小程序类目正确

## 📄 文件清单

共 **24 个文件**：
- 核心配置：5 个（app.js, app.json, app.wxss, sitemap.json, project.config.json）
- 工具：2 个（utils/config.js, utils/api.js）
- 图标：6 个（PNG，81×81 像素）
- 页面：16 个（4 页面 × 4 文件）

---

**Author**: 消防监测系统开发团队
