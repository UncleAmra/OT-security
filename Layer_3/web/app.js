// Unit 1 HMI - polls /api/telemetry from the L3 server and renders it.

document.addEventListener('DOMContentLoaded', () => {
  setInterval(updateClock, 1000);
  updateClock();

  // Emergency Stop Button
  const btnEsd = document.getElementById('btn-esd');
  if (btnEsd) {
    btnEsd.addEventListener('click', async () => {
      if (confirm('CONFIRM EMERGENCY PLANT SHUTDOWN (ESD)?\n\nThis will trigger immediate Master Fuel Trip (MFT) and Turbine Emergency Trip (ETS). Shutdown will be latched.')) {
        await sendCommand('esd');
        pollTelemetry();
      }
    });
  }

  // Restart Simulation Button
  const btnRestart = document.getElementById('btn-restart');
  if (btnRestart) {
    btnRestart.addEventListener('click', async () => {
      if (confirm('CONFIRM RESTART SIMULATION?\n\nThis will reset all process variables, clear trips, reset Modbus registers, and return the plant to a defined safe cold startup condition.')) {
        await sendCommand('restart');
        pollTelemetry();
      }
    });
  }

  setText('net-hmi-addr', location.hostname || '--');
  setText('net-hmi-port', location.port || (location.protocol === 'https:' ? '443' : '80'));

  setInterval(pollTelemetry, 300);
  pollTelemetry();
});

function updateClock() {
  const now = new Date();
  const timeStr = now.toTimeString().split(' ')[0];
  const el = document.getElementById('scada-clock');
  if (el) el.innerText = timeStr;
}

async function sendCommand(cmd, val = 0) {
  try {
    const res = await fetch('/api/command', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ cmd, val })
    });
    return await res.json();
  } catch (err) {
    console.error('Command dispatch error:', err);
    return { success: false };
  }
}

async function pollTelemetry() {
  const t0 = performance.now();
  try {
    const res = await fetch('/api/telemetry');
    if (!res.ok) throw new Error('Network error');
    const data = await res.json();
    renderTelemetry(data);
    updateComms(true, data, performance.now() - t0);
  } catch (err) {
    renderOffline();
    updateComms(false, null, 0);
  }
}

// Communications panel. Display only, does not affect polling or commands.
const comms = { ok: 0, fail: 0, lastHb: null, lastHbChange: 0 };

function updateComms(httpOk, data, rtt) {
  const now = Date.now();
  const httpEl = document.getElementById('net-http-status');
  const plcEl = document.getElementById('net-plc-status');

  if (httpOk) {
    comms.ok++;
    setStatus(httpEl, 'up', true);
    setText('net-rtt', rtt.toFixed(0) + ' ms');
    setText('net-last', new Date().toTimeString().split(' ')[0]);
  } else {
    comms.fail++;
    setStatus(httpEl, 'no response', false);
    setText('net-rtt', '--');
  }
  setText('net-count', comms.ok + ' / ' + comms.fail);

  if (!httpOk) {
    setStatus(plcEl, 'unknown', null);
    return;
  }
  setStatus(plcEl, data && data.connected ? 'connected' : 'disconnected', !!(data && data.connected));

  if (data && data.heartbeat !== undefined) {
    if (data.heartbeat !== comms.lastHb) {
      comms.lastHb = data.heartbeat;
      comms.lastHbChange = now;
    }
    const age = (now - comms.lastHbChange) / 1000;
    setText('net-hb', data.heartbeat + (age > 3 ? ' (stale ' + age.toFixed(0) + ' s)' : ''));
  }
}

function setStatus(el, text, good) {
  if (!el) return;
  el.innerText = text;
  el.className = good === true ? 'st-ok' : good === false ? 'st-alarm' : 'dim';
}

