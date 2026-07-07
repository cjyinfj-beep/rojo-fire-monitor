Page({
  data: {
    devices: [],
    loading: true,
    error: ''
  },
  onLoad() {
    this.loadDevices();
  },
  onShow() {
    if (!this.data.loading) {
      this.loadDevices();
    }
  },
  onPullDownRefresh() {
    this.loadDevices(() => {
      wx.stopPullDownRefresh();
    });
  },
  loadDevices(callback) {
    const api = require('../../utils/api');
    this.setData({ loading: true, error: '' });
    api.getDevices()
      .then(data => {
        const devices = data.map(d => {
          const reason = (d.alarm_reason || '').toUpperCase();
          const colorStatus = (d.color_status || '').toUpperCase();
          return {
            ...d,
            // 温度：报警且原因含 TEMP/LOW_TEMP/HIGH_TEMP → 红色，否则绿色
            _tempClass: d.alarm && (reason.includes('TEMP') || reason.includes('LOW_TEMP') || reason.includes('HIGH_TEMP'))
              ? 'status-danger' : 'status-normal',
            // 距离：报警且原因含 DIST/LOW_DIST/HIGH_DIST → 红色，否则默认白色
            _distClass: d.alarm && (reason.includes('DIST') || reason.includes('LOW_DIST') || reason.includes('HIGH_DIST'))
              ? 'status-danger' : '',
            // 颜色：根据颜色值本身显示对应颜色
            _colorClass: colorStatus === 'YELLOW' || colorStatus === 'YELLOW_COLOR'
              ? 'status-yellow'
              : (colorStatus === 'RED' || colorStatus === 'RED_COLOR'
                ? 'status-danger'
                : (colorStatus === 'GREEN' || colorStatus === 'GREEN_COLOR'
                  ? 'status-normal'
                  : '')),
            // 状态：报警红色，正常绿色
            _statusClass: d.alarm ? 'status-danger' : 'status-normal'
          };
        });
        this.setData({ devices, loading: false });
        getApp().globalData.deviceData = devices;
        if (callback) callback();
      })
      .catch(err => {
        this.setData({ error: '获取数据失败: ' + err.message, loading: false });
        if (callback) callback();
      });
  },
  refresh() {
    this.loadDevices();
  }
});
