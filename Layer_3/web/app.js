const MAX_HISTORY = 60;
const trendHistory = {
  pressure: [],
  level: [],
  mw: []
};

let currentBreakerState = false;
let currentSafety1State = false;
let currentRotorAngle = 0;

document.addEventListener('DOMContentLoaded', () => {
  setInterval(updateClock, 1000);
  updateClock();

  document.getElementById('btn-esd').addEventListener('click', () => {
    if (confirm('CONFIRM EMERGENCY PLANT TRIP?')) {
      sendCommand('esd');
    }
  });

  setInterval(pollTelemetry, 300);
  pollTelemetry();
});

function updateClock() {
  const now = new Date();
  document.getElementById('scada-clock').innerText = now.toTimeString().split(' ')[0] + ' UTC';
}

async function pollTelemetry() {
  try {
    const res = await fetch('/api/telemetry');
    if (!res.ok) throw new Error();
    const data = await res.json();
    renderTelemetry(data);
  } catch (err) {
    document.getElementById('plc-status-text').innerText = 'PLC: DISCONNECTED';
    document.getElementById('plc-status-text').style.color = '#ef4444';
  }
}

function renderTelemetry(data) {
  if (!data || !data.connected) {
    document.getElementById('plc-status-text').innerText = 'PLC: DISCONNECTED';
    return;
  }

  document.getElementById('plc-status-text').innerText = 'PLC: ONLINE (#' + (data.heartbeat || 0) + ')';
  document.getElementById('plc-status-text').style.color = '#22c55e';

  const mftPill = document.getElementById('mft-status-text');
  if (data.safety.mft || data.safety.turbineTrip) {
    mftPill.innerText = 'SIS: TRIPPED';
    mftPill.style.color = '#ef4444';
  } else {
    mftPill.innerText = 'SIS: HEALTHY';
    mftPill.style.color = '#22c55e';
  }

  const gridPill = document.getElementById('grid-status-text');
  currentBreakerState = (data.generator.breaker === 1);
  if (currentBreakerState) {
    gridPill.innerText = 'GRID: SYNC';
    gridPill.style.color = '#22c55e';
  } else {
    gridPill.innerText = 'GRID: OPEN';
    gridPill.style.color = '#f59e0b';
  }

  const banner = document.getElementById('alarm-banner');
  const firstOutText = document.getElementById('first-out-text');
  if (data.safety.firstOut > 0) {
    banner.classList.add('active-trip');
    firstOutText.innerText = 'TRIP: ' + data.safety.firstOutName;
  } else {
    banner.classList.remove('active-trip');
    firstOutText.innerText = 'ALL PLANT SYSTEMS OPERATING NORMALLY';
  }

  document.getElementById('val-drum-press').innerHTML = data.boiler.drumPress.toFixed(1) + ' <span class="unit">PSI</span>';
  document.getElementById('val-drum-temp').innerText = data.boiler.drumTemp.toFixed(1) + ' °C';
  document.getElementById('val-drum-level').innerText = data.boiler.drumLevel.toFixed(1) + ' %';

  document.getElementById('val-turbine-rpm').innerHTML = Math.round(data.turbine.rpm) + ' <span class="unit">RPM</span>';
  document.getElementById('val-brg1-vib').innerText = data.turbine.brg1Vib.toFixed(2);
  document.getElementById('val-brg2-vib').innerText = data.turbine.brg2Vib.toFixed(2);

  document.getElementById('val-gen-mw').innerHTML = data.generator.mw.toFixed(1) + ' <span class="unit">MW</span>';
  document.getElementById('val-gen-freq').innerText = data.generator.freq.toFixed(2) + ' Hz';
  document.getElementById('val-gen-volt').innerText = data.generator.volt.toFixed(1) + ' kV';

  document.getElementById('val-cond-vacuum').innerHTML = data.condenser.vacuum.toFixed(1) + ' <span class="unit">inHg</span>';
  document.getElementById('val-fuel-rate').innerText = data.combustion.fuelRate.toFixed(1);
  document.getElementById('val-flue-o2').innerText = data.combustion.flueGasO2.toFixed(1) + ' %';

  const waterLevel = Math.max(0, Math.min(100, data.boiler.drumLevel));
  const waterHeight = (waterLevel / 100.0) * 24;
  const drumWater = document.getElementById('pid-drum-water');
  drumWater.setAttribute('y', 110 + (24 - waterHeight));
  drumWater.setAttribute('height', waterHeight);
  document.getElementById('pid-drum-level-text').innerText = waterLevel.toFixed(1) + '% LVL';

  const flameEl = document.getElementById('flame-text');
  if (data.combustion.flame === 1) {
    flameEl.innerText = 'FLAME: ON';
    flameEl.style.fill = '#22c55e';
  } else {
    flameEl.innerText = 'FLAME: OFF';
    flameEl.style.fill = '#ef4444';
  }

  currentSafety1State = (data.boiler.safety1 === 1);
  document.getElementById('icon-safety1').style.fill = currentSafety1State ? '#ef4444' : '#22c55e';
  document.getElementById('pid-cv-text').innerText = 'CV: ' + data.turbine.cv + '%';
  document.getElementById('pid-turbine-rpm').innerText = Math.round(data.turbine.rpm) + ' RPM';

  const breakerBlade = document.getElementById('pid-breaker-blade');
  const breakerText = document.getElementById('pid-breaker-text');
  if (currentBreakerState) {
    breakerBlade.setAttribute('y1', '15');
    breakerBlade.setAttribute('y2', '15');
    breakerBlade.setAttribute('stroke', '#22c55e');
    breakerText.innerText = '52G CLOSED';
  } else {
    breakerBlade.setAttribute('y1', '5');
    breakerBlade.setAttribute('y2', '15');
    breakerBlade.setAttribute('stroke', '#ef4444');
    breakerText.innerText = '52G OPEN';
  }

  document.getElementById('pid-condenser-vac').innerText = data.condenser.vacuum.toFixed(1) + ' inHg';
  document.getElementById('pid-bfp-flow').innerText = data.boiler.feedFlow.toFixed(1) + ' kg/s';

  trendHistory.pressure.push(data.boiler.drumPress);
  trendHistory.level.push(data.boiler.drumLevel);
  trendHistory.mw.push(data.generator.mw);

  if (trendHistory.pressure.length > MAX_HISTORY) trendHistory.pressure.shift();
  if (trendHistory.level.length > MAX_HISTORY) trendHistory.level.shift();
  if (trendHistory.mw.length > MAX_HISTORY) trendHistory.mw.shift();

  drawTrendChart();
  updateAnnunciator(data.safety.alarmLow, data.safety.alarmHigh);
}

