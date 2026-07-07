<template>
  <view class="container">
    <!-- 设备基本信息 -->
    <view class="header-card">
      <view class="header-top">
        <view>
          <text class="device-title">{{ device.name || device.id }}</text>
          <text class="device-loc">📍 {{ device.location || '未设置位置' }}</text>
        </view>
        <view class="status-tag" :class="isAlarm ? 'tag-alarm' : 'tag-normal'">
          <text class="tag-icon">{{ isAlarm ? '🔥' : '✓' }}</text>
          <text>{{ isAlarm ? '报警中' : '正常' }}</text>
        </view>
      </view>

      <!-- 实时数据大卡片 -->
      <view class="big-stats">
        <view class="big-stat">
          <text class="big-value" :style="{color: valueColor}">{{ device.temperature || 0 }}</text>
          <text class="big-unit">°C</text>
          <text class="big-label">温度</text>
        </view>
        <view class="big-divider"></view>
        <view class="big-stat">
          <text class="big-value" :style="{color: valueColor}">{{ device.distance || 0 }}</text>
          <text class="big-unit">cm</text>
          <text class="big-label">距离</text>
        </view>
      </view>

      <!-- 颜色传感器 -->
      <view class="color-section">
        <text class="color-label">颜色传感器</text>
        <view class="color-preview" :style="{backgroundColor: colorPreview}"></view>
        <text class="color-text" :style="{color: colorTextColor}">{{ colorText }}</text>
        <text class="color-rgb">R:{{ device.color_r || 0 }} G:{{ device.color_g || 0 }} B:{{ device.color_b || 0 }}</text>
      </view>
    </view>

    <!-- 历史趋势图 -->
    <view class="chart-card">
      <view class="chart-header">
        <text class="chart-title">📈 历史趋势</text>
        <view class="time-tabs">
          <text
            v-for="t in timeRanges"
            :key="t.value"
            class="time-tab"
            :class="{ active: selectedHours === t.value }"
            @click="changeTimeRange(t.value)"
          >{{ t.label }}</text>
        </view>
      </view>
      
      <canvas
        v-if="historyData.length > 1"
        canvas-id="trendChart"
        id="trendChart"
        class="chart-canvas"
        :style="{width: canvasWidth + 'px', height: canvasHeight + 'px'}"
      ></canvas>
      <view v-else class="chart-empty">
        <text>暂无足够数据绘制趋势图</text>
      </view>

      <view class="chart-legend">
        <view class="legend-item">
          <view class="legend-dot" style="background:#ff3333"></view>
          <text>温度 (°C)</text>
        </view>
        <view class="legend-item">
          <view class="legend-dot" style="background:#448aff"></view>
          <text>距离 (cm)</text>
        </view>
      </view>
    </view>

    <!-- 控制面板 -->
    <view class="control-card">
      <text class="card-title">🎮 远程控制</text>
      <view class="control-btns">
        <view class="ctrl-btn primary" @click="sendCmd('buzzer_on')">
          <text class="btn-icon">🔔</text>
          <text>蜂鸣器开</text>
        </view>
        <view class="ctrl-btn" @click="sendCmd('buzzer_off')">
          <text class="btn-icon">🔕</text>
          <text>蜂鸣器关</text>
        </view>
        <view class="ctrl-btn" @click="sendCmd('led_on')">
          <text class="btn-icon">💡</text>
          <text>LED 开</text>
        </view>
        <view class="ctrl-btn" @click="sendCmd('led_off')">
          <text class="btn-icon">🌑</text>
          <text>LED 关</text>
        </view>
      </view>
    </view>

    <!-- 报警记录入口 -->
    <view class="link-card" @click="goAlarms">
      <view class="link-left">
        <text class="link-icon">🚨</text>
        <text>查看报警记录</text>
      </view>
      <text class="link-arrow">›</text>
    </view>

    <!-- 设备信息 -->
    <view class="footer-info">
      <text>🆔 设备ID: {{ device.id }}</text>
      <text>🔌 启动次数: {{ device.boot_count || 0 }}</text>
      <text>🕐 更新于 {{ formatTime(device.lastSeen) }}</text>
    </view>
  </view>
