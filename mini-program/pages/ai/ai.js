Page({
  data: {
    messages: [],
    inputValue: '',
    loading: false
  },
  onLoad() {
    this.setData({
      messages: [{
        role: 'ai',
        content: '你好，我是消防监测AI助手。你可以问我关于设备数据的问题，比如"当前温度是多少"、"设备状态如何"等。'
      }]
    });
  },
  onInput(e) {
    this.setData({ inputValue: e.detail.value });
  },
  sendMessage() {
    const query = this.data.inputValue.trim();
    if (!query || this.data.loading) return;
    const messages = [...this.data.messages, { role: 'user', content: query }];
    this.setData({ messages, inputValue: '', loading: true });
    const api = require('../../utils/api');
    api.askAI(query)
      .then(res => {
        if (res.success) {
          this.setData({
            messages: [...this.data.messages, { role: 'ai', content: res.reply }],
            loading: false
          });
        } else {
          this.setData({
            messages: [...this.data.messages, { role: 'ai', content: '抱歉，AI服务暂时不可用。' }],
            loading: false
          });
        }
      })
      .catch(err => {
        this.setData({
          messages: [...this.data.messages, { role: 'ai', content: '网络请求失败，请检查网络连接。' }],
          loading: false
        });
      });
  }
});