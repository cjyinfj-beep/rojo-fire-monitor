const fs = require('fs');
const path = require('path');

const settingsPath = path.join(process.env.APPDATA, 'Code', 'User', 'settings.json');
const content = fs.readFileSync(settingsPath, 'utf8');

// Split by lines and parse manually to handle multiple JSON objects
const lines = content.split('\n');
const merged = {};

let buffer = '';
let braceCount = 0;
let inString = false;
let escapeNext = false;

for (const line of lines) {
  for (let i = 0; i < line.length; i++) {
    const ch = line[i];
    if (escapeNext) {
      buffer += ch;
      escapeNext = false;
      continue;
    }
    if (ch === '\') {
      buffer += ch;
      escapeNext = true;
      continue;
    }
    if (ch === '"') {
      inString = !inString;
      buffer += ch;
      continue;
    }
    if (!inString) {
      if (ch === '{') braceCount++;
      if (ch === '}') braceCount--;
    }
    buffer += ch;
  }
  buffer += '\n';
  
  if (braceCount === 0 && buffer.trim().length > 0) {
    try {
      const obj = JSON.parse(buffer.trim());
      Object.assign(merged, obj);
      buffer = '';
    } catch (e) {
      // might be partial, keep buffering
    }
  }
}

// Remove any remaining ESP-IDF settings
const removed = [];
for (const key in merged) {
  if (key.startsWith('idf.')) {
    removed.push(key);
    delete merged[key];
  }
}

fs.writeFileSync(settingsPath, JSON.stringify(merged, null, 4), 'utf8');
console.log('Fixed settings.json. Removed ' + removed.length + ' ESP-IDF settings:');
removed.forEach(k => console.log('  - ' + k));