</template>

<script>
import { getDevice, getDeviceHistory, sendCommand } from '@/utils/request.js';

export default {
  data() {
    return {
      deviceId: '',
      device: {},
      historyData: [],
      selectedHours: 24,
      timeRanges: [
        { label: '24小时', value: 24 },
        { label: '3天', value: 72 },
        { label: '7天', value: 168 }
      ],
      canvasWidth: 350,
      canvasHeight: 200,
      timer: null,
      isLoading: false
    };
  },

  computed: {
    isAlarm() {
      const alarm = this.device.alarm;
      return alarm === true || alarm === 'true' || alarm === 1 || alarm === '1';
    },
    colorText() {
      const status = (this.device.color_status || '').toLowerCase();
      if (status === 'red') return '红色';
      if (status === 'yellow') return '黄色';
      if (status === 'green') return '绿色';
      return '未知';
    },
    colorPreview() {
      const r = Math.min(255, Math.round((this.device.color_r || 0) / 65535 * 255));
      const g = Math.min(255, Math.round((this.device.color_g || 0) / 65535 * 255));
      const b = Math.min(255, Math.round((this.device.color_b || 0) / 65535 * 255));
      return `rgb(${r},${g},${b})`;
    },
    valueColor() {
      return this.isAlarm ? '#ff1744' : '#e0e0e0';
    },
    colorTextColor() {
      return this.isAlarm ? '#ff1744' : '#e0e0e0';
    }
  },

  onLoad(options) {
    this.deviceId = options.id;
    this.startPolling();
  },

  onUnload() {
    this.stopPolling();
  },

  onHide() {
    this.stopPolling();
  },

  onShow() {
    this.startPolling();
  },

  methods: {
    startPolling() {
      if (this.timer) return;
      this.loadData();
      this.timer = setInterval(() => {
        this.loadDevice();
      }, 5000);
    },

    stopPolling() {
      if (this.timer) {
        clearInterval(this.timer);
        this.timer = null;
      }
    },

    async loadData() {
      await this.loadDevice();
      await this.loadHistory();
    },

    async loadDevice() {
      if (this.isLoading) return;
      this.isLoading = true;
      try {
        const data = await getDevice(this.deviceId);
        this.device = data || {};
      } catch (e) {
        console.error('获取设备详情失败:', e);
      } finally {
        this.isLoading = false;
      }
    },

    async loadHistory() {
      try {
        const data = await getDeviceHistory(this.deviceId, this.selectedHours);
        this.historyData = Array.isArray(data) ? data.reverse() : [];
        this.$nextTick(() => {
          this.drawChart();
        });
      } catch (e) {
        console.error('获取历史数据失败:', e);
      }
    },

    changeTimeRange(hours) {
      this.selectedHours = hours;
      this.loadHistory();
    },

    drawChart() {
      const ctx = uni.createCanvasContext('trendChart', this);
      const w = this.canvasWidth;
      const h = this.canvasHeight;
      const pad = 30;
      const chartW = w - pad * 2;
      const chartH = h - pad * 2;

      const data = this.historyData;
      if (data.length < 2) return;

      const temps = data.map(d => d.temperature || 0);
      const dists = data.map(d => d.distance || 0);
      const minT = Math.min(...temps) * 0.9;
      const maxT = Math.max(...temps) * 1.1 || 1;
      const minD = Math.min(...dists) * 0.9;
      const maxD = Math.max(...dists) * 1.1 || 1;

      ctx.clearRect(0, 0, w, h);

      ctx.setStrokeStyle('#2a3040');
      ctx.setLineWidth(1);
      for (let i = 0; i <= 4; i++) {
        const y = pad + (chartH / 4) * i;
        ctx.beginPath();
        ctx.moveTo(pad, y);
        ctx.lineTo(w - pad, y);
        ctx.stroke();
      }

      this.drawLine(ctx, data, 'temperature', minT, maxT, pad, chartW, chartH, '#ff3333');
      this.drawLine(ctx, data, 'distance', minD, maxD, pad, chartW, chartH, '#448aff');

      ctx.draw();
    },

    drawLine(ctx, data, key, min, max, pad, chartW, chartH, color) {
      ctx.setStrokeStyle(color);
      ctx.setLineWidth(2);
      ctx.beginPath();

      data.forEach((item, i) => {
        const x = pad + (chartW / (data.length - 1)) * i;
        const val = item[key] || 0;
        const y = pad + chartH - ((val - min) / (max - min)) * chartH;
        if (i === 0) {
          ctx.moveTo(x, y);
        } else {
          ctx.lineTo(x, y);
        }
      });

      ctx.stroke();

      ctx.setFillStyle(color);
      data.forEach((item, i) => {
        const x = pad + (chartW / (data.length - 1)) * i;
        const val = item[key] || 0;
        const y = pad + chartH - ((val - min) / (max - min)) * chartH;
        ctx.beginPath();
        ctx.arc(x, y, 3, 0, Math.PI * 2);
        ctx.fill();
      });
    },

    async sendCmd(command) {
      uni.showLoading({ title: '发送中...' });
      try {
        const res = await sendCommand(this.deviceId, command);
        uni.hideLoading();
        if (res.success) {
          uni.showToast({ title: '命令已发送', icon: 'success' });
        } else {
          uni.showToast({ title: '发送失败', icon: 'none' });
        }
      } catch (e) {
        uni.hideLoading();
        uni.showToast({ title: '网络错误', icon: 'none' });
      }
    },

    goAlarms() {
      uni.setStorageSync('alarmDeviceId', this.deviceId);
      uni.switchTab({
        url: '/pages/alarms/alarms'
      });
    },

    formatTime(ts) {
      if (!ts) return '未知';
      return new Date(ts).toLocaleString('zh-CN');
    }
  }
};
</script>