function drawTrendChart() {
  const canvas = document.getElementById('trendCanvas');
  if (!canvas) return;
  const ctx = canvas.getContext('2d');
  const w = canvas.width;
  const h = canvas.height;

  ctx.clearRect(0, 0, w, h);

  ctx.strokeStyle = '#111';
  ctx.lineWidth = 1;
  for (let y = 0; y < h; y += 25) {
    ctx.beginPath();
    ctx.moveTo(0, y);
    ctx.lineTo(w, y);
    ctx.stroke();
  }

  const n = trendHistory.pressure.length;
  if (n < 2) return;

  function plotSeries(arr, maxVal, color) {
    ctx.strokeStyle = color;
    ctx.lineWidth = 1.5;
    ctx.beginPath();
    for (let i = 0; i < n; i++) {
      const x = (i / (MAX_HISTORY - 1)) * w;
      const y = h - (arr[i] / maxVal) * (h - 10) - 5;
      if (i === 0) ctx.moveTo(x, y);
      else ctx.lineTo(x, y);
    }
    ctx.stroke();
  }

  plotSeries(trendHistory.pressure, 220, '#38bdf8');
  plotSeries(trendHistory.level, 100, '#22c55e');
  plotSeries(trendHistory.mw, 500, '#f59e0b');
}

function updateAnnunciator(low, high) {
  let count = 0;
  function setTile(id, active) {
    const el = document.getElementById(id);
    if (!el) return;
    if (active) {
      el.classList.add('active-alarm');
      count++;
    } else {
      el.classList.remove('active-alarm');
    }
  }

  setTile('tile-press-hi', (low & (1 << 0)) !== 0);
  setTile('tile-press-hihi', (low & (1 << 1)) !== 0);
  setTile('tile-level-lo', (low & (1 << 2)) !== 0);
  setTile('tile-level-lolo', (low & (1 << 3)) !== 0);
  setTile('tile-level-hi', (low & (1 << 4)) !== 0);
  setTile('tile-level-hihi', (low & (1 << 5)) !== 0);
  setTile('tile-flame-fail', (low & (1 << 8)) !== 0);
  setTile('tile-turb-vib', (low & (1 << 10)) !== 0);
  setTile('tile-turb-speed', (low & (1 << 12)) !== 0);
  setTile('tile-vac-loss', (low & (1 << 14)) !== 0);
  setTile('tile-mft-trip', (high & (1 << 0)) !== 0);
  setTile('tile-ets-trip', (high & (1 << 1)) !== 0);

  document.getElementById('active-alarm-count').innerText = count + ' ALARMS';
}

async function sendCommand(cmd, val = 0) {
  try {
    await fetch('/api/command', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ cmd, val })
    });
  } catch (err) {}
}

function applyTargetMW() {
  sendCommand('target_mw', parseInt(document.getElementById('slider-mw').value, 10));
}

function applyTargetLevel() {
  sendCommand('target_level', parseInt(document.getElementById('slider-level').value, 10));
}

function toggleBreaker() {
  sendCommand('breaker', currentBreakerState ? 0 : 1);
}

function toggleSafetyValve() {
  sendCommand('safety_valve', currentSafety1State ? 0 : 1);
}

function setPlcMode(mode) {
  sendCommand('mode', mode);
  document.getElementById('btn-mode-auto').classList.toggle('active', mode === 1);
  document.getElementById('btn-mode-manual').classList.toggle('active', mode === 0);
}