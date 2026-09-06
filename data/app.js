// CYD Clock Web Interface
class CYDClockApp {
  constructor() {
    this.ws = null;
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
    await this.loadTimezones();
    await this.fetchSettings();
    await this.fetchCountdowns();
    await this.fetchStatus();
    this.connectWebSocket();
    this.startStatusPolling();
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
      this.renderCountdowns();
      this.updatePreviewCountdown();
    } catch (e) {
      console.error('Failed to fetch countdowns:', e);
    }
  }
  
  async fetchStatus() {
    try {
      const resp = await fetch('/api/status');
      this.status = await resp.json();
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
    this.brightnessSlider.value = this.settings.brightness || 200;
    this.brightnessValue.textContent = this.settings.brightness || 200;
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
    this.fwVersionEl.textContent = this.status.firmwareVersion || '1.0.0';
  }
  
  updateClockPreview(time, date, weekday) {
    this.previewTime.textContent = time;
    this.previewDate.textContent = date;
    this.previewWeekday.textContent = weekday;
  }
  
  updatePreviewCountdown() {
    const next = this.countdowns
      .filter(cd => !cd.expired)
      .sort((a, b) => a.targetDate - b.targetDate)[0];
    
    if (next) {
      this.previewCountdown.classList.remove('empty');
      this.previewCountdown.innerHTML = `
        <div style="color: #${next.accentColor.toString(16).padStart(6, '0')}; font-weight: 600;">${next.name}</div>
        <div class="countdown-remaining">${next.remaining}</div>
      `;
    } else {
      this.previewCountdown.classList.add('empty');
      this.previewCountdown.textContent = 'No active countdowns';
    }
  }
  
  renderCountdowns() {
    this.countdownList.innerHTML = '';
    
    if (this.countdowns.length === 0) {
      this.countdownList.innerHTML = `
        <div style="text-align: center; padding: 2rem; color: var(--text-muted);">
          No countdowns yet. Tap "+ Add" to create one.
        </div>
      `;
      return;
    }
    
    // Sort: active first by date, then expired by priority
    const sorted = [...this.countdowns].sort((a, b) => {
      if (a.expired && !b.expired) return 1;
      if (!a.expired && b.expired) return -1;
      if (a.expired && b.expired) return a.priority - b.priority;
      return a.targetDate - b.targetDate;
    });
    
    sorted.forEach(cd => {
      const item = document.createElement('div');
      item.className = `countdown-item ${cd.expired ? 'expired' : ''}`;
      const colorHex = '#' + cd.accentColor.toString(16).padStart(6, '0');
      item.innerHTML = `
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
      
      item.querySelector('.edit-btn').addEventListener('click', () => this.openCountdownModal(cd));
      item.querySelector('.delete-btn').addEventListener('click', () => this.deleteCountdown(cd.id));
      
      this.countdownList.appendChild(item);
    });
  }
  
  openCountdownModal(cd = null) {
    this.countdownForm.reset();
    this.editCountdownId.value = cd?.id || '';
    this.modalTitle.textContent = cd ? 'Edit Countdown' : 'Add Countdown';
    
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
      this.cdColor.value = '#00ffff';
      this.cdShowOnMain.checked = true;
      this.cdHideWhenExpired.checked = false;
    }
    
    this.countdownModal.classList.add('active');
    this.cdName.focus();
  }
  
  closeCountdownModal() {
    this.countdownModal.classList.remove('active');
  }
  
  async saveCountdown(e) {
    e.preventDefault();
    
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
      } else {
        const err = await resp.json();
        alert('Error: ' + (err.error || 'Failed to save'));
      }
    } catch (e) {
      alert('Error: ' + e.message);
    }
  }
  
  async deleteCountdown(id) {
    if (!confirm('Delete this countdown?')) return;
    
    try {
      const resp = await fetch(`/api/countdowns/${id}`, { method: 'DELETE' });
      if (resp.ok) {
        await this.fetchCountdowns();
      } else {
        alert('Failed to delete');
      }
    } catch (e) {
      alert('Error: ' + e.message);
    }
  }
  
  async saveSettings() {
    const settings = {
      timezone: this.timezoneSelect.value,
      use24Hour: this.timeFormatSelect.value === '24',
      layoutId: parseInt(this.layoutSelect.value),
      themeId: parseInt(this.themeSelect.value),
      brightness: parseInt(this.brightnessSlider.value)
    };
    
    try {
      const resp = await fetch('/api/settings', {
        method: 'PUT',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify(settings)
      });
      
      if (!resp.ok) {
        alert('Failed to save settings');
      } else {
        this.settings = { ...this.settings, ...settings };
      }
    } catch (e) {
      alert('Error: ' + e.message);
    }
  }
  
  async scanWifi() {
    this.scanWifiBtn.disabled = true;
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
    } catch (e) {
      alert('Scan failed: ' + e.message);
    } finally {
      this.scanWifiBtn.disabled = false;
      this.scanWifiBtn.textContent = 'Scan Networks';
    }
  }
  
  async connectWifi(ssid, isOpen) {
    let password = '';
    if (!isOpen) {
      password = prompt(`Enter password for "${ssid}":`);
      if (password === null) return; // User cancelled
    }
    
    try {
      const resp = await fetch('/api/wifi/connect', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ ssid, password })
      });
      
      const result = await resp.json();
      if (result.success) {
        alert('Connected! IP: ' + result.ip);
        this.wifiNetworks.style.display = 'none';
        await this.fetchStatus();
      } else {
        alert('Connection failed: ' + (result.error || 'Unknown error'));
      }
    } catch (e) {
      alert('Error: ' + e.message);
    }
  }
  
  async forgetWifi() {
    if (!confirm('Forget Wi-Fi and enter setup mode?')) return;
    
    try {
      await fetch('/api/wifi/disconnect', { method: 'POST' });
      alert('Wi-Fi forgotten. Device will restart in setup mode.');
      await this.fetchStatus();
    } catch (e) {
      alert('Error: ' + e.message);
    }
  }
  
  async rebootDevice() {
    if (!confirm('Reboot the device?')) return;
    
    try {
      await fetch('/api/reboot', { method: 'POST' });
      alert('Rebooting...');
      this.updateConnectionStatus(false);
    } catch (e) {
      alert('Error: ' + e.message);
    }
  }
  
  // Helpers
  formatDateForInput(timestamp) {
    const d = new Date(timestamp * 1000);
    return d.toISOString().split('T')[0];
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
