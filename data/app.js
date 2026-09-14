// CYD Clock Web Interface
class CYDClockApp {
  constructor() {
    this.ws = null;
    this.motionPreference = window.matchMedia('(prefers-reduced-motion: reduce)');
    this.motionAnimations = new Set();
    this.motionPreference.addEventListener('change', () => {
      if (this.motionPreference.matches) {
        this.motionAnimations.forEach(animation => animation.cancel());
      }
    });
    this.countdownRenderVersion = 0;
    this.countdownFilter = 'all';
    this.focusState = null;
    this.focusMinutes = 25;
    this.deviceEpoch = null;
    this.deviceEpochReceived = 0;
    this.worldZones = ['Europe/London', 'Asia/Tokyo'];
    try {
      const saved = JSON.parse(localStorage.getItem('clocky-world-clocks'));
      if (Array.isArray(saved)) this.worldZones = saved.filter(zone => this.validZone(zone)).slice(0, 4);
    } catch (_) {}
    this.themes = [
      ['Graphite', '#242824', '#edaa7f', '#f5f4e9', '#9ea69b', '#30352f'],
      ['Nord', '#2e3440', '#88c0d0', '#eceff4', '#aeb9cc', '#3b4252'],
      ['Dracula', '#282a36', '#bd93f9', '#f8f8f2', '#b1accb', '#353746'],
      ['Paper', '#f4f0e3', '#376b74', '#34494b', '#647c7b', '#e6e1d2'],
      ['Contrast', '#000000', '#ffff00', '#ffffff', '#cccccc', '#242424'],
      ['Ember', '#30231e', '#e9a56b', '#f8e9d9', '#bda692', '#413129']
    ];
    this.settings = null;
    this.countdowns = [];
    this.status = null;
    this.timezones = [
      'UTC', 'America/Los_Angeles', 'America/Denver', 'America/Chicago', 'America/New_York',
      'America/Anchorage', 'Pacific/Honolulu', 'America/Toronto', 'America/Vancouver',
      'Europe/London', 'Europe/Paris', 'Europe/Berlin', 'Europe/Rome', 'Europe/Madrid',
      'Europe/Amsterdam', 'Europe/Stockholm', 'Europe/Oslo', 'Europe/Copenhagen',
      'Europe/Helsinki', 'Europe/Warsaw', 'Europe/Budapest', 'Europe/Prague',
      'Europe/Vienna', 'Europe/Zurich', 'Europe/Athens', 'Europe/Istanbul',
      'Asia/Dubai', 'Asia/Kolkata', 'Asia/Bangkok', 'Asia/Singapore', 'Asia/Hong_Kong',
      'Asia/Shanghai', 'Asia/Tokyo', 'Asia/Seoul', 'Australia/Sydney', 'Australia/Melbourne',
      'Australia/Brisbane', 'Australia/Perth', 'Australia/Adelaide', 'Pacific/Auckland',
      'Pacific/Fiji', 'Pacific/Guam', 'America/Sao_Paulo', 'America/Argentina/Buenos_Aires',
      'America/Mexico_City', 'America/Lima', 'America/Bogota', 'America/Caracas',
      'Africa/Cairo', 'Africa/Johannesburg', 'Africa/Lagos', 'Africa/Nairobi'
    ];
    
    this.init();
  }
  
  async init() {
    this.bindElements();
    this.bindEvents();
    this.bindDashboard();
    await this.loadTimezones();
    await this.fetchSettings();
    await this.fetchCountdowns();
    await this.fetchStatus();
    this.connectWebSocket();
    this.startStatusPolling();
    this.fetchFocus();
    this.refreshDashboard();
    setInterval(() => this.refreshDashboard(), 1000);
    setInterval(() => { if (!this.focusBusy) this.fetchFocus(); }, 5000);
  }
  
  bindElements() {
    // Tabs
    this.tabs = document.querySelectorAll('.tab');
    this.tabPanels = document.querySelectorAll('.tab-panel');
    
    // Clock preview
    this.previewTime = document.getElementById('previewTime');
    this.previewDate = document.getElementById('previewDate');
    this.previewWeekday = document.getElementById('previewWeekday');
    this.previewCountdown = document.getElementById('previewCountdown');
    
    // Countdowns
    this.countdownList = document.getElementById('countdownList');
    this.addCountdownBtn = document.getElementById('addCountdownBtn');
    
    // Settings
    this.timezoneSelect = document.getElementById('timezoneSelect');
    this.timeFormatSelect = document.getElementById('timeFormatSelect');
    this.layoutSelect = document.getElementById('layoutSelect');
    this.themeSelect = document.getElementById('themeSelect');
    this.brightnessSlider = document.getElementById('brightnessSlider');
    this.brightnessValue = document.getElementById('brightnessValue');
    this.scanWifiBtn = document.getElementById('scanWifiBtn');
    this.forgetWifiBtn = document.getElementById('forgetWifiBtn');
    this.wifiNetworks = document.getElementById('wifiNetworks');
    this.rebootBtn = document.getElementById('rebootBtn');
    
    // Status
    this.connectionStatus = document.getElementById('connectionStatus');
    this.wifiStatus = document.getElementById('wifiStatus');
    this.wifiSSID = document.getElementById('wifiSSID');
    this.wifiIP = document.getElementById('wifiIP');
    this.wifiRSSI = document.getElementById('wifiRSSI');
    this.uptimeEl = document.getElementById('uptime');
    this.freeHeapEl = document.getElementById('freeHeap');
    this.fwVersionEl = document.getElementById('fwVersion');
    
    // Modal
    this.countdownModal = document.getElementById('countdownModal');
    this.countdownForm = document.getElementById('countdownForm');
    this.modalTitle = document.getElementById('modalTitle');
    this.editCountdownId = document.getElementById('editCountdownId');
    this.cdName = document.getElementById('cdName');
    this.cdDate = document.getElementById('cdDate');
    this.cdTime = document.getElementById('cdTime');
    this.cdColor = document.getElementById('cdColor');
    this.cdShowOnMain = document.getElementById('cdShowOnMain');
    this.cdHideWhenExpired = document.getElementById('cdHideWhenExpired');
    this.cancelCountdownBtn = document.getElementById('cancelCountdownBtn');
  }
  
