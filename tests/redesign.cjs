const { chromium } = require('playwright');
const fs = require('node:fs');
const path = require('node:path');
const assert = require('node:assert/strict');
const root = path.resolve(__dirname, '..');
(async () => {
  const browser = await chromium.launch({ headless: true });
  try {
    const page = await browser.newPage({ viewport: { width: 1440, height: 1100 } });
    let settings = { timezone: 'America/Denver', brightness: 200, layoutId: 0, themeId: 0, use24Hour: false };
    let focus = { durationSeconds: 1500, remainingSeconds: 1500, running: false, completed: false, completedSessions: 0 };
    let events = [
      { id: '1', name: 'A weekend in the mountains', targetDate: Math.floor(Date.now()/1000) + 86400*12, accentColor: 0xe87545, showOnMain: true },
      { id: '2', name: 'Birthday dinner', targetDate: Math.floor(Date.now()/1000) + 86400*24, accentColor: 0x62866a, showOnMain: true }
    ];
    let failSettings = false;
    const errors = [];
    page.on('pageerror', error => errors.push(error.message));
    await page.routeWebSocket('**/ws', ws => {
      ws.send(JSON.stringify({ type: 'time', time: '10:42:08 AM', date: 'September 13, 2026', weekday: 'Sunday', epoch: Math.floor(Date.now()/1000), focus }));
    });
    await page.route('http://clocky.test/**', async route => {
      const pathname = new URL(route.request().url()).pathname;
      const method = route.request().method();
      if (pathname === '/api/settings') {
        if (method === 'PUT') {
          if (failSettings) return route.fulfill({ status: 500, json: { error: 'Failed' } });
          settings = { ...settings, ...route.request().postDataJSON() };
        }
        return route.fulfill({ json: settings });
      }
      if (pathname === '/api/status') return route.fulfill({ json: { connected: true, ssid: 'Studio Wi-Fi', ip: '192.168.1.152', timeSynced: true, uptime: 72534, freeHeap: 185600, firmwareVersion: '1.1.0', epoch: Math.floor(Date.now()/1000) } });
      if (pathname === '/api/focus') {
        if (method === 'POST') {
          const data = route.request().postDataJSON();
          if (data.action === 'start') focus = { ...focus, durationSeconds: data.durationSeconds, remainingSeconds: data.durationSeconds - 1, running: true };
          if (data.action === 'pause') focus.running = false;
          if (data.action === 'resume') focus.running = true;
          if (data.action === 'reset') focus = { ...focus, remainingSeconds: focus.durationSeconds, running: false };
        }
        return route.fulfill({ json: focus });
      }
      if (pathname === '/api/countdowns') {
        if (method === 'POST') events.push({ ...route.request().postDataJSON(), id: 'new' });
        return route.fulfill({ json: events });
      }
      if (pathname.startsWith('/api/countdowns/')) {
        if (method === 'DELETE') events = events.filter(cd => cd.id !== pathname.split('/').pop());
        return route.fulfill({ json: { success: true } });
      }
      if (pathname === '/api/wifi/scan') return route.fulfill({ json: [{ ssid: 'Studio', rssi: -45, encryption: 'secured' }] });
      const file = pathname === '/' ? 'index.html' : pathname.slice(1);
      if (!fs.existsSync(path.join(root, 'data', file))) return route.fulfill({ status: 404, body: '' });
      return route.fulfill({ body: fs.readFileSync(path.join(root, 'data', file)), contentType: file.endsWith('.css') ? 'text/css' : file.endsWith('.js') ? 'text/javascript' : 'text/html' });
    });
    await page.goto('http://clocky.test/');
    await page.waitForFunction(() => app.focusState && app.status && app.countdowns.length === 2);
    await page.waitForTimeout(500);
    if (process.env.CLOCKY_DESKTOP_SCREENSHOT) await page.screenshot({ path: process.env.CLOCKY_DESKTOP_SCREENSHOT, fullPage: true });
    await page.click('#focusToggleBtn');
    await page.waitForFunction(() => app.focusState.running);
    await page.click('#focusToggleBtn');
    await page.waitForFunction(() => !app.focusState.running);
    assert.match(await page.textContent('#focusToggleBtn'), /Resume/);
    await page.click('#focusToggleBtn');
    await page.waitForFunction(() => app.focusState.running);
    await page.click('[data-minutes="5"]');
    await page.click('#focusToggleBtn');
    await page.click('#actionDialog button[value="ok"]');
    await page.waitForFunction(() => app.focusState.durationSeconds === 300);
    await page.click('#focusResetBtn');
    await page.click('#actionDialog button[value="ok"]');
    await page.waitForFunction(() => !app.focusState.running && app.focusState.remainingSeconds === 300);
    await page.click('#addWorldBtn');
    await page.selectOption('#actionSelect', 'America/New_York');
    await page.click('#actionDialog button[value="ok"]');
    await page.waitForFunction(() => document.querySelectorAll('.world-row').length === 3);
    assert.equal(await page.locator('.world-row').count(), 3);
    assert.match(await page.evaluate(() => localStorage.getItem('clocky-world-clocks')), /New_York/);
    await page.click('[aria-label="Remove New York world clock"]');
    assert.equal(await page.locator('.world-row').count(), 2);
    await page.click('#deskModeBtn');
    assert.equal(await page.locator('body').evaluate(el => el.classList.contains('desk-mode')), true);
    await page.keyboard.press('Escape');
    await page.click('button[data-layout="2"]');
    await page.waitForFunction(() => app.settings.layoutId === 2);
    assert.equal(await page.locator('.analog-face').isVisible(), true);
    await page.click('#customizeBtn');
    await page.click('[data-theme="3"]');
    await page.waitForFunction(() => app.settings.themeId === 3);
    await page.locator('#brightnessSlider').fill('0');
    await page.locator('#brightnessSlider').dispatchEvent('change');
    await page.waitForFunction(() => app.settings.brightness === 0);
    failSettings = true;
    await page.selectOption('#timeFormatSelect', '24');
    await page.waitForFunction(() => document.getElementById('saveStatus').textContent.includes('Not saved'));
    failSettings = false;
    await page.click('[data-tab="countdowns"]');
    await page.fill('#countdownSearch', 'mountains');
    await page.waitForFunction(() => document.querySelectorAll('.countdown-item').length === 1);
    await page.fill('#countdownSearch', 'no matches');
    await page.waitForSelector('.empty-state');
    await page.fill('#countdownSearch', '');
    await page.click('[data-template="Vacation"]');
    assert.equal(await page.inputValue('#cdName'), 'Vacation');
    await page.fill('#cdDate', '2027-07-14');
    await page.fill('#cdTime', '12:30');
    await page.click('#countdownForm button[type="submit"]');
    await page.waitForFunction(() => !document.getElementById('countdownModal').classList.contains('active'));
    assert.equal(events.length, 3);
    await page.click('[data-tab="clock"]');
    await page.click('button[data-layout="0"]');
    await page.click('#customizeBtn');
    await page.click('[data-theme="0"]');
    await page.click('[data-tab="clock"]');
    await page.setViewportSize({ width: 390, height: 844 });
    await page.waitForTimeout(300);
    if (process.env.CLOCKY_MOBILE_SCREENSHOT) await page.screenshot({ path: process.env.CLOCKY_MOBILE_SCREENSHOT, fullPage: true });
    for (const tab of ['clock', 'countdowns', 'settings']) {
      await page.click(`[data-tab="${tab}"]`);
      assert.equal(await page.evaluate(() => document.documentElement.scrollWidth <= innerWidth), true, `${tab} should fit mobile width`);
    }
    await page.emulateMedia({ reducedMotion: 'reduce' });
    await page.click('[data-tab="clock"]');
    assert.equal(await page.evaluate(() => document.getAnimations().filter(animation => animation.playState === 'running').length), 0);
    assert.deepEqual(errors, []);
    console.log('PASS: focus controls, world clocks, desk mode, layouts, themes, save failure, brightness zero, search, templates, mobile, reduced motion');
  } finally { await browser.close(); }
})().catch(error => { console.error(error); process.exitCode = 1; });
