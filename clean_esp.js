const fs = require('fs');
const path = require('path');

const extPath = path.join(process.env.USERPROFILE || process.env.HOME, '.vscode', 'extensions', 'extensions.json');
const data = JSON.parse(fs.readFileSync(extPath, 'utf8'));

const originalCount = data.length;
const filtered = data.filter(ext => !(ext.identifier && ext.identifier.id && ext.identifier.id.includes('espressif')));
const removed = originalCount - filtered.length;

console.log(`Removed ${removed} ESP entries, ${filtered.length} extensions remain`);
fs.writeFileSync(extPath, JSON.stringify(filtered, null, 2), 'utf8');
console.log('extensions.json cleaned');