  bindEvents() {
    // Tabs
    this.tabs.forEach(tab => {
      tab.addEventListener('click', () => this.switchTab(tab.dataset.tab));
    });
    
    // Countdowns
    this.addCountdownBtn.addEventListener('click', () => this.openCountdownModal());
    this.cancelCountdownBtn.addEventListener('click', () => this.closeCountdownModal());
    this.countdownForm.addEventListener('submit', (e) => this.saveCountdown(e));
    
    // Settings
    this.timeFormatSelect.addEventListener('change', () => this.saveSettings());
    this.layoutSelect.addEventListener('change', () => this.saveSettings());
    this.themeSelect.addEventListener('change', () => this.saveSettings());
    this.brightnessSlider.addEventListener('input', (e) => {
      this.brightnessValue.textContent = e.target.value;
    });
    this.brightnessSlider.addEventListener('change', () => this.saveSettings());
    this.timezoneSelect.addEventListener('change', () => this.saveSettings());
    
    this.scanWifiBtn.addEventListener('click', () => this.scanWifi());
    this.forgetWifiBtn.addEventListener('click', () => this.forgetWifi());
    this.rebootBtn.addEventListener('click', () => this.rebootDevice());
    
    document.addEventListener('keydown', (e) => {
      if (!this.countdownModal.classList.contains('active')) return;
      if (e.key === 'Escape') this.closeCountdownModal();
      if (e.key === 'Tab') {
        const controls = [...this.countdownModal.querySelectorAll('input:not([type="hidden"]), button, select')]
          .filter(control => !control.disabled);
        const first = controls[0];
        const last = controls[controls.length - 1];
        if (e.shiftKey && document.activeElement === first) {
          e.preventDefault();
          last.focus();
        } else if (!e.shiftKey && document.activeElement === last) {
          e.preventDefault();
          first.focus();
        }
      }
    });

    // Modal close on backdrop click
    this.countdownModal.addEventListener('click', (e) => {
      if (e.target === this.countdownModal) this.closeCountdownModal();
    });
  }
  
  async loadTimezones() {
    this.timezoneSelect.innerHTML = this.timezones.map(tz => 
      `<option value="${tz}">${tz}</option>`
    ).join('');
  }
  
  switchTab(tabName) {
    this.tabs.forEach(tab => {
      tab.classList.toggle('active', tab.dataset.tab === tabName);
      tab.setAttribute('aria-selected', String(tab.dataset.tab === tabName));
    });
    this.tabPanels.forEach(panel => {
      panel.classList.toggle('active', panel.id === `tab-${tabName}`);
    });
  }
  
  connectWebSocket() {
    const protocol = window.location.protocol === 'https:' ? 'wss:' : 'ws:';
    this.ws = new WebSocket(`${protocol}//${window.location.host}/ws`);
    
    this.ws.onopen = () => {
      this.updateConnectionStatus(true);
    };
    
    this.ws.onclose = () => {
      this.updateConnectionStatus(false);
      // Reconnect after 3 seconds
      setTimeout(() => this.connectWebSocket(), 3000);
    };
    
    this.ws.onerror = () => {
      this.updateConnectionStatus(false);
    };
    
    this.ws.onmessage = (event) => {
      try {
        const data = JSON.parse(event.data);
        if (data.type === 'time') {
          this.acceptEpoch(data.epoch);
          if (data.focus && !this.focusBusy) this.applyFocus(data.focus);
          this.updateClockPreview(data.time, data.date, data.weekday);
        } else if (data.type === 'countdowns') {
          this.countdowns = data.countdowns;
          this.renderCountdowns();
          this.updatePreviewCountdown();
        }
      } catch (e) {
        console.error('WS message error:', e);
      }
    };
  }
  
  updateConnectionStatus(connected) {
    this.connectionStatus.className = 'connection-status ' + (connected ? 'connected' : 'disconnected');
    this.connectionStatus.querySelector('span:last-child').textContent = connected ? 'Connected' : 'Disconnected';
    this.connectionStatus.querySelector('.status-dot').style.background = connected ? 'var(--success)' : 'var(--danger)';
  }
  
  async fetchSettings() {
    try {
      const resp = await fetch('/api/settings');
      if (!resp.ok) throw new Error('Device settings unavailable');
      this.settings = await resp.json();
      this.applySettingsToUI();
    } catch (e) {
      console.error('Failed to fetch settings:', e);
    }
  }
  
  async fetchCountdowns() {
    try {
      const resp = await fetch('/api/countdowns');
      this.countdowns = await resp.json();
      await this.renderCountdowns();
      this.updatePreviewCountdown();
    } catch (e) {
      console.error('Failed to fetch countdowns:', e);
    }
  }
  
  async fetchStatus() {
    try {
      const resp = await fetch('/api/status');
      if (!resp.ok) throw new Error('Device status unavailable');
      this.status = await resp.json();
      this.acceptEpoch(this.status.epoch);
      this.updateStatusUI();
    } catch (e) {
      console.error('Failed to fetch status:', e);
    }
  }
  
  startStatusPolling() {
    setInterval(() => this.fetchStatus(), 10000);
  }
  
