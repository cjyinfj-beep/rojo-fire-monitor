<template>
  <view class="container">
    <!-- 消防主题顶部区域 -->
    <view class="fire-header">
      <view class="fire-badge">
        <text class="fire-icon">🧯</text>
        <text class="fire-title">消防监测中心</text>
      </view>
      <text class="fire-subtitle">实时监控 · 智能预警 · 安全防护</text>
    </view>

    <!-- 顶部统计卡片 -->
    <view class="stats-bar">
      <view class="stat-item">
        <text class="stat-num">{{ devices.length }}</text>
        <text class="stat-label">设备总数</text>
      </view>
      <view class="stat-item">
        <text class="stat-num" :style="{color: onlineCount === devices.length ? '#00c853' : '#ffaa00'}">
          {{ onlineCount }}
        </text>
        <text class="stat-label">在线设备</text>
      </view>
      <view class="stat-item">
        <text class="stat-num alarm">{{ alarmCount }}</text>
        <text class="stat-label">报警中</text>
      </view>
    </view>

    <!-- 设备列表 -->
    <scroll-view
      scroll-y
      class="device-list"
      refresher-enabled
      :refresher-triggered="isRefreshing"
      @refresherrefresh="onRefresh"
    >
      <view
        v-for="device in devices"
        :key="device.id"
        class="device-card"
        :class="{ 'alarm-card': device.alarm }"
        @click="goDetail(device.id)"
      >
        <!-- 卡片头部 -->
        <view class="card-header">
          <view class="device-info">
            <text class="device-name">{{ device.name || device.id }}</text>
            <text class="device-location">📍 {{ device.location || '未设置位置' }}</text>
          </view>
          <view class="status-badge" :class="device.alarm ? 'status-alarm' : 'status-normal'">
            <text class="status-icon">{{ device.alarm ? '🔥' : '✓' }}</text>
            <text>{{ device.alarm ? '报警' : '正常' }}</text>
          </view>
        </view>

        <!-- 传感器数据 -->
        <view class="sensor-grid">
          <view class="sensor-item">
            <text class="sensor-icon">🌡️</text>
            <text class="sensor-value" :style="{color: getTempColor(device.temperature)}">{{ device.temperature || 0 }}°C</text>
            <text class="sensor-label">温度</text>
          </view>
          <view class="sensor-item">
            <text class="sensor-icon">📏</text>
            <text class="sensor-value">{{ device.distance || 0 }}cm</text>
            <text class="sensor-label">距离</text>
          </view>
          <view class="sensor-item">
            <text class="sensor-icon">🎨</text>
            <text class="sensor-value" :style="{color: getColorHex(device)}">{{ device.color_status || 'N/A' }}</text>
            <text class="sensor-label">颜色</text>
          </view>
          <view class="sensor-item">
            <text class="sensor-icon">🔌</text>
            <text class="sensor-value">{{ device.boot_count || 0 }}</text>
            <text class="sensor-label">启动次数</text>
          </view>
        </view>

        <!-- 报警原因 -->
        <view v-if="device.alarm && device.alarm_reason" class="alarm-reason">
          <text class="alarm-icon">⚠️</text>
          <text>{{ device.alarm_reason }}</text>
        </view>

        <!-- 更新时间 -->
        <view class="update-time">
          <text class="time-icon">🕐</text>
          <text>更新于 {{ formatTime(device.lastSeen) }}</text>
        </view>
      </view>

      <!-- 空状态 -->
      <view v-if="devices.length === 0 && !isRefreshing" class="empty-state">
        <text class="empty-icon">📡</text>
        <text class="empty-text">暂无设备数据</text>
        <text class="empty-hint">请检查设备是否在线或服务器连接</text>
      </view>
    </scroll-view>

    <!-- 底部刷新提示 -->
    <view class="refresh-hint">
      <text>🔥 消防守护 · 每 5 秒自动刷新 · 下拉手动刷新</text>
    </view>
  </view>
</template>

<script>
import { getDevices } from '@/utils/request.js';

