/**
 * API 请求封装
 * 
 * ⚠️ 重要提示：
 * 小程序强制要求 HTTPS，当前后端是 HTTP (139.196.153.51)
 * 
 * 开发测试方案：
 * 1. 微信开发者工具 → 详情 → 本地设置 → 勾选"不校验合法域名"
 * 2. 手机预览时：打开调试模式（右上角菜单 → 打开调试）
 * 
 * 正式部署方案（二选一）：
 * 1. 给服务器配置 SSL 证书（推荐）
 *    - 在服务器上安装 certbot，申请免费证书
 *    - Nginx 配置 443 端口 + SSL
 *    - 修改此处 BASE_URL 为 https://你的域名
 * 2. 使用 Render 等自带 HTTPS 的托管服务
 */

// ===== 修改这里 =====
// 开发测试用 HTTP（需开启不校验域名）
const BASE_URL = 'http://<YOUR_SERVER_IP>';

// 正式部署后改为 HTTPS（配置 SSL 证书后）
// const BASE_URL = 'https://你的域名';
// ===================

/**
 * 通用请求封装
 */
function request(options) {
  return new Promise((resolve, reject) => {
    const fullUrl = BASE_URL + options.url;
    console.log(`[Request] ${options.method || 'GET'} ${fullUrl}`);
    uni.request({
      url: fullUrl,
      method: options.method || 'GET',
      data: options.data || {},
      header: {
        'Content-Type': 'application/json',
        ...options.header
      },
      timeout: 30000,
      success: (res) => {
        console.log(`[Response] ${fullUrl} status=${res.statusCode}`);
        if (res.statusCode >= 200 && res.statusCode < 300) {
          resolve(res.data);
        } else {
          reject(new Error(`HTTP ${res.statusCode}`));
        }
      },
      fail: (err) => {
        console.error(`[Request Fail] ${fullUrl}:`, JSON.stringify(err));
        uni.showToast({
          title: err.errMsg || '网络请求失败',
          icon: 'none'
        });
        reject(err);
      }
    });
  });
}

// ===== 设备相关 API =====

/**
 * 获取所有设备列表
 */
export function getDevices() {
  return request({ url: '/api/devices' });
}

/**
 * 获取单个设备详情
 */
export function getDevice(deviceId) {
  return request({ url: `/api/devices/${deviceId}` });
}

/**
 * 获取设备历史数据
 * @param {string} deviceId - 设备ID
 * @param {number} hours - 查询小时数（默认24）
 */
export function getDeviceHistory(deviceId, hours = 24) {
  return request({
    url: `/api/devices/${deviceId}/history`,
    data: { hours }
  });
}

/**
 * 获取设备报警记录
 */
export function getDeviceAlarms(deviceId) {
  return request({ url: `/api/devices/${deviceId}/alarms` });
}

/**
 * 下发控制命令
 * @param {string} deviceId - 设备ID
 * @param {string} command - 命令，如 'buzzer_on', 'buzzer_off'
 */
export function sendCommand(deviceId, command) {
  return request({
    url: `/api/devices/${deviceId}/cmd`,
    method: 'POST',
    data: { command }
  });
}

/**
 * 健康检查
 */
export function healthCheck() {
  return request({ url: '/api/health' });
}

export { BASE_URL };