  applySettingsToUI() {
    if (!this.settings) return;
    
    this.timezoneSelect.value = this.settings.timezone || 'America/Denver';
    this.timeFormatSelect.value = this.settings.use24Hour ? '24' : '12';
    this.layoutSelect.value = this.settings.layoutId || '0';
    this.themeSelect.value = this.settings.themeId || '0';
    this.brightnessSlider.value = this.settings.brightness ?? 200;
    this.brightnessValue.textContent = this.settings.brightness ?? 200;
    this.applyAppearance();
  }
  
  updateStatusUI() {
    if (!this.status) return;
    
    this.wifiStatus.textContent = this.status.connected ? 'Connected' : 'Disconnected';
    this.wifiStatus.style.color = this.status.connected ? 'var(--success)' : 'var(--danger)';
    this.wifiSSID.textContent = this.status.ssid || '--';
    this.wifiIP.textContent = this.status.ip || '--';
    this.wifiRSSI.textContent = this.status.rssi ? `${this.status.rssi} dBm` : '--';
    this.uptimeEl.textContent = this.formatUptime(this.status.uptime || 0);
    this.freeHeapEl.textContent = this.formatBytes(this.status.freeHeap || 0);
    this.fwVersionEl.textContent = this.status.firmwareVersion || '—';
    document.getElementById('timeSyncStatus').textContent = this.status.timeSynced ? 'Synchronized' : 'Waiting for Wi-Fi sync';
    document.getElementById('syncLabel').textContent = this.status.timeSynced ? 'Time synchronized' : 'Time not yet synchronized';
  }
  
  updateClockPreview(time, date, weekday) {
    // Keep the seconds steady; only reveal date changes.
    if (this.layoutSelect.value !== '3') {
      const parts = String(time).match(/^(.*?)(\s*[AP]M)$/i);
      this.previewTime.textContent = parts ? parts[1] : time;
      if (parts) {
        const suffix = document.createElement('small');
        suffix.textContent = parts[2].trim();
        this.previewTime.appendChild(suffix);
      }
    }
    this.updateAnimatedText(this.previewDate, date);
    this.updateAnimatedText(this.previewWeekday, weekday);
  }
  
  updatePreviewCountdown() {
    const next = this.countdowns
      .filter(cd => !cd.expired && cd.showOnMain !== false)
      .sort((a, b) => a.targetDate - b.targetDate)[0];
    
    const nextId = next?.id || '';
    const changed = this.previewCountdown.dataset.countdownId !== nextId;
    this.previewCountdown.dataset.countdownId = nextId;
    if (next) {
      this.previewCountdown.classList.remove('empty');
      this.previewCountdown.innerHTML = `
        <div style="color: #${next.accentColor.toString(16).padStart(6, '0')}; font-weight: 600;">${this.escapeHtml(next.name)}</div>
        <div class="countdown-remaining">${next.remaining}</div>
      `;
    } else {
      this.previewCountdown.classList.add('empty');
      this.previewCountdown.textContent = 'No active countdowns';
    }
    if (changed) this.reveal(this.previewCountdown);
  }
  
  async renderCountdowns() {
    const version = ++this.countdownRenderVersion;
    const search = document.getElementById('countdownSearch').value.trim().toLowerCase();
    const visible = this.countdowns.filter(cd => (!search || cd.name.toLowerCase().includes(search)) &&
      (this.countdownFilter === 'all' || (this.countdownFilter === 'past' ? cd.expired : !cd.expired)));
    document.getElementById('eventCount').textContent = `${this.countdowns.length} / 10 events`;
    this.addCountdownBtn.disabled = this.countdowns.length >= 10;
    const existing = new Map([...this.countdownList.querySelectorAll('.countdown-item')]
      .map(item => [item.dataset.id, item]));
    const previousPositions = new Map([...existing].map(([id, item]) => [id, item.getBoundingClientRect().top]));
    const focusedControl = this.countdownList.contains(document.activeElement) ? document.activeElement : null;
    const ids = new Set(visible.map(cd => String(cd.id)));
    await Promise.all([...existing].filter(([id]) => !ids.has(id)).map(async ([id, item]) => {
      await this.animate(item, [{ opacity: 1 }, { opacity: 0, transform: 'translateX(12px)' }], { duration: 150 });
      if (version === this.countdownRenderVersion) {
        item.remove();
        existing.delete(id);
      }
    }));
    if (version !== this.countdownRenderVersion) return;
    this.countdownList.querySelector('.empty-state')?.remove();
    
    if (visible.length === 0) {
      this.countdownList.innerHTML = `
        <div class="empty-state"><strong>${this.countdowns.length ? 'Nothing here just yet.' : 'Make room for a good thing.'}</strong><p>${this.countdowns.length ? 'Try another search or filter.' : 'Add a milestone, a getaway, or a little moment worth counting down to.'}</p></div>
      `;
      this.reveal(this.countdownList.firstElementChild);
      return;
    }
    
    // Sort: active first by date, then expired by priority
    const sorted = [...visible].sort((a, b) => {
      if (a.expired && !b.expired) return 1;
      if (!a.expired && b.expired) return -1;
      if (a.expired && b.expired) return a.priority - b.priority;
      return a.targetDate - b.targetDate;
    });
    
    sorted.forEach((cd, index) => {
      const item = existing.get(String(cd.id)) || document.createElement('div');
      const isNew = !existing.has(String(cd.id));
      item.dataset.id = String(cd.id);
      item.countdown = cd;
      item.className = `countdown-item ${cd.expired ? 'expired' : ''}`;
      const colorHex = '#' + cd.accentColor.toString(16).padStart(6, '0');
      if (isNew) item.innerHTML = `
        <div class="countdown-color" style="background: ${colorHex}"></div>
        <div class="countdown-info">
          <div class="countdown-name">${this.escapeHtml(cd.name)}</div>
          <div class="countdown-remaining">${cd.remaining}</div>
        </div>
        <div class="countdown-actions">
          <button class="icon-btn edit-btn" data-id="${cd.id}" title="Edit">
            <svg width="18" height="18" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2">
              <path d="M11 4H4a2 2 0 0 0-2 2v14a2 2 0 0 0 2 2h14a2 2 0 0 0 2-2v-7"/>
              <path d="M18.5 2.5a2.121 2.121 0 0 1 3 3L12 15l-4 1 1-4 9.5-9.5z"/>
            </svg>
          </button>
          <button class="icon-btn delete-btn" data-id="${cd.id}" title="Delete">
            <svg width="18" height="18" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2">
              <polyline points="3 6 5 6 21 6"/>
              <path d="M19 6v14a2 2 0 0 1-2 2H7a2 2 0 0 1-2-2V6m3 0V4a2 2 0 0 1 2-2h4a2 2 0 0 1 2 2v2"/>
            </svg>
          </button>
        </div>
      `;
      
      if (isNew) {
        item.querySelector('.edit-btn').addEventListener('click', () => this.openCountdownModal(item.countdown));
        item.querySelector('.delete-btn').addEventListener('click', () => this.deleteCountdown(item.countdown.id));
      } else {
        item.querySelector('.countdown-color').style.background = colorHex;
        item.querySelector('.countdown-name').textContent = cd.name;
        item.querySelector('.countdown-remaining').textContent = cd.remaining;
      }
      if (this.countdownList.children[index] !== item) {
        this.countdownList.insertBefore(item, this.countdownList.children[index] || null);
      }
    });
    if (focusedControl?.isConnected && document.activeElement === document.body) {
      focusedControl.focus({ preventScroll: true });
    }
    sorted.forEach((cd, index) => {
      const item = this.countdownList.children[index];
      if (!existing.has(String(cd.id))) {
        this.reveal(item, Math.min(index, 5) * 35);
      } else {
        const offset = previousPositions.get(String(cd.id)) - item.getBoundingClientRect().top;
        if (offset) this.animate(item, [
          { transform: `translateY(${offset}px)` }, { transform: 'translateY(0)' }
        ]);
      }
    });
  }
  