export default {
  data() {
    return {
      devices: [],
      isRefreshing: false,
      timer: null,
      isLoading: false
    };
  },

  computed: {
    onlineCount() {
      const now = Date.now();
      return this.devices.filter(d => {
        const last = d.lastSeen ? new Date(d.lastSeen).getTime() : 0;
        return now - last < 5 * 60 * 1000;
      }).length;
    },
    alarmCount() {
      return this.devices.filter(d => d.alarm).length;
    }
  },

  onLoad() {
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
    this.loadDevices();
  },

  methods: {
    startPolling() {
      if (this.timer) return;
      this.timer = setInterval(() => {
        this.loadDevices();
      }, 5000);
    },

    stopPolling() {
      if (this.timer) {
        clearInterval(this.timer);
        this.timer = null;
      }
    },

    async loadDevices() {
      if (this.isLoading) return;
      this.isLoading = true;
      try {
        const data = await getDevices();
        this.devices = Array.isArray(data) ? data : [];
      } catch (e) {
        console.error('获取设备失败:', e);
      } finally {
        this.isLoading = false;
      }
    },

    onRefresh() {
      this.isRefreshing = true;
      this.loadDevices().finally(() => {
        setTimeout(() => {
          this.isRefreshing = false;
        }, 500);
      });
    },

    goDetail(deviceId) {
      uni.navigateTo({
        url: `/pages/detail/detail?id=${deviceId}`
      });
    },

    getTempColor(temp) {
      const t = temp || 0;
      if (t > 50) return '#ff1744';
      if (t > 35) return '#ffaa00';
      return '#00c853';
    },

    getColorHex(device) {
      const status = (device.color_status || '').toLowerCase();
      if (status === 'red') return '#ff1744';
      if (status === 'yellow') return '#ffaa00';
      if (status === 'green') return '#00c853';
      return '#8899aa';
    },

    formatTime(ts) {
      if (!ts) return '未知';
      const date = new Date(ts);
      const now = new Date();
      const diff = now - date;
      
      if (diff < 60000) return '刚刚';
      if (diff < 3600000) return Math.floor(diff / 60000) + '分钟前';
      
      return date.toLocaleString('zh-CN', {
        month: 'short',
        day: 'numeric',
        hour: '2-digit',
        minute: '2-digit'
      });
    }
  }
};
</script>

<style scoped>
.container {
  min-height: 100vh;
  background: #0a0e1a;
  padding-bottom: 30rpx;
}

