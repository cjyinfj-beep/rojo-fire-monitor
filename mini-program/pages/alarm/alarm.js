Page({
  data: {
    alarms: [],
    loading: true,
    error: ''
  },
  onLoad() {
    this.loadAlarms();
  },
  onShow() {
    if (!this.data.loading) {
      this.loadAlarms();
    }
  },
  onPullDownRefresh() {
    this.loadAlarms(() => {
      wx.stopPullDownRefresh();
    });
  },
  loadAlarms(callback) {
    const api = require('../../utils/api');
    this.setData({ loading: true, error: '' });
    api.getDevices()
      .then(data => {
        const alarms = data.filter(d => d.alarm === true).map(d => {
          const reason = (d.alarm_reason || '').toUpperCase();
          const colorStatus = (d.color_status || '').toUpperCase();
          return {
            ...d,
            _tempClass: reason.includes('TEMP') || reason.includes('LOW_TEMP') || reason.includes('HIGH_TEMP')
              ? 'status-danger' : 'status-normal',
            _distClass: reason.includes('DIST') || reason.includes('LOW_DIST') || reason.includes('HIGH_DIST')
              ? 'status-danger' : '',
            _colorClass: colorStatus === 'YELLOW' || colorStatus === 'YELLOW_COLOR'
              ? 'status-yellow'
              : (colorStatus === 'RED' || colorStatus === 'RED_COLOR'
                ? 'status-danger'
                : (colorStatus === 'GREEN' || colorStatus === 'GREEN_COLOR'
                  ? 'status-normal'
                  : '')),
            _statusClass: 'status-danger'
          };
        });
        this.setData({ alarms: alarms, loading: false });
        if (callback) callback();
      })
      .catch(err => {
        this.setData({ error: '获取数据失败: ' + err.message, loading: false });
        if (callback) callback();
      });
  },
  refresh() {
    this.loadAlarms();
  }
});