  openCountdownModal(cd = null) {
    this.countdownForm.reset();
    this.editCountdownId.value = cd?.id || '';
    this.modalTitle.textContent = cd ? 'Edit your moment' : 'New countdown';
    
    if (cd) {
      this.cdName.value = cd.name;
      this.cdDate.value = this.formatDateForInput(cd.targetDate);
      this.cdTime.value = cd.hasTime ? this.formatTimeForInput(cd.targetDate) : '';
      this.cdColor.value = '#' + cd.accentColor.toString(16).padStart(6, '0');
      this.cdShowOnMain.checked = cd.showOnMain;
      this.cdHideWhenExpired.checked = cd.hideWhenExpired;
    } else {
      // Default to tomorrow
      const tomorrow = new Date();
      tomorrow.setDate(tomorrow.getDate() + 1);
      this.cdDate.value = this.formatDateForInput(tomorrow.getTime() / 1000);
      this.cdColor.value = '#e87545';
      this.cdShowOnMain.checked = true;
      this.cdHideWhenExpired.checked = false;
    }
    
    this.modalReturnFocus = document.activeElement;
    this.countdownModal.inert = false;
    this.countdownModal.setAttribute('aria-hidden', 'false');
    document.querySelector('.main').inert = true;
    document.querySelector('.header').inert = true;
    document.body.style.overflow = 'hidden';
    this.countdownModal.classList.add('active');
    this.cdName.focus();
  }
  
  closeCountdownModal() {
    this.countdownModal.classList.remove('active');
    this.countdownModal.inert = true;
    this.countdownModal.setAttribute('aria-hidden', 'true');
    document.querySelector('.main').inert = false;
    document.querySelector('.header').inert = false;
    document.body.style.overflow = '';
    const returnFocus = this.modalReturnFocus?.isConnected ? this.modalReturnFocus : this.addCountdownBtn;
    returnFocus.focus({ preventScroll: true });
  }
  
  async saveCountdown(e) {
    e.preventDefault();
    
    const submitButton = this.countdownForm.querySelector('[type="submit"]');
    if (submitButton.disabled) return;
    submitButton.disabled = true;
    submitButton.setAttribute('aria-busy', 'true');
    const cd = {
      name: this.cdName.value,
      targetDate: this.combineDateTime(this.cdDate.value, this.cdTime.value),
      hasTime: !!this.cdTime.value,
      accentColor: parseInt(this.cdColor.value.slice(1), 16),
      showOnMain: this.cdShowOnMain.checked,
      hideWhenExpired: this.cdHideWhenExpired.checked
    };
    
    const id = this.editCountdownId.value;
    const url = id ? `/api/countdowns/${id}` : '/api/countdowns';
    const method = id ? 'PUT' : 'POST';
    
    try {
      const resp = await fetch(url, {
        method,
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify(cd)
      });
      
      if (resp.ok) {
        this.closeCountdownModal();
        await this.fetchCountdowns();
        this.toast('A moment to look forward to. Countdown saved.');
      } else {
        const err = await resp.json();
        this.toast(err.error || 'Could not save this countdown', true);
      }
    } catch (e) {
      this.toast(e.message, true);
    } finally {
      submitButton.disabled = false;
      submitButton.removeAttribute('aria-busy');
    }
  }
  
  async deleteCountdown(id) {
    if (!await this.ask('Remove this countdown?', 'This event will be removed from your Clocky.')) return;
    
    try {
      const resp = await fetch(`/api/countdowns/${id}`, { method: 'DELETE' });
      if (resp.ok) {
        await this.fetchCountdowns();
      } else {
        this.toast('Could not remove this countdown', true);
      }
    } catch (e) {
      this.toast(e.message, true);
    }
  }
  
