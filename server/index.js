const express = require('express');
const http = require('http');
const { WebSocketServer } = require('ws');
const path = require('path');

const app = express();
const server = http.createServer(app);
const wss = new WebSocketServer({ server });

const PORT = process.env.PORT || 3000;

// Serve static files
app.use(express.static(path.join(__dirname, 'public')));

// In-memory state
let screens = [];
let deviceSettings = { brightness: 255, textSize: 1 };
const clients = { devices: new Map(), browsers: new Map() };
let clientIdCounter = 0;

// Message history - per day
const messageHistory = new Map();

function getTodayKey() {
  return new Date().toISOString().split('T')[0];
}

function getTodayHistory() {
  const key = getTodayKey();
  if (!messageHistory.has(key)) {
    messageHistory.set(key, []);
  }
  return messageHistory.get(key);
}

function addToHistory(screenTexts) {
  const history = getTodayHistory();
  const now = new Date().toISOString();
  for (const screen of screenTexts) {
    if (screen.text && screen.text.trim().length > 0) {
      history.push({ text: screen.text, sentAt: now, id: screen.id });
    }
  }
}

function cleanOldHistory() {
  const cutoff = new Date();
  cutoff.setDate(cutoff.getDate() - 7);
  const cutoffKey = cutoff.toISOString().split('T')[0];
  for (const key of messageHistory.keys()) {
    if (key < cutoffKey) messageHistory.delete(key);
  }
}

function broadcast(targets, message) {
  const data = JSON.stringify(message);
  for (const [, ws] of targets) {
    if (ws.readyState === ws.OPEN) ws.send(data);
  }
}

// Heartbeat - ping all devices every 5 seconds
setInterval(() => {
  for (const [id, ws] of clients.devices) {
    if (ws.isAlive === false) {
      console.log(`[-] device heartbeat timeout (id=${id})`);
      ws.terminate();
      clients.devices.delete(id);
      broadcast(clients.browsers, {
        type: 'device_status',
        online: clients.devices.size > 0,
        deviceCount: clients.devices.size
      });
      continue;
    }
    ws.isAlive = false;
    ws.ping();
  }
}, 5000);

wss.on('connection', (ws, req) => {
  const params = new URL(req.url, `http://${req.headers.host}`).searchParams;
  const role = params.get('role') || 'browser';
  const token = params.get('token') || 'anonymous';
  const id = ++clientIdCounter;

  ws.isAlive = true;
  ws.on('pong', () => { ws.isAlive = true; });

  console.log(`[+] ${role} connected (id=${id}, token=${token})`);

  if (role === 'device') {
    clients.devices.set(id, ws);
    if (screens.length > 0) {
      ws.send(JSON.stringify({ type: 'screens', data: screens }));
    }
    // Send current settings
    ws.send(JSON.stringify({ type: 'settings', ...deviceSettings }));
    broadcast(clients.browsers, {
      type: 'device_status',
      online: true,
      deviceCount: clients.devices.size
    });
  } else {
    clients.browsers.set(id, ws);
    ws.send(JSON.stringify({
      type: 'init',
      screens,
      deviceOnline: clients.devices.size > 0,
      history: getTodayHistory(),
      settings: deviceSettings
    }));
  }

  ws.on('message', (raw) => {
    let msg;
    try { msg = JSON.parse(raw); } catch { return; }

    console.log(`[${role}:${id}] ${msg.type}`);

    switch (msg.type) {
      case 'screens':
        screens = msg.data || [];
        broadcast(clients.devices, { type: 'screens', data: screens });
        addToHistory(screens);
        cleanOldHistory();
        broadcast(clients.browsers, {
          type: 'screens_sent',
          count: screens.length,
          history: getTodayHistory()
        });
        break;

      case 'get_history':
        ws.send(JSON.stringify({ type: 'history', data: getTodayHistory() }));
        break;

      case 'resend':
        if (msg.text) {
          const resendScreens = [{ id: 1, text: msg.text }];
          screens = resendScreens;
          broadcast(clients.devices, { type: 'screens', data: resendScreens });
          addToHistory(resendScreens);
          broadcast(clients.browsers, {
            type: 'screens_sent', count: 1, history: getTodayHistory()
          });
        }
        break;

      case 'settings':
        // Browser sending display settings
        if (msg.brightness !== undefined) deviceSettings.brightness = msg.brightness;
        if (msg.textSize !== undefined) deviceSettings.textSize = msg.textSize;
        broadcast(clients.devices, { type: 'settings', ...deviceSettings });
        break;

      case 'status':
        broadcast(clients.browsers, { type: 'device_status', ...msg, online: true });
        break;

      case 'touch':
        broadcast(clients.browsers, { type: 'touch', gesture: msg.gesture });
        break;

      default:
        break;
    }
  });

  ws.on('close', () => {
    console.log(`[-] ${role} disconnected (id=${id})`);
    if (role === 'device') {
      clients.devices.delete(id);
      broadcast(clients.browsers, {
        type: 'device_status',
        online: clients.devices.size > 0,
        deviceCount: clients.devices.size
      });
    } else {
      clients.browsers.delete(id);
    }
  });
});

server.listen(PORT, () => {
  console.log(`ESP Display Server running on http://localhost:${PORT}`);
  console.log(`WebSocket endpoint: ws://localhost:${PORT}/ws`);
});
