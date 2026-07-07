<template>
  <view class="container">
    <!-- 消防主题标题区 -->
    <view class="fire-header">
      <text class="fire-icon">🚨</text>
      <view class="fire-title-box">
        <text class="fire-title">报警管理中心</text>
        <text class="fire-subtitle">及时处理 · 确保安全</text>
      </view>
    </view>

    <!-- 设备选择器（如果是全局报警页） -->
    <view v-if="!deviceId" class="device-selector">
      <picker mode="selector" :range="deviceOptions" :value="selectedIndex" @change="onDeviceChange">
        <view class="picker-box">
          <text>{{ currentDeviceName }}</text>
          <text class="picker-arrow">▼</text>
        </view>
      </picker>
    </view>

    <!-- 统计摘要 -->
    <view class="summary-bar">
      <view class="summary-item">
        <text class="summary-num">{{ alarms.length }}</text>
        <text class="summary-label">报警总数</text>
      </view>
      <view class="summary-item">
        <text class="summary-num" style="color:#00c853">{{ resolvedCount }}</text>
        <text class="summary-label">已处理</text>
      </view>
      <view class="summary-item">
        <text class="summary-num alarm">{{ unresolvedCount }}</text>
        <text class="summary-label">待处理</text>
      </view>
    </view>

    <!-- 筛选标签 -->
    <view class="filter-tabs">
      <text
        v-for="tab in filterTabs"
        :key="tab.value"
        class="filter-tab"
        :class="{ active: filter === tab.value }"
        @click="filter = tab.value"
      >{{ tab.label }}</text>
    </view>

    <!-- 报警列表 -->
    <scroll-view
      scroll-y
      class="alarm-list"
      refresher-enabled
      :refresher-triggered="isRefreshing"
      @refresherrefresh="onRefresh"
    >
      <view
        v-for="alarm in filteredAlarms"
        :key="alarm.id"
        class="alarm-item"
        :class="{ resolved: alarm.is_resolved }"
      >
        <view class="alarm-main">
          <view class="alarm-icon" :class="alarm.is_resolved ? 'icon-resolved' : 'icon-active'">
            <text>{{ alarm.is_resolved ? '✓' : '!' }}</text>
          </view>
          <view class="alarm-content">
            <view class="alarm-header-row">
              <text class="alarm-type">{{ alarm.alarm_type || '传感器报警' }}</text>
              <text v-if="!alarm.is_resolved" class="alarm-badge">待处理</text>
            </view>
            <text class="alarm-detail">{{ alarm.detail || '设备检测到异常' }}</text>
            <text class="alarm-time">🕐 {{ formatTime(alarm.created_at) }}</text>
          </view>
        </view>
        <view class="alarm-actions">
          <view
            v-if="!alarm.is_resolved"
            class="resolve-btn"
            @click="resolveAlarm(alarm.id)"
          >
            <text>✓ 标记已处理</text>
          </view>
          <view v-else class="resolved-tag">
            <text>✓ 已处理</text>
            <text v-if="alarm.resolved_at" class="resolved-time">{{ formatTime(alarm.resolved_at) }}</text>
          </view>
        </view>
      </view>

      <!-- 空状态 -->
      <view v-if="filteredAlarms.length === 0" class="empty-state">
        <text class="empty-icon">🛡️</text>
        <text class="empty-text">暂无报警记录</text>
        <text class="empty-hint">一切正常，继续保持！</text>
      </view>
    </scroll-view>
  </view>
</template>

<script>
import { getDeviceAlarms, getDevices } from '@/utils/request.js';

