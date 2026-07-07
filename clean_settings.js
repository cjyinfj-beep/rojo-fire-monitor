const fs = require('fs');
const path = require('path');

const settingsPath = path.join(process.env.APPDATA, 'Code', 'User', 'settings.json');
const content = fs.readFileSync(settingsPath, 'utf8');
const data = JSON.parse(content);

const removed = [];
for (const key in data) {
  if (key.startsWith('idf.')) {
    removed.push(key);
    delete data[key];
  }
}

fs.writeFileSync(settingsPath, JSON.stringify(data, null, 2), 'utf8');
console.log('Removed ' + removed.length + ' ESP-IDF settings:');
removed.forEach(k => console.log('  - ' + k));
