App({
  onLaunch() {
    console.log('消防监测系统小程序启动');
    // 检查网络状态
    wx.getNetworkType({
      success: (res) => {
        console.log('网络类型:', res.networkType);
        if (res.networkType === 'none') {
          wx.showToast({ title: '请检查网络连接', icon: 'none' });
        }
      }
    });
  },
  globalData: {
    apiBase: 'http://172.20.10.3:5000',
    userInfo: null,
    deviceData: []
  }
});