function renderOffline() {
  const linkBadge = document.getElementById('link-status');
  if (linkBadge) {
    linkBadge.innerText = 'PLC: OFFLINE';
    linkBadge.className = 'badge badge-link';
    linkBadge.style.color = '#e05a4f';
    linkBadge.style.borderColor = '#e05a4f';
  }

  const masterBadge = document.getElementById('master-plant-status');
  if (masterBadge) {
    masterBadge.innerText = 'OFFLINE';
    masterBadge.className = 'status-badge status-offline';
  }

  const secBadge = document.getElementById('security-alert-badge');
  if (secBadge) {
    secBadge.innerText = 'OFFLINE';
    secBadge.className = 'status-badge status-offline';
  }
}

function renderTelemetry(data) {
  if (!data || !data.connected) {
    renderOffline();
    return;
  }

  // 1. Connection & Heartbeat
  const linkBadge = document.getElementById('link-status');
  if (linkBadge) {
    linkBadge.innerText = 'PLC: ONLINE';
    linkBadge.className = 'badge badge-link';
    linkBadge.style.color = '#7fb07a';
    linkBadge.style.borderColor = '#7fb07a';
  }

  const hbBadge = document.getElementById('scan-heartbeat');
  if (hbBadge) {
    hbBadge.innerText = 'HB: #' + (data.heartbeat || 0);
  }

  // 2. Master Status Calculation
  const isMft = data.safety && data.safety.mft === 1;
  const isTurbineTrip = data.safety && data.safety.turbineTrip === 1;
  const isGenTrip = data.safety && data.safety.genTrip === 1;
  const isEsd = data.safety && data.safety.esd === 1;
  const isTripped = isMft || isTurbineTrip || isGenTrip || isEsd;

  const alertLevel = (data.safety && data.safety.securityAlertLevel !== undefined) 
                     ? data.safety.securityAlertLevel : 0;
  const alarmLow = (data.safety && data.safety.alarmLow) || 0;
  const alarmHigh = (data.safety && data.safety.alarmHigh) || 0;
  const hasCriticalAlarm = (alarmLow & (1 << 1)) || (alarmLow & (1 << 3)) || 
                           (alarmLow & (1 << 5)) || (alarmLow & (1 << 10)) || (alarmLow & (1 << 12));
  const hasWarningAlarm = (alarmLow !== 0) || (alarmHigh !== 0);

  const masterBadge = document.getElementById('master-plant-status');
  if (masterBadge) {
    if (isTripped) {
      masterBadge.innerText = 'TRIPPED';
      masterBadge.className = 'status-badge status-tripped';
    } else if (alertLevel === 2 || hasCriticalAlarm) {
      masterBadge.innerText = 'CRITICAL';
      masterBadge.className = 'status-badge status-critical';
    } else if (alertLevel === 1 || hasWarningAlarm) {
      masterBadge.innerText = 'WARNING';
      masterBadge.className = 'status-badge status-warning';
    } else {
      masterBadge.innerText = 'NORMAL';
      masterBadge.className = 'status-badge status-normal';
    }
  }

  // 3. Security Threat Badge
  const secBadge = document.getElementById('security-alert-badge');
  if (secBadge) {
    if (alertLevel === 3 || isTripped) {
      secBadge.innerText = 'TRIPPED / SHUTDOWN';
      secBadge.className = 'status-badge status-tripped';
    } else if (alertLevel === 2) {
      secBadge.innerText = 'ATTACK / ANOMALY DETECTED';
      secBadge.className = 'status-badge status-critical';
    } else if (alertLevel === 1) {
      secBadge.innerText = 'WARNING / ELEVATED';
      secBadge.className = 'status-badge status-warning';
    } else {
      secBadge.innerText = 'NORMAL (HEALTHY)';
      secBadge.className = 'status-badge status-normal';
    }
  }

  // 4. First-Out Banner
  const firstOutText = document.getElementById('first-out-text');
  const firstOutName = (data.safety && data.safety.firstOutName) || 'NONE';
  if (firstOutText) {
    if (data.safety && data.safety.firstOut > 0) {
      firstOutText.innerText = 'TRIP: ' + firstOutName;
      firstOutText.className = 'first-out-val active-trip';
    } else {
      firstOutText.innerText = 'ALL PLANT SYSTEMS NORMAL';
      firstOutText.className = 'first-out-val';
    }
  }

  // 5. Boiler Card Telemetry
  const b = data.boiler || {};
  setVal('val-drum-temp', b.drumTemp ? b.drumTemp.toFixed(1) : '25.0', '&deg;C');
  setVal('val-drum-press', b.drumPress ? b.drumPress.toFixed(1) : '14.7', 'PSI');
  setVal('val-drum-level', b.drumLevel ? b.drumLevel.toFixed(1) : '50.0', '%');
  setVal('val-superheat-temp', b.superheatTemp ? b.superheatTemp.toFixed(1) : '25.0', '&deg;C');
  setVal('val-steam-flow', b.steamFlow ? b.steamFlow.toFixed(1) : '0.0', 'kg/s');
  setVal('val-feed-flow', b.feedFlow ? b.feedFlow.toFixed(1) : '0.0', 'kg/s');

  const srv1 = b.safety1 === 1 ? '<span style="color:#e05a4f;font-weight:700;">OPEN (VENTING)</span>' : '<span style="color:#7fb07a;">CLOSED</span>';
  const srv2 = b.safety2 === 1 ? '<span style="color:#e05a4f;font-weight:700;">OPEN (VENTING)</span>' : '<span style="color:#7fb07a;">CLOSED</span>';
  setHtml('val-safety-valves', 'SRV-1: ' + srv1 + ' | SRV-2: ' + srv2);

  const boilerPill = document.getElementById('boiler-status-pill');
  if (boilerPill) {
    if (b.drumPress >= 190.0 || isMft) {
      boilerPill.innerText = 'TRIPPED';
      boilerPill.style.color = '#e05a4f';
      boilerPill.style.borderColor = '#e05a4f';
    } else if (b.drumPress > 175.0 || b.drumLevel < 35.0 || b.drumLevel > 65.0) {
      boilerPill.innerText = 'WARNING';
      boilerPill.style.color = '#d9a730';
      boilerPill.style.borderColor = '#d9a730';
    } else if (b.drumPress >= 100.0) {
      boilerPill.innerText = 'OPERATING';
      boilerPill.style.color = '#7fb07a';
      boilerPill.style.borderColor = '#7fb07a';
    } else {
      boilerPill.innerText = 'WARMING UP';
      boilerPill.style.color = '#6f9fc8';
      boilerPill.style.borderColor = '#6f9fc8';
    }
  }

  // 6. Combustion Card Telemetry
  const c = data.combustion || {};
  let burnerStateStr = 'OFF';
  if (c.burnerState === 1) burnerStateStr = 'IGNITING';
  if (c.burnerState === 2) burnerStateStr = 'FIRING (ACTIVE)';
  setText('val-burner-state', burnerStateStr);

  const flameEl = document.getElementById('val-flame-status');
  if (flameEl) {
    if (c.flame === 1) {
      flameEl.innerText = 'FLAME PRESENT';
      flameEl.style.color = '#7fb07a';
    } else {
      flameEl.innerText = 'DARK / NO FLAME';
      flameEl.style.color = '#8a929b';
    }
  }

  setVal('val-fuel-rate', c.fuelRate ? c.fuelRate.toFixed(1) : '0.0', 'kg/min');
  setVal('val-furnace-press', c.furnacePress ? c.furnacePress.toFixed(1) : '-10.0', 'mm H2O');
  setVal('val-flue-o2', c.flueGasO2 ? c.flueGasO2.toFixed(1) : '20.9', '%');
  setVal('val-comb-eff', c.efficiency ? c.efficiency.toFixed(1) : '0.0', '%');
  setText('val-fans-state', 'FD: ' + (c.fdFan || 0) + '% | ID: ' + (c.idFan || 0) + '%');

  const combPill = document.getElementById('combustion-status-pill');
  if (combPill) {
    if (isMft) {
      combPill.innerText = 'MFT TRIP';
      combPill.style.color = '#e05a4f';
      combPill.style.borderColor = '#e05a4f';
    } else if (c.burnerState === 2) {
      combPill.innerText = 'FIRING';
      combPill.style.color = '#7fb07a';
      combPill.style.borderColor = '#7fb07a';
    } else {
      combPill.innerText = 'STANDBY';
      combPill.style.color = '#8a929b';
      combPill.style.borderColor = '#3a4047';
    }
  }

  // 7. Turbine Card Telemetry
  const t = data.turbine || {};
  setVal('val-turbine-rpm', t.rpm ? Math.round(t.rpm).toString() : '0', 'RPM');
  setVal('val-brg1-vib', t.brg1Vib ? t.brg1Vib.toFixed(2) : '0.00', 'mm/s');
  setVal('val-brg2-vib', t.brg2Vib ? t.brg2Vib.toFixed(2) : '0.00', 'mm/s');
  setVal('val-lube-press', t.lubeOilPress ? t.lubeOilPress.toFixed(1) : '0.0', 'Bar');
  setVal('val-lube-temp', t.lubeOilTemp ? t.lubeOilTemp.toFixed(1) : '25.0', '&deg;C');
  setVal('val-chest-temp', t.chestTemp ? t.chestTemp.toFixed(1) : '25.0', '&deg;C');

  const msvStr = t.msv === 1 ? '<span style="color:#7fb07a;">OPEN</span>' : '<span style="color:#8a929b;">CLOSED</span>';
  setHtml('val-turbine-valves', 'MSV: ' + msvStr + ' | CV: ' + (t.cv || 0) + '%');

  const turbPill = document.getElementById('turbine-status-pill');
  if (turbPill) {
    if (isTurbineTrip || t.tripped === 1) {
      turbPill.innerText = 'ETS TRIPPED';
      turbPill.style.color = '#e05a4f';
      turbPill.style.borderColor = '#e05a4f';
    } else if (t.rpm >= 3708 || t.brg1Vib > 3.0 || t.brg2Vib > 3.0) {
      turbPill.innerText = 'WARNING';
      turbPill.style.color = '#d9a730';
      turbPill.style.borderColor = '#d9a730';
    } else if (t.rpm >= 3550) {
      turbPill.innerText = 'SYNCHRONOUS (3600 RPM)';
      turbPill.style.color = '#7fb07a';
      turbPill.style.borderColor = '#7fb07a';
    } else if (t.rpm > 100) {
      turbPill.innerText = 'ROLLING UP';
      turbPill.style.color = '#6f9fc8';
      turbPill.style.borderColor = '#6f9fc8';
    } else {
      turbPill.innerText = 'STANDBY / STOPPED';
      turbPill.style.color = '#8a929b';
      turbPill.style.borderColor = '#3a4047';
    }
  }

  // 8. Generator Card Telemetry
  const g = data.generator || {};
  setVal('val-gen-mw', g.mw ? g.mw.toFixed(1) : '0.0', 'MW');
  setVal('val-gen-freq', g.freq ? g.freq.toFixed(2) : '0.00', 'Hz');
  setVal('val-gen-volt', g.volt ? g.volt.toFixed(1) : '0.0', 'kV');
  setVal('val-power-factor', g.powerFactor ? g.powerFactor.toFixed(2) : '1.00', '');
  setVal('val-stator-temp', g.statorTemp ? g.statorTemp.toFixed(1) : '25.0', '&deg;C');

  const breakerEl = document.getElementById('val-breaker-state');
  if (breakerEl) {
    if (g.breaker === 1) {
      breakerEl.innerText = 'CLOSED (GRID TIED)';
      breakerEl.style.color = '#7fb07a';
    } else {
      breakerEl.innerText = 'OPEN (ISOLATED)';
      breakerEl.style.color = '#d9a730';
    }
  }

  const syncEl = document.getElementById('val-sync-permit');
  if (syncEl) {
    if (g.syncPermit === 1) {
      syncEl.innerText = 'PERMITTED';
      syncEl.style.color = '#7fb07a';
    } else {
      syncEl.innerText = 'LOCKED';
      syncEl.style.color = '#8a929b';
    }
  }

  const genPill = document.getElementById('generator-status-pill');
  if (genPill) {
    if (isGenTrip) {
      genPill.innerText = '86G TRIPPED';
      genPill.style.color = '#e05a4f';
      genPill.style.borderColor = '#e05a4f';
    } else if (g.breaker === 1) {
      genPill.innerText = 'ON-LINE (' + (g.mw ? g.mw.toFixed(0) : '0') + ' MW)';
      genPill.style.color = '#7fb07a';
      genPill.style.borderColor = '#7fb07a';
    } else {
      genPill.innerText = 'OFF-LINE';
      genPill.style.color = '#8a929b';
      genPill.style.borderColor = '#3a4047';
    }
  }

  // 9. Condenser Card Telemetry
  const cd = data.condenser || {};
  setVal('val-cond-vacuum', cd.vacuum ? cd.vacuum.toFixed(1) : '28.5', 'inHg');
  setVal('val-hotwell-temp', cd.hotwellTemp ? cd.hotwellTemp.toFixed(1) : '25.0', '&deg;C');
  setVal('val-circ-flow', cd.circFlow ? cd.circFlow.toFixed(1) : '70.0', '%');
  setVal('val-bfp-a', cd.bfpA ? cd.bfpA.toFixed(1) : '0.0', '%');
  setVal('val-bfp-b', cd.bfpB ? cd.bfpB.toFixed(1) : '0.0', '%');
  setText('val-cep-state', cd.cepState === 1 ? 'RUNNING' : 'STOPPED');
  setText('val-deaerator-state', 'Level: ' + (cd.deaeratorLevel || 60) + '% | Press: 1.2 Bar');

  const condPill = document.getElementById('condenser-status-pill');
  if (condPill) {
    if (cd.vacuum < 8.0 && t.rpm > 500) {
      condPill.innerText = 'TRIPPED';
      condPill.style.color = '#e05a4f';
      condPill.style.borderColor = '#e05a4f';
    } else if (cd.vacuum < 18.0) {
      condPill.innerText = 'POOR VACUUM';
      condPill.style.color = '#d9a730';
      condPill.style.borderColor = '#d9a730';
    } else {
      condPill.innerText = 'HEALTHY';
      condPill.style.color = '#7fb07a';
      condPill.style.borderColor = '#7fb07a';
    }
  }

  // 10. Security & Anomaly Monitor Card
  const anomBits = (data.safety && data.safety.anomalyStatus) || 0;
  const anomMsgEl = document.getElementById('anomaly-message');
  if (anomMsgEl) {
    let anomList = [];
    if (anomBits & (1 << 0)) anomList.push('RAPID FUEL MANIPULATION DETECTED');
    if (anomBits & (1 << 1)) anomList.push('BOILER OVERPRESSURE DISPARITY');
    if (anomBits & (1 << 2)) anomList.push('MASS BALANCE VIOLATION (DRUM LOSS)');
    if (anomBits & (1 << 3)) anomList.push('TURBINE RUNAWAY ACCELERATION');
    if (anomBits & (1 << 4)) anomList.push('COOLING WATER COLLAPSE DETECTED');
    if (anomBits & (1 << 5)) anomList.push('SENSOR SPOOFING / PHYSICAL INCONSISTENCY');

    if (anomList.length > 0) {
      anomMsgEl.innerText = anomList.join(' | ');
      anomMsgEl.className = 'anomaly-message anomaly-active';
    } else {
      anomMsgEl.innerText = 'NO DETECTED THREATS OR ANOMALIES';
      anomMsgEl.className = 'anomaly-message';
    }
  }

  const mftStateEl = document.getElementById('val-mft-state');
  if (mftStateEl) {
    mftStateEl.innerText = isMft ? 'TRIPPED (FUEL CUTOFF)' : 'HEALTHY';
    mftStateEl.style.color = isMft ? '#e05a4f' : '#7fb07a';
  }

  const etsStateEl = document.getElementById('val-ets-state');
  if (etsStateEl) {
    etsStateEl.innerText = isTurbineTrip ? 'TRIPPED (VALVES SHUT)' : 'HEALTHY';
    etsStateEl.style.color = isTurbineTrip ? '#e05a4f' : '#7fb07a';
  }

  const esdLatchEl = document.getElementById('val-esd-latch');
  if (esdLatchEl) {
    esdLatchEl.innerText = isEsd ? 'SHUTDOWN LATCHED' : 'NORMAL';
    esdLatchEl.style.color = isEsd ? '#e05a4f' : '#7fb07a';
  }

  const plcModeEl = document.getElementById('val-plc-mode');
  if (plcModeEl) {
    const mode = data.plcMode || 1;
    plcModeEl.innerText = mode === 1 ? 'AUTO / CASCADE REGULATION' : 'MANUAL OVERRIDE MODE';
  }

  const sisPill = document.getElementById('sis-status-pill');
  if (sisPill) {
    if (isTripped) {
      sisPill.innerText = 'TRIPPED';
      sisPill.style.color = '#e05a4f';
      sisPill.style.borderColor = '#e05a4f';
    } else if (alertLevel > 0) {
      sisPill.innerText = 'ALERT';
      sisPill.style.color = '#d9a730';
      sisPill.style.borderColor = '#d9a730';
    } else {
      sisPill.innerText = 'HEALTHY';
      sisPill.style.color = '#7fb07a';
      sisPill.style.borderColor = '#7fb07a';
    }
  }

  // 11. Annunciator Matrix
  updateAnnunciator(alarmLow, alarmHigh, isEsd);
}