<style scoped>
.container {
  min-height: 100vh;
  background: #0a0e1a;
  padding: 20rpx;
  padding-bottom: 60rpx;
}

.header-card {
  background: linear-gradient(135deg, #1a1f2e 0%, #151921 100%);
  border-radius: 24rpx;
  padding: 40rpx;
  margin-bottom: 20rpx;
  border: 1rpx solid #2a3040;
  box-shadow: 0 4rpx 20rpx rgba(0,0,0,0.3);
}

.header-top {
  display: flex;
  justify-content: space-between;
  align-items: flex-start;
  margin-bottom: 30rpx;
}

.device-title {
  font-size: 40rpx;
  font-weight: bold;
  color: #e0e0e0;
  display: block;
}

.device-loc {
  font-size: 24rpx;
  color: #8899aa;
  margin-top: 8rpx;
  display: block;
}

.status-tag {
  padding: 10rpx 24rpx;
  border-radius: 30rpx;
  font-size: 24rpx;
  font-weight: 500;
  display: flex;
  align-items: center;
}

.tag-normal {
  background: rgba(0,200,83,0.15);
  color: #00c853;
  border: 1rpx solid rgba(0,200,83,0.3);
}

.tag-alarm {
  background: rgba(255,51,51,0.15);
  color: #ff3333;
  border: 1rpx solid rgba(255,51,51,0.3);
  animation: alarmPulse 1.5s ease-in-out infinite;
}

.tag-icon {
  margin-right: 6rpx;
}

.big-stats {
  display: flex;
  justify-content: space-around;
  padding: 30rpx 0;
  border-top: 1rpx solid #2a3040;
}

.big-stat {
  display: flex;
  flex-direction: column;
  align-items: center;
}

.big-value {
  font-size: 72rpx;
  font-weight: bold;
  line-height: 1;
  transition: color 0.3s ease;
}

.big-unit {
  font-size: 28rpx;
  color: #8899aa;
  margin-top: 8rpx;
}

.big-label {
  font-size: 26rpx;
  color: #8899aa;
  margin-top: 12rpx;
}

.big-divider {
  width: 1rpx;
  background: #2a3040;
}

.color-section {
  display: flex;
  align-items: center;
  margin-top: 24rpx;
  padding-top: 24rpx;
  border-top: 1rpx solid #2a3040;
}

.color-label {
  font-size: 26rpx;
  color: #8899aa;
  margin-right: 20rpx;
}

.color-preview {
  width: 48rpx;
  height: 48rpx;
  border-radius: 12rpx;
  margin-right: 16rpx;
  border: 2rpx solid #2a3040;
}

.color-text {
  font-size: 28rpx;
  font-weight: 500;
  margin-right: 16rpx;
}

.color-rgb {
  font-size: 22rpx;
  color: #667788;
}

.chart-card {
  background: linear-gradient(135deg, #1a1f2e 0%, #151921 100%);
  border-radius: 24rpx;
  padding: 30rpx;
  margin-bottom: 20rpx;
  border: 1rpx solid #2a3040;
  box-shadow: 0 4rpx 20rpx rgba(0,0,0,0.3);
}

.chart-header {
  display: flex;
  justify-content: space-between;
  align-items: center;
  margin-bottom: 20rpx;
}

.chart-title {
  font-size: 30rpx;
  font-weight: bold;
  color: #e0e0e0;
}

.time-tabs {
  display: flex;
  background: #0d1117;
  border-radius: 12rpx;
  padding: 4rpx;
  border: 1rpx solid #2a3040;
}

.time-tab {
  padding: 8rpx 20rpx;
  font-size: 22rpx;
  color: #8899aa;
  border-radius: 8rpx;
}

.time-tab.active {
  background: linear-gradient(135deg, #ff3333 0%, #ff6600 100%);
  color: #fff;
  font-weight: 500;
}

.chart-canvas {
  width: 100%;
  height: 400rpx;
}

.chart-empty {
  height: 400rpx;
  display: flex;
  align-items: center;
  justify-content: center;
  color: #667788;
  font-size: 28rpx;
}

.chart-legend {
  display: flex;
  justify-content: center;
  margin-top: 16rpx;
}

.legend-item {
  display: flex;
  align-items: center;
  margin: 0 30rpx;
  font-size: 24rpx;
  color: #8899aa;
}

.legend-dot {
  width: 20rpx;
  height: 20rpx;
  border-radius: 10rpx;
  margin-right: 10rpx;
}

.control-card {
  background: linear-gradient(135deg, #1a1f2e 0%, #151921 100%);
  border-radius: 24rpx;
  padding: 30rpx;
  margin-bottom: 20rpx;
  border: 1rpx solid #2a3040;
  box-shadow: 0 4rpx 20rpx rgba(0,0,0,0.3);
}

.card-title {
  font-size: 30rpx;
  font-weight: bold;
  color: #e0e0e0;
  margin-bottom: 24rpx;
  display: block;
}

.control-btns {
  display: flex;
  flex-wrap: wrap;
  gap: 20rpx;
}

.ctrl-btn {
  flex: 1;
  min-width: 140rpx;
  display: flex;
  flex-direction: column;
  align-items: center;
  padding: 30rpx 20rpx;
  background: #0d1117;
  border-radius: 16rpx;
  font-size: 24rpx;
  color: #8899aa;
  border: 1rpx solid #2a3040;
}

.ctrl-btn.primary {
  background: linear-gradient(135deg, #ff3333 0%, #ff6600 100%);
  color: #fff;
  border: none;
}

.ctrl-btn:active {
  opacity: 0.8;
}

.btn-icon {
  font-size: 48rpx;
  margin-bottom: 12rpx;
}

.link-card {
  background: linear-gradient(135deg, #1a1f2e 0%, #151921 100%);
  border-radius: 24rpx;
  padding: 30rpx;
  margin-bottom: 20rpx;
  display: flex;
  justify-content: space-between;
  align-items: center;
  font-size: 30rpx;
  color: #e0e0e0;
  border: 1rpx solid #2a3040;
  box-shadow: 0 4rpx 20rpx rgba(0,0,0,0.3);
}

.link-left {
  display: flex;
  align-items: center;
}

.link-icon {
  margin-right: 12rpx;
}

.link-arrow {
  font-size: 40rpx;
  color: #ff3333;
}

.footer-info {
  text-align: center;
  padding: 30rpx;
}

.footer-info text {
  display: block;
  font-size: 22rpx;
  color: #667788;
  margin: 8rpx 0;
}

@keyframes alarmPulse {
  0%, 100% { opacity: 1; }
  50% { opacity: 0.7; }
}
</style>