  async saveSettings() {
    // Serialize writes so a slower response cannot overwrite a newer selection.
    const settings = {
      timezone: this.timezoneSelect.value,
      use24Hour: this.timeFormatSelect.value === '24',
      layoutId: Number(this.layoutSelect.value),
      themeId: Number(this.themeSelect.value),
      brightness: Number(this.brightnessSlider.value)
    };
    const status = document.getElementById('saveStatus');
    status.textContent = 'Saving…';
    this.applyAppearance();
    this.settingsQueue = (this.settingsQueue || Promise.resolve()).then(async () => {
      const resp = await fetch('/api/settings', {
        method: 'PUT', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify(settings)
      });
      if (!resp.ok) throw new Error('Could not save settings. Please try again.');
      this.settings = { ...this.settings, ...settings };
      status.textContent = 'All changes saved';
    }).catch(error => {
      status.textContent = 'Not saved · try again';
      this.toast(error.message, true);
    });
    await this.settingsQueue;
  }

  async scanWifi() {
    this.scanWifiBtn.disabled = true;
    this.scanWifiBtn.setAttribute('aria-busy', 'true');
    this.scanWifiBtn.textContent = 'Scanning...';
    this.wifiNetworks.style.display = 'none';
    this.wifiNetworks.innerHTML = '';
    
    try {
      const resp = await fetch('/api/wifi/scan', { method: 'POST' });
      const networks = await resp.json();
      
      if (networks.length === 0) {
        this.wifiNetworks.innerHTML = '<div style="padding:1rem;color:var(--text-muted);">No networks found</div>';
      } else {
        networks.forEach(net => {
          const item = document.createElement('div');
          item.className = 'wifi-network-item';
          item.innerHTML = `
            <div class="wifi-network-info">
              <span class="wifi-network-ssid">${this.escapeHtml(net.ssid)}</span>
              <span class="wifi-network-meta">${net.rssi} dBm · ${net.encryption}</span>
            </div>
            <button class="btn btn-primary connect-btn" data-ssid="${this.escapeHtml(net.ssid)}" ${net.encryption === 'secured' ? '' : 'data-open'}>Connect</button>
          `;
          item.querySelector('.connect-btn').addEventListener('click', () => this.connectWifi(net.ssid, net.encryption === 'open'));
          this.wifiNetworks.appendChild(item);
        });
      }
      this.wifiNetworks.style.display = 'flex';
      [...this.wifiNetworks.children].forEach((item, index) => this.reveal(item, Math.min(index, 5) * 35));
    } catch (e) {
      this.toast('Scan failed: ' + e.message, true);
    } finally {
      this.scanWifiBtn.disabled = false;
      this.scanWifiBtn.removeAttribute('aria-busy');
      this.scanWifiBtn.textContent = 'Scan Networks';
    }
  }
  