function updateAnnunciator(low, high, isEsd) {
  function setTile(id, active, isTrip = true) {
    const el = document.getElementById(id);
    if (!el) return;
    if (active) {
      el.className = isTrip ? 'annun-tile active-alarm' : 'annun-tile active-warn';
    } else {
      el.className = 'annun-tile';
    }
  }

  setTile('tile-press-hi', (low & (1 << 0)) !== 0, false);
  setTile('tile-press-hihi', (low & (1 << 1)) !== 0, true);
  setTile('tile-level-lo', (low & (1 << 2)) !== 0, false);
  setTile('tile-level-lolo', (low & (1 << 3)) !== 0, true);
  setTile('tile-level-hi', (low & (1 << 4)) !== 0, false);
  setTile('tile-level-hihi', (low & (1 << 5)) !== 0, true);
  setTile('tile-flame-fail', (low & (1 << 8)) !== 0, true);
  setTile('tile-turb-vib', (low & (1 << 10)) !== 0, true);
  setTile('tile-turb-speed', (low & (1 << 12)) !== 0, true);
  setTile('tile-vac-loss', (low & (1 << 14)) !== 0, false);
  setTile('tile-mft-active', (high & (1 << 0)) !== 0, true);
  setTile('tile-esd-latched', isEsd, true);
}

function setText(id, text) {
  const el = document.getElementById(id);
  if (el) el.innerText = text;
}

function setHtml(id, html) {
  const el = document.getElementById(id);
  if (el) el.innerHTML = html;
}

function setVal(id, val, unit) {
  const el = document.getElementById(id);
  if (el) el.innerHTML = val + ' <span class="m-unit">' + unit + '</span>';
}
