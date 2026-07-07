<script>
export default {
  onLaunch() {
    console.log('App Launch');
  },
  onShow() {
    console.log('App Show');
  },
  onHide() {
    console.log('App Hide');
  }
};
</script>

<style>
/* 全局暗色消防主题 */
page {
  background-color: #0a0e1a;
  font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif;
  color: #e0e0e0;
}

/* 全局滚动条样式 */
::-webkit-scrollbar {
  width: 0;
  height: 0;
}
</style>