/* 消防主题头部 */
.fire-header {
  background: linear-gradient(135deg, #1a1f2e 0%, #0d1117 100%);
  padding: 30rpx 30rpx 20rpx;
  border-bottom: 2rpx solid #ff3333;
}

.fire-badge {
  display: flex;
  align-items: center;
  margin-bottom: 8rpx;
}

.fire-icon {
  font-size: 48rpx;
  margin-right: 16rpx;
}

.fire-title {
  font-size: 40rpx;
  font-weight: bold;
  color: #ff3333;
  letter-spacing: 4rpx;
}

.fire-subtitle {
  font-size: 22rpx;
  color: #8899aa;
  margin-left: 64rpx;
  letter-spacing: 2rpx;
}

/* 统计栏 - 暗色消防风 */
.stats-bar {
  display: flex;
  justify-content: space-around;
  background: linear-gradient(135deg, #1a1f2e 0%, #252b3d 100%);
  padding: 40rpx 20rpx;
  margin: 20rpx;
  border-radius: 20rpx;
  border: 1rpx solid #2a3040;
}

.stat-item {
  display: flex;
  flex-direction: column;
  align-items: center;
}

.stat-num {
  font-size: 52rpx;
  font-weight: bold;
  color: #e0e0e0;
  text-shadow: 0 0 20rpx rgba(224,224,224,0.3);
}

.stat-num.alarm {
  color: #ff3333;
  text-shadow: 0 0 20rpx rgba(255,51,51,0.5);
  animation: alarmPulse 2s ease-in-out infinite;
}

@keyframes alarmPulse {
  0%, 100% { opacity: 1; text-shadow: 0 0 20rpx rgba(255,51,51,0.5); }
  50% { opacity: 0.8; text-shadow: 0 0 40rpx rgba(255,51,51,0.8); }
}

.stat-label {
  font-size: 24rpx;
  color: #8899aa;
  margin-top: 8rpx;
}

/* 设备列表 */
.device-list {
  height: calc(100vh - 320rpx);
  padding: 0 20rpx;
}

.device-card {
  background: linear-gradient(135deg, #1a1f2e 0%, #151921 100%);
  border-radius: 20rpx;
  padding: 30rpx;
  margin-bottom: 20rpx;
  border: 1rpx solid #2a3040;
  box-shadow: 0 4rpx 20rpx rgba(0,0,0,0.3);
}

.alarm-card {
  border: 2rpx solid #ff3333;
  background: linear-gradient(135deg, #2a1515 0%, #1a1f2e 100%);
  box-shadow: 0 4rpx 30rpx rgba(255,51,51,0.2);
  animation: borderPulse 2s ease-in-out infinite;
}

@keyframes borderPulse {
  0%, 100% { box-shadow: 0 4rpx 20rpx rgba(255,51,51,0.2); }
  50% { box-shadow: 0 4rpx 40rpx rgba(255,51,51,0.4); }
}

/* 卡片头部 */
.card-header {
  display: flex;
  justify-content: space-between;
  align-items: flex-start;
  margin-bottom: 24rpx;
}

.device-name {
  font-size: 36rpx;
  font-weight: bold;
  color: #e0e0e0;
  display: block;
}

.device-location {
  font-size: 24rpx;
  color: #8899aa;
  margin-top: 8rpx;
  display: block;
}

.status-badge {
  padding: 8rpx 20rpx;
  border-radius: 24rpx;
  font-size: 24rpx;
  font-weight: 500;
  display: flex;
  align-items: center;
}

.status-normal {
  background: rgba(0,200,83,0.15);
  color: #00c853;
  border: 1rpx solid rgba(0,200,83,0.3);
}

.status-alarm {
  background: rgba(255,51,51,0.15);
  color: #ff3333;
  border: 1rpx solid rgba(255,51,51,0.3);
  animation: alarmPulse 1.5s ease-in-out infinite;
}

.status-icon {
  margin-right: 6rpx;
}

/* 传感器网格 */
.sensor-grid {
  display: flex;
  justify-content: space-between;
  padding: 20rpx 0;
  border-top: 1rpx solid #2a3040;
  border-bottom: 1rpx solid #2a3040;
}

.sensor-item {
  display: flex;
  flex-direction: column;
  align-items: center;
  flex: 1;
}

.sensor-icon {
  font-size: 40rpx;
  margin-bottom: 8rpx;
}

.sensor-value {
  font-size: 32rpx;
  font-weight: 600;
  color: #e0e0e0;
}

.sensor-label {
  font-size: 22rpx;
  color: #8899aa;
  margin-top: 4rpx;
}

/* 报警原因 */
.alarm-reason {
  margin-top: 20rpx;
  padding: 16rpx 20rpx;
  background: rgba(255,51,51,0.1);
  border-radius: 12rpx;
  color: #ff6666;
  font-size: 26rpx;
  border-left: 4rpx solid #ff3333;
  display: flex;
  align-items: center;
}

.alarm-icon {
  margin-right: 8rpx;
}

/* 更新时间 */
.update-time {
  margin-top: 16rpx;
  font-size: 22rpx;
  color: #667788;
  text-align: right;
  display: flex;
  align-items: center;
  justify-content: flex-end;
}

.time-icon {
  margin-right: 6rpx;
}

/* 空状态 */
.empty-state {
  display: flex;
  flex-direction: column;
  align-items: center;
  padding: 100rpx 40rpx;
}

.empty-icon {
  font-size: 80rpx;
  margin-bottom: 20rpx;
}

.empty-text {
  font-size: 32rpx;
  color: #8899aa;
  margin-bottom: 12rpx;
}

.empty-hint {
  font-size: 24rpx;
  color: #667788;
  text-align: center;
}

/* 刷新提示 */
.refresh-hint {
  text-align: center;
  font-size: 20rpx;
  color: #556677;
  padding: 16rpx 0;
}
</style>