  async connectWifi(ssid, isOpen) {
    let password = '';
    if (!isOpen) {
      password = await this.ask('Join network', `Connect your Clocky to ${ssid}.`, { input: 'password', label: 'Wi-Fi password' });
      if (password === null) return;
    }
    
    try {
      const resp = await fetch('/api/wifi/connect', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ ssid, password })
      });
      
      const result = await resp.json();
      if (result.success) {
        this.toast('Connected. Device address: ' + result.ip);
        this.wifiNetworks.style.display = 'none';
        await this.fetchStatus();
      } else {
        this.toast(result.error || 'Could not connect', true);
      }
    } catch (e) {
      this.toast(e.message, true);
    }
  }
  
  async forgetWifi() {
    if (!await this.ask('Forget this network?', 'Clocky will open its Wi-Fi setup hotspot so you can connect it again.')) return;
    
    try {
      await fetch('/api/wifi/disconnect', { method: 'POST' });
      this.toast('Network forgotten. Connect to the Clocky setup hotspot.');
      await this.fetchStatus();
    } catch (e) {
      this.toast(e.message, true);
    }
  }
  
  async rebootDevice() {
    if (!await this.ask('Restart your Clocky?', 'Saved settings and countdowns will stay. The focus timer will reset.')) return;
    
    try {
      await fetch('/api/reboot', { method: 'POST' });
      this.toast('Clocky is restarting. It will reconnect shortly.');
      this.updateConnectionStatus(false);
    } catch (e) {
      this.toast(e.message, true);
    }
  }
  
  bindDashboard() {
    const on = (id, fn) => document.getElementById(id).addEventListener('click', fn);
    on('customizeBtn', () => this.switchTab('settings'));
    on('viewCountdownsBtn', () => this.switchTab('countdowns'));
    on('quickAddBtn', () => this.openCountdownModal());
    document.querySelector('.brand').addEventListener('click', event => { event.preventDefault(); this.switchTab('clock'); });
    on('deskModeBtn', () => { document.body.classList.add('desk-mode'); document.getElementById('exitDeskModeBtn').focus(); });
    const exitDesk = () => { document.body.classList.remove('desk-mode'); document.getElementById('deskModeBtn').focus(); };
    on('exitDeskModeBtn', exitDesk);
    document.addEventListener('keydown', event => {
      if (event.key === 'Escape' && document.body.classList.contains('desk-mode')) exitDesk();
    });
    this.tabs.forEach((tab, index) => tab.addEventListener('keydown', event => {
      if (!['ArrowLeft', 'ArrowRight', 'Home', 'End'].includes(event.key)) return;
      event.preventDefault();
      let target = event.key === 'Home' ? 0 : event.key === 'End' ? this.tabs.length - 1 :
        (index + (event.key === 'ArrowRight' ? 1 : -1) + this.tabs.length) % this.tabs.length;
      this.tabs[target].focus(); this.switchTab(this.tabs[target].dataset.tab);
    }));
    document.querySelectorAll('button[data-layout]').forEach(button => button.addEventListener('click', () => {
      this.layoutSelect.value = button.dataset.layout; this.saveSettings();
    }));
    document.getElementById('themePicker').innerHTML = this.themes.map((theme, index) =>
      `<button class="theme-choice" data-theme="${index}" aria-label="${theme[0]} theme"><span class="theme-swatch" style="background:${theme[1]};color:${theme[2]}">Aa</span>${theme[0]}</button>`).join('');
    document.querySelectorAll('[data-theme]').forEach(button => button.addEventListener('click', () => {
      this.themeSelect.value = button.dataset.theme; this.saveSettings();
    }));
    document.querySelectorAll('[data-template]').forEach(button => button.addEventListener('click', () => {
      this.openCountdownModal(); this.cdName.value = button.dataset.template;
    }));
    document.getElementById('countdownSearch').addEventListener('input', () => this.renderCountdowns());
    document.querySelectorAll('[data-filter]').forEach(button => button.addEventListener('click', () => {
      this.countdownFilter = button.dataset.filter;
      document.querySelectorAll('[data-filter]').forEach(item => {
        item.classList.toggle('selected', item === button); item.setAttribute('aria-pressed', String(item === button));
      });
      this.renderCountdowns();
    }));
    document.querySelectorAll('[data-minutes]').forEach(button => button.addEventListener('click', () => {
      this.focusMinutes = Number(button.dataset.minutes);
      document.querySelectorAll('[data-minutes]').forEach(item => {
        item.classList.toggle('selected', item === button); item.setAttribute('aria-pressed', String(item === button));
      });
      if (!this.focusState?.running) document.getElementById('focusTime').textContent = `${this.focusMinutes}:00`;
      this.focusPresetChanged = true;
      document.getElementById('focusToggleBtn').textContent = 'Start new session →';
    }));
    on('focusToggleBtn', async () => {
      const state = this.focusState;
      if (!state) return;
      if (this.focusPresetChanged) {
        if ((state.running || state.remainingSeconds < state.durationSeconds) && !state.completed &&
            !await this.ask('Start a fresh session?', 'This will replace the current focus timer on your Clocky.')) return;
        this.focusCommand('start');
      } else this.focusCommand(state.running ? 'pause' : state.completed || state.remainingSeconds === state.durationSeconds ? 'start' : 'resume');
    });
    on('focusResetBtn', async () => {
      if (await this.ask('Reset this session?', 'The timer will return to its starting duration.')) this.focusCommand('reset');
    });
    on('addWorldBtn', async () => {
      if (this.worldZones.length >= 4) { this.toast('You can keep up to four world clocks. Remove one to add another.'); return; }
      const zone = await this.ask('A little closer.', 'Choose a timezone for your world clock. Saved in this browser.', { zones: this.timezones.filter(zone => !this.worldZones.includes(zone)) });
      if (!zone) return;
      this.worldZones.push(zone); this.saveWorldClocks(); this.renderWorldClocks();
    });
    this.renderWorldClocks();
  }

  validZone(zone) {
    try { new Intl.DateTimeFormat('en', { timeZone: zone }).format(); return typeof zone === 'string'; } catch (_) { return false; }
  }

  saveWorldClocks() {
    try { localStorage.setItem('clocky-world-clocks', JSON.stringify(this.worldZones)); }
    catch (_) { this.toast('World clocks will last for this visit; browser storage is unavailable.'); }
  }

  renderWorldClocks() {
    const container = document.getElementById('worldClocks');
    container.replaceChildren();
    this.worldZones.forEach(zone => {
      const row = document.createElement('div'); row.className = 'world-row'; row.dataset.zone = zone;
      const city = zone.split('/').pop().replaceAll('_', ' ');
      row.innerHTML = `<div class="world-city"><strong>${this.escapeHtml(city)}</strong><p></p></div><span class="world-time"></span><button class="icon-btn" aria-label="Remove ${this.escapeHtml(city)} world clock">×</button>`;
      row.querySelector('button').addEventListener('click', () => {
        this.worldZones = this.worldZones.filter(value => value !== zone);
        this.saveWorldClocks(); this.renderWorldClocks(); document.getElementById('addWorldBtn').focus();
      });
      container.appendChild(row);
    });
    if (!this.worldZones.length) container.innerHTML = '<p class="muted">Add a city to keep another corner of the world close.</p>';
    this.updateWorldClocks();
  }

  updateWorldClocks() {
    document.querySelectorAll('.world-row').forEach(row => {
      const date = new Date();
      row.querySelector('.world-time').textContent = date.toLocaleTimeString([], { timeZone: row.dataset.zone, hour: '2-digit', minute: '2-digit', hour12: false });
      row.querySelector('p').textContent = date.toLocaleDateString([], { timeZone: row.dataset.zone, weekday: 'short', month: 'short', day: 'numeric' });
    });
  }

  applyAppearance() {
    const theme = this.themes[Number(this.themeSelect.value)] || this.themes[0];
    const preview = document.getElementById('clockPreview');
    ['bg', 'accent', 'text', 'muted', 'panel'].forEach((key, index) => preview.style.setProperty(`--clock-${key}`, theme[index + 1]));
    preview.dataset.layout = this.layoutSelect.value;
    document.getElementById('previewZone').textContent = this.timezoneSelect.value.split('/').pop().replaceAll('_', ' ');
    document.querySelectorAll('[data-layout]').forEach(button => {
      if (button.tagName !== 'BUTTON') return;
      button.classList.toggle('selected', button.dataset.layout === this.layoutSelect.value);
      button.setAttribute('aria-pressed', String(button.dataset.layout === this.layoutSelect.value));
    });
    document.querySelectorAll('[data-theme]').forEach(button => {
      button.classList.toggle('selected', button.dataset.theme === this.themeSelect.value);
      button.setAttribute('aria-pressed', String(button.dataset.theme === this.themeSelect.value));
    });
    this.refreshDashboard();
  }

  acceptEpoch(epoch) {
    if (!Number.isFinite(epoch) || epoch < 1000000000) return;
    this.deviceEpoch = epoch; this.deviceEpochReceived = performance.now();
  }

  clockNow() {
    return this.deviceEpoch === null ? Date.now() : this.deviceEpoch * 1000 + performance.now() - this.deviceEpochReceived;
  }

  refreshDashboard() {
    this.updateWorldClocks();
    const now = this.clockNow();
    let hour = new Date(now).getHours(), minute = new Date(now).getMinutes(), second = new Date(now).getSeconds();
    const zone = this.settings?.timezone;
    if (this.validZone(zone)) {
      const parts = new Intl.DateTimeFormat('en-GB', { timeZone: zone, hour: '2-digit', minute: '2-digit', second: '2-digit', hourCycle: 'h23' }).formatToParts(now);
      const values = Object.fromEntries(parts.map(part => [part.type, part.value]));
      hour = Number(values.hour); minute = Number(values.minute); second = Number(values.second);
    }
    document.getElementById('greeting').textContent = hour < 12 ? 'Make room for a good day.' : hour < 18 ? 'A little more present.' : 'Let the day slow down.';
    const dayPercent = ((hour * 3600 + minute * 60 + second) / 86400) * 100;
    document.getElementById('dayProgress').style.width = `${dayPercent}%`;
    document.getElementById('dayCaption').textContent = `${Math.floor(dayPercent)}% of today · make it count`;
    document.querySelector('.hour-hand').style.transform = `rotate(${(hour % 12 + minute / 60) * 30}deg)`;
    document.querySelector('.minute-hand').style.transform = `rotate(${(minute + second / 60) * 6}deg)`;
    document.querySelector('.second-hand').style.transform = `rotate(${second * 6}deg)`;
    let expiredChanged = false;
    this.countdowns.forEach(cd => {
      const expired = cd.targetDate * 1000 <= now;
      if (cd.expired !== expired) expiredChanged = true;
      cd.expired = expired;
      cd.remaining = this.remainingLabel(cd.targetDate, now);
    });
    document.querySelectorAll('.countdown-item').forEach(row => {
      const cd = this.countdowns.find(item => String(item.id) === row.dataset.id);
      if (cd) row.querySelector('.countdown-remaining').textContent = cd.remaining;
    });
    if (expiredChanged) this.renderCountdowns();
    this.updatePreviewCountdown();
    const upcoming = this.countdowns.filter(cd => !cd.expired).sort((a, b) => a.targetDate - b.targetDate).slice(0, 2);
    const html = upcoming.length ? upcoming.map(cd => {
      const date = new Date(cd.targetDate * 1000);
      return `<div class="upcoming-row"><span class="event-date">${date.toLocaleDateString('en', { month: 'short' }).toUpperCase()}<strong>${date.getDate()}</strong></span><div class="event-title"><strong>${this.escapeHtml(cd.name)}</strong><p>${date.toLocaleDateString([], { weekday: 'long', month: 'long', day: 'numeric' })}</p></div><span class="event-distance">${this.remainingLabel(cd.targetDate, now)}</span></div>`;
    }).join('') : '<div class="empty-state"><strong>Your next good thing.</strong><p>Add an event and watch it get a little closer.</p></div>';
    const upcomingEl = document.getElementById('upcomingEvents');
    if (upcomingEl.innerHTML !== html) upcomingEl.innerHTML = html;
    if (this.layoutSelect.value === '3') {
      this.previewTime.textContent = this.focusState ? this.formatTimer(this.focusState.remainingSeconds) : '25:00';
      this.previewWeekday.textContent = 'One thing at a time.';
      this.previewDate.textContent = this.focusState?.running ? 'Focus session in progress' : this.focusState?.completed ? 'Session complete. Take a breath.' : 'Your focus timer';
    }
  }

  remainingLabel(target, now = this.clockNow()) {
    const seconds = Math.max(0, Math.ceil((target * 1000 - now) / 1000));
    if (!seconds) return 'Past event';
    const days = Math.floor(seconds / 86400);
    if (days) return `${days}d ${Math.floor(seconds % 86400 / 3600)}h away`;
    const hours = Math.floor(seconds / 3600);
    return hours ? `${hours}h ${Math.floor(seconds % 3600 / 60)}m away` : `${Math.floor(seconds / 60)}m ${seconds % 60}s away`;
  }

  formatTimer(seconds) { return `${String(Math.floor(seconds / 60)).padStart(2, '0')}:${String(seconds % 60).padStart(2, '0')}`; }

  async fetchFocus() {
    if (this.focusFetching) return;
    this.focusFetching = true;
    try {
      const resp = await fetch('/api/focus');
      if (!resp.ok) throw new Error('Timer unavailable');
      const state = await resp.json();
      if (!this.focusBusy) this.applyFocus(state);
    } catch (_) {
      document.getElementById('focusState').textContent = 'OFFLINE';
      document.getElementById('focusToggleBtn').disabled = true;
      document.getElementById('focusResetBtn').disabled = true;
    } finally { this.focusFetching = false; }
  }

  applyFocus(state) {
    if (!Number.isFinite(state.remainingSeconds) || !Number.isFinite(state.durationSeconds) || state.durationSeconds <= 0) return;
    if (state.completed && !this.focusState?.completed) this.toast('Session complete. Take a breath — you earned it.');
    this.focusState = state;
    const label = state.running ? 'In the zone' : state.completed ? 'Nicely done.' : state.remainingSeconds < state.durationSeconds ? 'Take your time' : 'A fresh start';
    document.getElementById('focusTime').textContent = this.focusPresetChanged && !state.running ? `${this.focusMinutes}:00` : this.formatTimer(state.remainingSeconds);
    document.getElementById('focusLabel').textContent = label;
    document.getElementById('focusState').textContent = state.running ? 'FOCUSING' : state.completed ? 'COMPLETE' : state.remainingSeconds < state.durationSeconds ? 'PAUSED' : 'READY';
    document.getElementById('focusDial').style.setProperty('--progress', `${Math.max(0, Math.min(100, (1 - state.remainingSeconds / state.durationSeconds) * 100))}%`);
    const toggle = document.getElementById('focusToggleBtn');
    toggle.textContent = this.focusPresetChanged ? 'Start new session →' : state.running ? 'Pause session Ⅱ' : state.completed ? 'Go again →' : state.remainingSeconds < state.durationSeconds ? 'Resume session →' : 'Start session →';
    toggle.disabled = !!this.focusBusy;
    document.getElementById('focusResetBtn').disabled = !!this.focusBusy;
    document.getElementById('focusSessions').textContent = `${state.completedSessions} completed since restart · synced with Clocky`;
  }

  async focusCommand(action) {
    if (this.focusBusy) return;
    this.focusBusy = true;
    const toggle = document.getElementById('focusToggleBtn');
    toggle.disabled = true; toggle.setAttribute('aria-busy', 'true');
    document.getElementById('focusResetBtn').disabled = true;
    try {
      const resp = await fetch('/api/focus', { method: 'POST', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify({ action, durationSeconds: this.focusPresetChanged ? this.focusMinutes * 60 : this.focusState.durationSeconds }) });
      if (!resp.ok) throw new Error('Could not update the timer. Please try again.');
      const state = await resp.json();
      this.focusPresetChanged = false;
      this.focusBusy = false;
      this.applyFocus(state);
    } catch (error) { this.toast(error.message, true); }
    finally {
      this.focusBusy = false; toggle.removeAttribute('aria-busy');
      toggle.disabled = false; document.getElementById('focusResetBtn').disabled = false;
    }
  }

  toast(message, error = false) {
    const toast = document.getElementById('toast');
    clearTimeout(this.toastTimer);
    toast.textContent = message; toast.classList.toggle('error', error); toast.classList.add('visible');
    this.toastTimer = setTimeout(() => toast.classList.remove('visible'), error ? 6500 : 4000);
  }

  ask(title, message, options = {}) {
    const dialog = document.getElementById('actionDialog');
    if (dialog.open) return Promise.resolve(null);
    document.getElementById('actionTitle').textContent = title;
    document.getElementById('actionMessage').textContent = message;
    const input = document.getElementById('actionInput'); input.value = ''; input.type = options.input || 'text';
    document.getElementById('actionInputLabel').hidden = !options.input;
    document.getElementById('actionFieldLabel').textContent = options.label || '';
    document.getElementById('actionSelectLabel').hidden = !options.zones;
    const select = document.getElementById('actionSelect');
    select.replaceChildren();
    (options.zones || []).forEach(zone => select.add(new Option(zone.replaceAll('_', ' '), zone)));
    dialog.returnValue = 'cancel'; dialog.showModal();
    if (options.input) input.focus();
    return new Promise(resolve => dialog.addEventListener('close', () => {
      const result = dialog.returnValue === 'ok' ? options.input ? input.value : options.zones ? select.value : true : null;
      input.value = ''; resolve(result);
    }, { once: true }));
  }

  // Animation is optional; state updates never depend on browser motion support.
  animate(element, keyframes, options = {}) {
    if (this.motionPreference.matches || !element.animate || !element.getClientRects().length) {
      return Promise.resolve();
    }
    element.getAnimations().forEach(animation => animation.cancel());
    const animation = element.animate(keyframes, {
      duration: 240, easing: 'cubic-bezier(0.22, 1, 0.36, 1)', ...options
    });
    this.motionAnimations.add(animation);
    return animation.finished.catch(() => {}).finally(() => this.motionAnimations.delete(animation));
  }

  reveal(element, delay = 0) {
    return this.animate(element, [
      { opacity: 0, transform: 'translateY(8px)' },
      { opacity: 1, transform: 'translateY(0)' }
    ], { delay, fill: 'backwards' });
  }

  updateAnimatedText(element, value) {
    if (element.textContent === value) return;
    element.textContent = value;
    this.reveal(element);
  }

  // Helpers
  formatDateForInput(timestamp) {
    const d = new Date(timestamp * 1000);
    return `${d.getFullYear()}-${String(d.getMonth() + 1).padStart(2, '0')}-${String(d.getDate()).padStart(2, '0')}`;
  }
  
  formatTimeForInput(timestamp) {
    const d = new Date(timestamp * 1000);
    return d.toTimeString().slice(0, 5);
  }
  
  combineDateTime(dateStr, timeStr) {
    const d = new Date(dateStr + 'T' + (timeStr || '00:00'));
    return Math.floor(d.getTime() / 1000);
  }
  
  formatUptime(seconds) {
    const days = Math.floor(seconds / 86400);
    const hours = Math.floor((seconds % 86400) / 3600);
    const mins = Math.floor((seconds % 3600) / 60);
    const secs = seconds % 60;
    
    if (days > 0) return `${days}d ${hours}h ${mins}m`;
    if (hours > 0) return `${hours}h ${mins}m`;
    if (mins > 0) return `${mins}m ${secs}s`;
    return `${secs}s`;
  }
  
  formatBytes(bytes) {
    if (bytes < 1024) return `${bytes} B`;
    if (bytes < 1024 * 1024) return `${(bytes / 1024).toFixed(1)} KB`;
    return `${(bytes / (1024 * 1024)).toFixed(1)} MB`;
  }
  
  escapeHtml(text) {
    const div = document.createElement('div');
    div.textContent = text;
    return div.innerHTML;
  }
}

// Initialize app when DOM is ready
document.addEventListener('DOMContentLoaded', () => {
  window.app = new CYDClockApp();
});
