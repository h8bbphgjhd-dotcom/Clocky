// Run with Node and Playwright; uses mocked device APIs.
const { chromium } = require('playwright');
const fs = require('node:fs');
const path = require('node:path');
const assert = require('node:assert/strict');
const root = path.resolve(__dirname, '..');
(async () => {
  const browser = await chromium.launch({ headless: true });
  try {
    const page = await browser.newPage();
    const zones = ['UTC', 'America/Denver', 'America/Phoenix', 'Asia/Kolkata'];
    let settings = { timezone: 'America/Denver', brightness: 200, layoutId: 0, themeId: 0, use24Hour: false };
    let socket;
    let fail = false;
    const errors = [];
    page.on('pageerror', error => errors.push(error.message));
    await page.routeWebSocket('**/ws', ws => { socket = ws; });
    await page.route('http://clocky.test/**', route => {
      const name = new URL(route.request().url()).pathname;
      if (name === '/api/timezones') return route.fulfill({ json: zones });
      if (name === '/api/settings') {
        if (route.request().method() === 'PUT') {
          if (fail) return route.fulfill({ status: 400, json: { error: 'Failed' } });
          settings = { ...settings, ...route.request().postDataJSON() };
        }
        return route.fulfill({ json: settings });
      }
      if (name === '/api/status') return route.fulfill({ json: { timezone: settings.timezone, epoch: 1784116800 } });
      if (name === '/api/countdowns') return route.fulfill({ json: [] });
      if (name === '/api/focus') return route.fulfill({ json: { durationSeconds: 1500, remainingSeconds: 1500 } });
      const file = name === '/' ? 'index.html' : name.slice(1);
      return route.fulfill({ body: fs.readFileSync(path.join(root, 'data', file)),
        contentType: file.endsWith('.css') ? 'text/css' : file.endsWith('.js') ? 'text/javascript' : 'text/html' });
    });
    await page.goto('http://clocky.test/');
    await page.waitForFunction(() => app.status);
    await page.click('[data-tab="settings"]');
    assert.deepEqual(await page.locator('#timezoneSelect option').evaluateAll(options => options.map(option => option.value)), zones);
    assert.equal(await page.inputValue('#timezoneSelect'), 'America/Denver');
    await page.selectOption('#timezoneSelect', 'Asia/Kolkata');
    await page.waitForFunction(() => app.settings.timezone === 'Asia/Kolkata' && !app.pendingSettings);
    assert.equal(settings.timezone, 'Asia/Kolkata');
    assert.equal(await page.textContent('#previewZone'), 'Kolkata');
    await page.reload();
    await page.waitForFunction(() => app.status);
    assert.equal(await page.inputValue('#timezoneSelect'), 'Asia/Kolkata');
    // A touchscreen change propagates without reloading the browser.
    settings.timezone = 'America/Phoenix';
    socket.send(JSON.stringify({ type: 'time', timezone: settings.timezone, time: '5:00:00 AM', date: 'July 15, 2026', weekday: 'Wednesday', epoch: 1784116800 }));
    await page.waitForFunction(() => app.settings.timezone === 'America/Phoenix');
    assert.equal(await page.inputValue('#timezoneSelect'), 'America/Phoenix');
    assert.equal(await page.textContent('#previewZone'), 'Phoenix');
    // Failed writes restore the saved selection.
    await page.click('[data-tab="settings"]');
    fail = true;
    await page.selectOption('#timezoneSelect', 'UTC');
    await page.waitForFunction(() => !app.pendingSettings && document.getElementById('saveStatus').textContent.includes('Not saved'));
    assert.equal(await page.inputValue('#timezoneSelect'), 'America/Phoenix');
    assert.equal(settings.timezone, 'America/Phoenix');
    assert.deepEqual(errors, []);
    console.log('Timezone UI tests passed');
  } finally { await browser.close(); }
})().catch(error => { console.error(error); process.exitCode = 1; });
