// Run with Node and Playwright installed; no Clocky hardware or server needed.
const { chromium } = require('playwright');
const fs = require('node:fs');
const path = require('node:path');
const assert = require('node:assert/strict');
const root = path.resolve(__dirname, '..');
(async () => {
  const browser = await chromium.launch({ headless: true });
  try {
    const page = await browser.newPage();
    const errors = [];
    page.on('pageerror', error => errors.push(error.message));
    await page.route('http://clocky.test/**', route => {
      const name = new URL(route.request().url()).pathname;
      const fixtures = {
        '/api/settings': { timezone: 'America/Denver', brightness: 200 },
        '/api/status': { connected: true },
        '/api/countdowns': [],
        '/api/focus': { durationSeconds: 1500, remainingSeconds: 1500, running: false, completed: false, completedSessions: 0 },
        '/api/wifi/scan': [{ ssid: 'Home', rssi: -50, encryption: 'open' }]
      };
      if (name in fixtures) return route.fulfill({ json: fixtures[name] });
      const file = name === '/' ? 'index.html' : name.slice(1);
      return route.fulfill({ body: fs.readFileSync(path.join(root, 'data', file)),
        contentType: file.endsWith('.css') ? 'text/css' : file.endsWith('.js') ? 'text/javascript' : 'text/html' });
    });
    await page.goto('http://clocky.test/');
    await page.waitForFunction(() => window.app?.status);
    await page.click('[data-tab="countdowns"]');
    await page.click('#addCountdownBtn');
    assert.equal(await page.locator('#cdName').evaluate(el => el === document.activeElement), true);
    await page.keyboard.press('Shift+Tab');
    assert.equal(await page.locator('button[type="submit"]').evaluate(el => el === document.activeElement), true);
    await page.keyboard.press('Escape');
    assert.equal(await page.locator('#addCountdownBtn').evaluate(el => el === document.activeElement), true);
    await page.evaluate(() => {
      app.openCountdownModal(); app.closeCountdownModal(); app.openCountdownModal();
    });
    await page.waitForTimeout(300);
    assert.equal(await page.locator('#countdownModal').evaluate(el => getComputedStyle(el).opacity), '1');
    await page.keyboard.press('Escape');
    await page.evaluate(async () => {
      window.cd = id => ({ id, name: id, targetDate: 2000000000 + Number(id), accentColor: 65535, remaining: '3 days' });
      app.countdowns = [cd('1'), cd('2')];
      await app.renderCountdowns();
      window.row = app.countdownList.firstElementChild;
      row.querySelector('.edit-btn').focus();
      app.countdowns[0].name = 'Updated';
      await app.renderCountdowns();
    });
    assert.equal(await page.evaluate(() => row === app.countdownList.firstElementChild), true);
    assert.equal(await page.evaluate(() => document.activeElement === row.querySelector('.edit-btn')), true);
    await page.click('.edit-btn >> nth=0');
    assert.equal(await page.inputValue('#cdName'), 'Updated');
    await page.keyboard.press('Escape');
    // Overlapping removal and replacement must leave the newest result intact.
    await page.evaluate(async () => {
      app.countdowns = [];
      const removal = app.renderCountdowns();
      app.countdowns = [cd('2'), cd('3')];
      await app.renderCountdowns();
      await removal;
    });
    assert.deepEqual(await page.locator('.countdown-item').evaluateAll(rows => rows.map(r => r.dataset.id)), ['2', '3']);
    await page.click('[data-tab="settings"]');
    await page.click('#scanWifiBtn');
    await page.waitForSelector('.wifi-network-item');
    await page.setViewportSize({ width: 390, height: 844 });
    assert.equal(await page.evaluate(() => document.documentElement.scrollWidth <= innerWidth), true);
    if (process.env.CLOCKY_SCREENSHOT) await page.screenshot({ path: process.env.CLOCKY_SCREENSHOT, fullPage: true });
    await page.emulateMedia({ reducedMotion: 'reduce' });
    await page.click('[data-tab="countdowns"]');
    await page.evaluate(async () => { app.countdowns = []; await app.renderCountdowns(); });
    assert.equal(await page.locator('.countdown-item').count(), 0);
    assert.equal(await page.evaluate(() => document.getAnimations().filter(a => a.playState === 'running').length), 0);
    await page.click('#addCountdownBtn');
    await page.keyboard.press('Escape');
    assert.equal(await page.locator('#countdownModal').evaluate(el => getComputedStyle(el).visibility), 'hidden');
    assert.deepEqual(errors, []);
    console.log('PASS: dialogs, focus, keyed updates, overlapping removal, Wi-Fi, mobile, and reduced motion');
  } finally {
    await browser.close();
  }
})().catch(error => { console.error(error); process.exitCode = 1; });