export default {
  data() {
    return {
      deviceId: '',
      alarms: [],
      devices: [],
      selectedIndex: 0,
      filter: 'all',
      isRefreshing: false,
      isLoading: false,
      filterTabs: [
        { label: '全部', value: 'all' },
        { label: '待处理', value: 'unresolved' },
        { label: '已处理', value: 'resolved' }
      ]
    };
  },

  computed: {
    deviceOptions() {
      return this.devices.map(d => d.name || d.id);
    },
    currentDeviceName() {
      if (this.deviceId) {
        const d = this.devices.find(x => x.id === this.deviceId);
        return d ? (d.name || d.id) : '加载中...';
      }
      return this.deviceOptions[this.selectedIndex] || '选择设备';
    },
    filteredAlarms() {
      if (this.filter === 'all') return this.alarms;
      if (this.filter === 'unresolved') return this.alarms.filter(a => !a.is_resolved);
      if (this.filter === 'resolved') return this.alarms.filter(a => a.is_resolved);
      return this.alarms;
    },
    resolvedCount() {
      return this.alarms.filter(a => a.is_resolved).length;
    },
    unresolvedCount() {
      return this.alarms.filter(a => !a.is_resolved).length;
    }
  },

  onLoad(options) {
    this.deviceId = options.id || '';
    this.loadData();
  },

  onShow() {
    const storedId = uni.getStorageSync('alarmDeviceId');
    if (storedId) {
      this.deviceId = storedId;
      uni.removeStorageSync('alarmDeviceId');
    }
    this.loadData();
  },

  methods: {
    async loadData() {
      if (this.isLoading) return;
      this.isLoading = true;

      try {
        if (!this.deviceId) {
          const data = await getDevices() || [];
          this.devices = Array.isArray(data) ? data : [];
          if (this.devices.length > 0 && !this.deviceId) {
            this.deviceId = this.devices[0].id;
          }
        }

        if (this.deviceId) {
          await this.loadAlarms();
        }
      } catch (e) {
        console.error('加载数据失败:', e);
      } finally {
        this.isLoading = false;
      }
    },

    async loadAlarms() {
      try {
        const data = await getDeviceAlarms(this.deviceId);
        this.alarms = Array.isArray(data) ? data : [];
      } catch (e) {
        console.error('获取报警记录失败:', e);
      }
    },

    onRefresh() {
      this.isRefreshing = true;
      this.loadData().finally(() => {
        setTimeout(() => {
          this.isRefreshing = false;
        }, 500);
      });
    },

    onDeviceChange(e) {
      this.selectedIndex = e.detail.value;
      if (this.devices[this.selectedIndex]) {
        this.deviceId = this.devices[this.selectedIndex].id;
        this.loadAlarms();
      }
    },

    resolveAlarm(alarmId) {
      uni.showModal({
        title: '确认处理',
        content: '标记此报警为已处理？',
        success: (res) => {
          if (res.confirm) {
            const alarm = this.alarms.find(a => a.id === alarmId);
            if (alarm) {
              alarm.is_resolved = true;
              alarm.resolved_at = new Date().toISOString();
              uni.showToast({ title: '已标记处理', icon: 'success' });
            }
          }
        }
      });
    },

    formatTime(ts) {
      if (!ts) return '未知时间';
      const date = new Date(ts);
      const now = new Date();
      const diff = now - date;

      if (diff < 60000) return '刚刚';
      if (diff < 3600000) return Math.floor(diff / 60000) + '分钟前';
      if (diff < 86400000) return Math.floor(diff / 3600000) + '小时前';

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
}

/* 消防主题头部 */
.fire-header {
  background: linear-gradient(135deg, #1a1f2e 0%, #0d1117 100%);
  padding: 30rpx;
  display: flex;
  align-items: center;
  border-bottom: 2rpx solid #ff3333;
}

.fire-icon {
  font-size: 56rpx;
  margin-right: 20rpx;
}

.fire-title-box {
  display: flex;
  flex-direction: column;
}

.fire-title {
  font-size: 36rpx;
  font-weight: bold;
  color: #ff3333;
  letter-spacing: 2rpx;
}

.fire-subtitle {
  font-size: 22rpx;
  color: #8899aa;
  margin-top: 4rpx;
}

/* 设备选择器 */
.device-selector {
  background: #1a1f2e;
  padding: 20rpx 30rpx;
  border-bottom: 1rpx solid #2a3040;
}

.picker-box {
  display: flex;
  justify-content: space-between;
  align-items: center;
  font-size: 30rpx;
  color: #e0e0e0;
  padding: 16rpx 20rpx;
  background: #0d1117;
  border-radius: 12rpx;
  border: 1rpx solid #2a3040;
}

.picker-arrow {
  color: #ff3333;
  font-size: 24rpx;
}

/* 统计栏 */
.summary-bar {
  display: flex;
  justify-content: space-around;
  background: linear-gradient(135deg, #1a1f2e 0%, #252b3d 100%);
  padding: 30rpx 20rpx;
  margin: 20rpx;
  border-radius: 20rpx;
  border: 1rpx solid #2a3040;
}

.summary-item {
  display: flex;
  flex-direction: column;
  align-items: center;
}

.summary-num {
  font-size: 48rpx;
  font-weight: bold;
  color: #e0e0e0;
}

.summary-num.alarm {
  color: #ff3333;
  animation: alarmPulse 2s ease-in-out infinite;
}

@keyframes alarmPulse {
  0%, 100% { opacity: 1; text-shadow: 0 0 20rpx rgba(255,51,51,0.5); }
  50% { opacity: 0.8; text-shadow: 0 0 40rpx rgba(255,51,51,0.8); }
}

.summary-label {
  font-size: 24rpx;
  color: #8899aa;
  margin-top: 8rpx;
}

/* 筛选标签 */
.filter-tabs {
  display: flex;
  padding: 20rpx 30rpx;
  margin-bottom: 10rpx;
}

.filter-tab {
  padding: 12rpx 30rpx;
  margin-right: 20rpx;
  font-size: 26rpx;
  color: #8899aa;
  border-radius: 30rpx;
  background: #1a1f2e;
  border: 1rpx solid #2a3040;
}

.filter-tab.active {
  background: linear-gradient(135deg, #ff3333 0%, #ff6600 100%);
  color: #fff;
  font-weight: 500;
  border: none;
}

/* 报警列表 */
.alarm-list {
  height: calc(100vh - 420rpx);
  padding: 0 20rpx;
}

.alarm-item {
  background: linear-gradient(135deg, #1a1f2e 0%, #151921 100%);
  border-radius: 20rpx;
  padding: 30rpx;
  margin-bottom: 20rpx;
  border: 1rpx solid #2a3040;
  box-shadow: 0 4rpx 20rpx rgba(0,0,0,0.3);
}

.alarm-item.resolved {
  opacity: 0.7;
  border: 1rpx solid #1a3a1a;
}

.alarm-main {
  display: flex;
  align-items: flex-start;
}

.alarm-icon {
  width: 56rpx;
  height: 56rpx;
  border-radius: 28rpx;
  display: flex;
  align-items: center;
  justify-content: center;
  font-size: 28rpx;
  font-weight: bold;
  margin-right: 20rpx;
  flex-shrink: 0;
}

.icon-active {
  background: rgba(255,51,51,0.15);
  color: #ff3333;
  border: 1rpx solid rgba(255,51,51,0.3);
  animation: alarmPulse 2s ease-in-out infinite;
}

.icon-resolved {
  background: rgba(0,200,83,0.15);
  color: #00c853;
  border: 1rpx solid rgba(0,200,83,0.3);
}

.alarm-content {
  flex: 1;
}

.alarm-header-row {
  display: flex;
  align-items: center;
  margin-bottom: 8rpx;
}

.alarm-type {
  font-size: 30rpx;
  font-weight: bold;
  color: #e0e0e0;
}

.alarm-badge {
  margin-left: 12rpx;
  padding: 4rpx 12rpx;
  background: rgba(255,51,51,0.2);
  color: #ff3333;
  font-size: 20rpx;
  border-radius: 8rpx;
}

.alarm-detail {
  font-size: 26rpx;
  color: #8899aa;
  margin-top: 8rpx;
  display: block;
}

.alarm-time {
  font-size: 22rpx;
  color: #667788;
  margin-top: 12rpx;
  display: block;
}

.alarm-actions {
  display: flex;
  justify-content: flex-end;
  margin-top: 20rpx;
  padding-top: 20rpx;
  border-top: 1rpx solid #2a3040;
}

.resolve-btn {
  padding: 12rpx 30rpx;
  background: linear-gradient(135deg, #ff3333 0%, #ff6600 100%);
  color: #fff;
  border-radius: 30rpx;
  font-size: 24rpx;
  display: flex;
  align-items: center;
}

.resolved-tag {
  padding: 12rpx 24rpx;
  background: rgba(0,200,83,0.1);
  color: #00c853;
  border-radius: 30rpx;
  font-size: 24rpx;
  display: flex;
  flex-direction: column;
  align-items: flex-end;
}

.resolved-time {
  font-size: 20rpx;
  color: #667788;
  margin-top: 4rpx;
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
}
</style>
