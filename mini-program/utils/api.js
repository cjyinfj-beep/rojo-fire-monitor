const { API_BASE } = require('./config');

const request = (options) => {
  return new Promise((resolve, reject) => {
    wx.request({
      url: API_BASE + options.url,
      method: options.method || 'GET',
      data: options.data || {},
      header: {
        'Content-Type': 'application/json',
        ...options.header
      },
      timeout: 10000,
      success: (res) => {
        if (res.statusCode >= 200 && res.statusCode < 300) {
          resolve(res.data);
        } else {
          reject(new Error(`HTTP ${res.statusCode}`));
        }
      },
      fail: (err) => {
        reject(err);
      }
    });
  });
};

module.exports = {
  getDevices: () => request({ url: '/api/devices' }),
  getDevice: (id) => request({ url: '/api/device/' + id }),
  askAI: (query, sensorData) => request({
    url: '/api/ai',
    method: 'POST',
    data: { query, sensor_data: sensorData }
  })
};