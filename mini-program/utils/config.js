// 环境配置文件
// 修改此处即可切换服务器地址，无需改动其他代码
const CONFIG = {
  // 开发环境
  dev: {
    API_BASE: 'http://172.20.10.3:5000'
  },
  // 生产环境（审批时必须使用 HTTPS + 备案域名）
  prod: {
    API_BASE: 'https://你的域名.com'
  }
};

// 当前环境，开发时保持 'dev'，提交审核前改为 'prod'
const ENV = 'dev';

module.exports = {
  API_BASE: CONFIG[ENV].API_BASE
};