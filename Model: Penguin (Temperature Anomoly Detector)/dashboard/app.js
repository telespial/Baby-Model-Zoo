"use strict";

const elements = {
  clock: document.querySelector("#clock"),
  status: document.querySelector("#status"),
  statusText: document.querySelector("#statusText"),
  connect: document.querySelector("#connectButton"),
  celsius: document.querySelector("#celsius"),
  fahrenheit: document.querySelector("#fahrenheit"),
  rate: document.querySelector("#rate"),
  rateLabel: document.querySelector("#rateLabel"),
  intervalLock: document.querySelector("#intervalLock"),
  trainingWindow: document.querySelector("#trainingWindow"),
  trainingLabel: document.querySelector("#trainingLabel"),
  trainingLock: document.querySelector("#trainingLock"),
  aiFilter: document.querySelector("#aiFilter"),
  aiAnomaly: document.querySelector("#aiAnomaly"),
  anomalyAlert: document.querySelector("#anomalyAlert"),
  anomalyAlarm: document.querySelector("#anomalyAlarm"),
  anomalyNotice: document.querySelector("#anomalyNotice"),
  chart: document.querySelector("#chart"),
  chartMode: document.querySelector("#chartMode"),
  emptyChart: document.querySelector("#emptyChart"),
  sampleCount: document.querySelector("#sampleCount"),
  recordDot: document.querySelector("#recordDot"),
  recordState: document.querySelector("#recordState"),
  memoryState: document.querySelector("#memoryState"),
  recordButton: document.querySelector("#recordButton"),
  playbackButton: document.querySelector("#playbackButton"),
  showAllButton: document.querySelector("#showAllButton"),
  exportButton: document.querySelector("#exportButton"),
  clearButton: document.querySelector("#clearButton"),
};

const RECORD_RATES = [1000, 5000, 10000, 20000, 30000, 60000, 300000, 600000, 1800000, 3600000];
const TRAINING_SAMPLES = [32, 64, 96, 128, 160, 192, 224, 256];
const TELEMETRY_PERIOD_MS = 200;
const MAX_SAMPLES = 1800;
const state = {
  port: null,
  reader: null,
  keepReading: false,
  samples: [],
  recording: true,
  synced: false,
  logCount: 0,
  logCapacity: 1024,
  playback: false,
  writeChain: Promise.resolve(),
  rateTimer: null,
  trainingTimer: null,
  trainingSamples: 128,
  viewportStart: null,
  viewportEnd: null,
  drag: null,
  lastGraphTimestamp: null,
  lastGraphWallTime: null,
  localInterval: true,
  intervalLocked: true,
  trainingLocked: true,
  aiFilter: false,
  aiAnomaly: true,
  anomalyAlert: true,
  anomalyAlarm: false,
  anomalyNoticeTimer: null,
  lastAnomalyState: false,
};

const dateFormatter = new Intl.DateTimeFormat(undefined, { weekday: "short", year: "numeric", month: "short", day: "2-digit" });
const timeFormatter = new Intl.DateTimeFormat(undefined, { hour: "2-digit", minute: "2-digit", second: "2-digit" });
const axisFormatter = new Intl.DateTimeFormat(undefined, { month: "short", day: "2-digit", hour: "2-digit", minute: "2-digit", second: "2-digit" });

function updateClock() {
  const now = new Date();
  elements.clock.textContent = `${dateFormatter.format(now)}\n${timeFormatter.format(now)}`;
}

function setConnection(stateName, text) {
  elements.status.dataset.state = stateName;
  elements.statusText.textContent = text;
}

function formatRate(milliseconds) {
  if (milliseconds >= 3600000) return `${(milliseconds / 3600000).toFixed(0)} hr`;
  if (milliseconds >= 60000) return `${(milliseconds / 60000).toFixed(0)} min`;
  return `${(milliseconds / 1000).toFixed(milliseconds < 10000 ? 0 : 0)} s`;
}

function selectedRate() {
  return RECORD_RATES[Number(elements.rate.value)];
}

function selectedTrainingSamples() {
  return TRAINING_SAMPLES[Number(elements.trainingWindow.value)];
}

function formatTraining(samples) {
  return `${samples} samples · ~${((samples * TELEMETRY_PERIOD_MS) / 1000).toFixed(1)} s`;
}

function setPill(button, enabled, label) {
  button.setAttribute("aria-pressed", String(enabled));
  button.classList.toggle("is-on", enabled);
  const stateLabel = button.querySelector("span");
  if (stateLabel) stateLabel.textContent = enabled ? "On" : "Off";
  else button.textContent = label ? `${label}: ${enabled ? "On" : "Off"}` : (enabled ? "On" : "Off");
}

function updateControlPills() {
  setPill(elements.intervalLock, state.intervalLocked, "Lock");
  setPill(elements.trainingLock, state.trainingLocked, "Lock");
  setPill(elements.aiFilter, state.aiFilter);
  setPill(elements.aiAnomaly, state.aiAnomaly);
  setPill(elements.anomalyAlert, state.anomalyAlert);
  setPill(elements.anomalyAlarm, state.anomalyAlarm);
}

function showAnomalyNotice(message) {
  if (!state.anomalyAlert || !state.aiAnomaly) return;
  elements.anomalyNotice.textContent = message;
  elements.anomalyNotice.hidden = false;
  elements.anomalyNotice.classList.add("is-active");
  clearTimeout(state.anomalyNoticeTimer);
  state.anomalyNoticeTimer = setTimeout(() => {
    elements.anomalyNotice.hidden = true;
    elements.anomalyNotice.classList.remove("is-active");
  }, 5000);
  if (state.anomalyAlarm && typeof navigator.vibrate === "function") navigator.vibrate([120, 80, 120]);
}

function selectRate(milliseconds) {
  const previous = selectedRate();
  let closest = 0;
  RECORD_RATES.forEach((rate, index) => {
    if (Math.abs(rate - milliseconds) < Math.abs(RECORD_RATES[closest] - milliseconds)) closest = index;
  });
  elements.rate.value = String(closest);
  elements.rateLabel.textContent = formatRate(RECORD_RATES[closest]);
  if (previous !== RECORD_RATES[closest]) {
    state.lastGraphTimestamp = null;
    state.lastGraphWallTime = null;
  }
}

function selectTrainingSamples(samples) {
  let closest = 0;
  TRAINING_SAMPLES.forEach((candidate, index) => {
    if (Math.abs(candidate - samples) < Math.abs(TRAINING_SAMPLES[closest] - samples)) closest = index;
  });
  state.trainingSamples = TRAINING_SAMPLES[closest];
  elements.trainingWindow.value = String(closest);
  elements.trainingLabel.textContent = formatTraining(state.trainingSamples);
}

function updateRecorderUi() {
  const connected = Boolean(state.port);
  elements.recordDot.dataset.recording = connected ? String(state.recording) : "";
  elements.recordState.textContent = !connected ? "Ready to record" : state.recording ? "Recording" : "Recording stopped";
  elements.memoryState.textContent = `${state.synced ? "RTC synced" : "RTC waiting for sync"} · ${state.logCount} / ${state.logCapacity} records`;
  elements.recordButton.disabled = !connected;
  elements.playbackButton.disabled = !connected;
  elements.showAllButton.disabled = state.samples.length < 2;
  elements.exportButton.disabled = state.samples.length === 0;
  elements.clearButton.disabled = !connected;
  elements.rate.disabled = state.intervalLocked;
  elements.rate.setAttribute("aria-disabled", String(state.intervalLocked));
  elements.recordButton.dataset.recording = String(state.recording);
  elements.recordButton.textContent = connected && state.recording ? "Pause recording" : "Start recording";
  elements.trainingWindow.disabled = !connected || state.trainingLocked;
  elements.trainingWindow.setAttribute("aria-disabled", String(!connected || state.trainingLocked));
  elements.aiFilter.disabled = !connected;
  elements.aiAnomaly.disabled = !connected;
}

function timestampFromMessage(message) {
  const epoch = Number(message.epoch);
  return Number.isFinite(epoch) && epoch >= 946684800 ? epoch * 1000 : Date.now();
}

function addSample(message, fromPlayback = false) {
  const tempC = Number(message.temp_c);
  const tempF = Number.isFinite(Number(message.temp_f)) ? Number(message.temp_f) : (tempC * 1.8) + 32;
  if (!Number.isFinite(tempC) || !Number.isFinite(tempF)) return;

  elements.celsius.textContent = tempC.toFixed(2);
  elements.fahrenheit.textContent = tempF.toFixed(2);
  const timestamp = timestampFromMessage(message);
  if (!fromPlayback) {
    state.lastGraphTimestamp = timestamp;
    state.lastGraphWallTime = Date.now();
  }
  state.samples.push({ timestamp, tempC, tempF, fromPlayback });
  if (state.samples.length > MAX_SAMPLES) state.samples.splice(0, state.samples.length - MAX_SAMPLES);
  elements.emptyChart.hidden = true;
  elements.sampleCount.textContent = `${state.samples.length} sample${state.samples.length === 1 ? "" : "s"}`;
  updateRecorderUi();
  drawChart();
}

function resetViewport() {
  state.viewportStart = null;
  state.viewportEnd = null;
}

function visibleSamples() {
  if (state.viewportStart === null || state.viewportEnd === null) return state.samples;
  return state.samples.slice(state.viewportStart, state.viewportEnd + 1);
}

function applyBoardState(message) {
  if (typeof message.recording === "boolean") state.recording = message.recording;
  if (typeof message.synced === "boolean") state.synced = message.synced;
  if (Number.isFinite(Number(message.log_count))) state.logCount = Number(message.log_count);
  if (Number.isFinite(Number(message.log_capacity))) state.logCapacity = Number(message.log_capacity);
  if (Number.isFinite(Number(message.record_ms))) selectRate(Number(message.record_ms));
  if (Number.isFinite(Number(message.training_samples))) selectTrainingSamples(Number(message.training_samples));
  if (typeof message.ai_filter === "boolean") state.aiFilter = message.ai_filter;
  if (typeof message.ai_anomaly === "boolean") state.aiAnomaly = message.ai_anomaly;
  if (state.synced && state.port) setConnection("online", "Board connected · RTC synced");
  updateControlPills();
  updateRecorderUi();
}

function handleLine(line) {
  if (!line.trim()) return;
  try {
    const message = JSON.parse(line);
    if (message.type === "sample" && !state.playback) {
      const aiState = String(message.ai_state || "").toLowerCase();
      const isAnomaly = aiState === "anomaly";
      if (isAnomaly && !state.lastAnomalyState) showAnomalyNotice("Temperature Anomaly Detected");
      state.lastAnomalyState = isAnomaly;
      addSample(message);
      applyBoardState(message);
    } else if (message.type === "log") {
      addSample(message, true);
    } else if (message.type === "hello") {
      if (Number.isFinite(Number(message.log_capacity))) state.logCapacity = Number(message.log_capacity);
      setConnection("online", `${message.sensor || "Sensor"} connected`);
      updateRecorderUi();
    } else if (message.type === "state" || message.type === "status") {
      applyBoardState(message);
    } else if (message.type === "time") {
      state.synced = Boolean(message.synced);
      setConnection(state.synced ? "online" : "error", state.synced ? "Board connected · RTC synced" : "RTC synchronization failed");
      updateRecorderUi();
    } else if (message.type === "rate") {
      selectRate(Number(message.record_ms));
    } else if (message.type === "training") {
      selectTrainingSamples(Number(message.training_samples));
    } else if (message.type === "ai_filter" || message.type === "ai_anomaly") {
      applyBoardState(message);
    } else if (message.type === "playback_start") {
      state.playback = true;
      state.samples = [];
      resetViewport();
      elements.chartMode.textContent = "Flash playback";
      elements.emptyChart.hidden = false;
      elements.emptyChart.textContent = "Loading recorded temperatures…";
      drawChart();
    } else if (message.type === "playback_end") {
      state.playback = false;
      state.logCount = Number(message.log_count) || 0;
      elements.chartMode.textContent = "Recorded history";
      elements.emptyChart.hidden = state.samples.length > 0;
      resetViewport();
      updateRecorderUi();
    } else if (message.type === "clear") {
      if (message.success !== true) {
        setConnection("error", "Logger memory could not be cleared");
        return;
      }
      state.logCount = Number(message.log_count) || 0;
      state.samples = [];
      resetViewport();
      elements.chartMode.textContent = "Live history";
      elements.emptyChart.hidden = false;
      elements.emptyChart.textContent = "Logger memory cleared";
      updateRecorderUi();
      drawChart();
    } else if (message.type === "error") {
      setConnection("error", message.message || "Board error");
    }
  } catch {
    // Ignore bootloader and non-JSON diagnostics without interrupting telemetry.
  }
}

elements.intervalLock.addEventListener("click", () => {
  state.intervalLocked = !state.intervalLocked;
  localStorage.setItem("embeddedx.mcxc162.intervalLocked", String(state.intervalLocked));
  updateControlPills();
  updateRecorderUi();
});
elements.trainingLock.addEventListener("click", () => {
  state.trainingLocked = !state.trainingLocked;
  localStorage.setItem("embeddedx.mcxc162.trainingLocked", String(state.trainingLocked));
  updateControlPills();
  updateRecorderUi();
});
elements.aiFilter.addEventListener("click", () => {
  state.aiFilter = !state.aiFilter;
  updateControlPills();
  void sendCommand(`AIFILTER ${state.aiFilter ? 1 : 0}`);
  drawChart();
});
elements.aiAnomaly.addEventListener("click", () => {
  state.aiAnomaly = !state.aiAnomaly;
  if (!state.aiAnomaly) { elements.anomalyNotice.hidden = true; elements.anomalyNotice.classList.remove("is-active"); }
  updateControlPills();
  void sendCommand(`AIANOMALY ${state.aiAnomaly ? 1 : 0}`);
});
elements.anomalyAlert.addEventListener("click", () => {
  state.anomalyAlert = !state.anomalyAlert;
  localStorage.setItem("embeddedx.mcxc162.anomalyAlert", String(state.anomalyAlert));
  if (!state.anomalyAlert) { elements.anomalyNotice.hidden = true; elements.anomalyNotice.classList.remove("is-active"); }
  updateControlPills();
});
elements.anomalyAlarm.addEventListener("click", () => {
  state.anomalyAlarm = !state.anomalyAlarm;
  localStorage.setItem("embeddedx.mcxc162.anomalyAlarm", String(state.anomalyAlarm));
  updateControlPills();
});

async function readSerial() {
  const decoder = new TextDecoder();
  let pending = "";
  state.keepReading = true;
  while (state.port?.readable && state.keepReading) {
    state.reader = state.port.readable.getReader();
    try {
      while (state.keepReading) {
        const { value, done } = await state.reader.read();
        if (done) break;
        pending += decoder.decode(value, { stream: true });
        const lines = pending.split(/\r?\n/);
        pending = lines.pop() || "";
        lines.forEach(handleLine);
      }
    } catch (error) {
      if (state.keepReading) setConnection("error", error.message || "Serial read failed");
    } finally {
      state.reader.releaseLock();
      state.reader = null;
    }
  }
}

function sendCommand(command) {
  state.writeChain = state.writeChain.then(async () => {
    if (!state.port?.writable) return;
    const writer = state.port.writable.getWriter();
    try {
      await writer.write(new TextEncoder().encode(`${command}\n`));
    } finally {
      writer.releaseLock();
    }
  }).catch((error) => setConnection("error", error.message || "Serial write failed"));
  return state.writeChain;
}

async function disconnect() {
  state.keepReading = false;
  if (state.reader) await state.reader.cancel().catch(() => {});
  await state.writeChain.catch(() => {});
  if (state.port) await state.port.close().catch(() => {});
  state.port = null;
  state.synced = false;
  elements.connect.textContent = "Connect board";
  setConnection("offline", "Not connected");
  updateRecorderUi();
}

async function connect() {
  if (state.port) return disconnect();
  if (!("serial" in navigator)) {
    setConnection("error", "Web Serial unavailable");
    elements.emptyChart.textContent = "Use current Chrome or Edge on localhost to connect by USB";
    return;
  }
  try {
    state.port = await navigator.serial.requestPort();
    await state.port.open({ baudRate: 115200, dataBits: 8, stopBits: 1, parity: "none", flowControl: "none" });
    elements.connect.textContent = "Disconnect";
    setConnection("online", "Synchronizing board RTC…");
    updateRecorderUi();
    void readSerial();
    await sendCommand(`TIME ${Math.floor(Date.now() / 1000)}`);
    await sendCommand("STATUS");
  } catch (error) {
    if (state.port?.readable || state.port?.writable) await disconnect();
    else state.port = null;
    if (error.name !== "NotFoundError") setConnection("error", error.message || "Connection failed");
  }
}

function drawChart() {
  const canvas = elements.chart;
  const rect = canvas.getBoundingClientRect();
  const ratio = Math.max(1, window.devicePixelRatio || 1);
  const width = Math.max(1, rect.width);
  const height = Math.max(1, rect.height);
  canvas.width = Math.round(width * ratio);
  canvas.height = Math.round(height * ratio);
  const ctx = canvas.getContext("2d");
  ctx.setTransform(ratio, 0, 0, ratio, 0, 0);
  ctx.clearRect(0, 0, width, height);

  const pad = { top: 12, right: 58, bottom: 42, left: 56 };
  const plotW = Math.max(1, width - pad.left - pad.right);
  const plotH = Math.max(1, height - pad.top - pad.bottom);
  const plottedSamples = visibleSamples();
  const values = plottedSamples.map((sample) => sample.tempC);
  let minC = values.length ? Math.min(...values) : 15;
  let maxC = values.length ? Math.max(...values) : 30;
  const spread = Math.max(2, maxC - minC);
  minC = Math.floor((minC - spread * 0.25) * 2) / 2;
  maxC = Math.ceil((maxC + spread * 0.25) * 2) / 2;

  ctx.font = "11px ui-sans-serif, system-ui, sans-serif";
  ctx.lineWidth = 1;
  ctx.textBaseline = "middle";
  for (let i = 0; i <= 5; i++) {
    const y = pad.top + (plotH * i) / 5;
    const c = maxC - ((maxC - minC) * i) / 5;
    ctx.strokeStyle = "rgba(137, 174, 192, 0.11)";
    ctx.beginPath(); ctx.moveTo(pad.left, y); ctx.lineTo(pad.left + plotW, y); ctx.stroke();
    ctx.fillStyle = "#7593a3";
    ctx.textAlign = "right"; ctx.fillText(`${c.toFixed(1)} °C`, pad.left - 9, y);
    ctx.textAlign = "left"; ctx.fillText(`${((c * 1.8) + 32).toFixed(1)} °F`, pad.left + plotW + 9, y);
  }

  const firstTime = plottedSamples[0]?.timestamp ?? Date.now() - 4000;
  const lastTime = plottedSamples.at(-1)?.timestamp ?? Date.now();
  const timeRange = Math.max(1000, lastTime - firstTime);
  ctx.textBaseline = "top";
  for (let i = 0; i <= 4; i++) {
    const x = pad.left + (plotW * i) / 4;
    const stamp = firstTime + (timeRange * i) / 4;
    ctx.strokeStyle = "rgba(137, 174, 192, 0.07)";
    ctx.beginPath(); ctx.moveTo(x, pad.top); ctx.lineTo(x, pad.top + plotH); ctx.stroke();
    ctx.fillStyle = "#7593a3";
    ctx.textAlign = i === 0 ? "left" : i === 4 ? "right" : "center";
    ctx.fillText(axisFormatter.format(stamp), x, pad.top + plotH + 12);
  }

  if (!plottedSamples.length) return;
  const point = (sample) => ({
    x: pad.left + ((sample.timestamp - firstTime) / timeRange) * plotW,
    y: pad.top + ((maxC - sample.tempC) / (maxC - minC)) * plotH,
  });
  const gradient = ctx.createLinearGradient(0, pad.top, 0, pad.top + plotH);
  gradient.addColorStop(0, "rgba(56, 214, 210, 0.25)");
  gradient.addColorStop(1, "rgba(56, 214, 210, 0)");
  ctx.beginPath();
  plottedSamples.forEach((sample, index) => {
    const p = point(sample);
    if (index) ctx.lineTo(p.x, p.y);
    else ctx.moveTo(p.x, p.y);
  });
  const last = point(plottedSamples.at(-1));
  ctx.lineTo(last.x, pad.top + plotH); ctx.lineTo(pad.left, pad.top + plotH); ctx.closePath();
  ctx.fillStyle = gradient; ctx.fill();
  ctx.beginPath();
  plottedSamples.forEach((sample, index) => {
    const p = point(sample);
    if (index) ctx.lineTo(p.x, p.y);
    else ctx.moveTo(p.x, p.y);
  });
  ctx.strokeStyle = "#38d6d2"; ctx.lineWidth = 2.2; ctx.lineJoin = "round"; ctx.stroke();
  ctx.beginPath(); ctx.arc(last.x, last.y, 4, 0, Math.PI * 2); ctx.fillStyle = "#e6ffff"; ctx.fill();
}

elements.connect.addEventListener("click", connect);
elements.rate.addEventListener("input", () => {
  if (state.intervalLocked) {
    updateRecorderUi();
    return;
  }
  elements.rateLabel.textContent = formatRate(selectedRate());
  localStorage.setItem("embeddedx.mcxc162.recordRateMs", String(selectedRate()));
  clearTimeout(state.rateTimer);
  state.rateTimer = setTimeout(() => void sendCommand(`RATE ${selectedRate()}`), 180);
});
elements.rate.addEventListener("change", () => {
  if (!state.intervalLocked) void sendCommand(`RATE ${selectedRate()}`);
  else updateRecorderUi();
});
elements.trainingWindow.addEventListener("input", () => {
  if (state.trainingLocked) {
    updateRecorderUi();
    return;
  }
  const samples = selectedTrainingSamples();
  state.trainingSamples = samples;
  elements.trainingLabel.textContent = formatTraining(samples);
  clearTimeout(state.trainingTimer);
  state.trainingTimer = setTimeout(() => void sendCommand(`TRAIN ${samples}`), 180);
});
elements.trainingWindow.addEventListener("change", () => {
  clearTimeout(state.trainingTimer);
  if (!state.trainingLocked) void sendCommand(`TRAIN ${selectedTrainingSamples()}`);
  else updateRecorderUi();
});
elements.recordButton.addEventListener("click", () => void sendCommand(`RECORD ${state.recording ? 0 : 1}`));
elements.playbackButton.addEventListener("click", () => void sendCommand("PLAY"));
elements.showAllButton.addEventListener("click", () => { resetViewport(); drawChart(); });
elements.exportButton.addEventListener("click", () => {
  if (!state.samples.length) return;
  const rows = ["datetime,epoch,temp_c,temp_f,source"];
  state.samples.forEach((sample) => {
    const epoch = Math.round(sample.timestamp / 1000);
    rows.push(`${new Date(sample.timestamp).toISOString()},${epoch},${sample.tempC.toFixed(2)},${sample.tempF.toFixed(2)},${sample.fromPlayback ? "flash" : "live"}`);
  });
  const blob = new Blob([rows.join("\n") + "\n"], { type: "text/csv;charset=utf-8" });
  const link = document.createElement("a");
  link.href = URL.createObjectURL(blob);
  link.download = `embeddedx-mcxc162-temperature-${new Date().toISOString().replaceAll(":", "-")}.csv`;
  link.click();
  setTimeout(() => URL.revokeObjectURL(link.href), 0);
});
elements.clearButton.addEventListener("click", () => {
  const warning = "Reset the logger and permanently erase every recorded temperature? This cannot be undone.";
  if (window.confirm(warning)) void sendCommand("CLEAR");
});
navigator.serial?.addEventListener("disconnect", (event) => { if (event.target === state.port) void disconnect(); });
elements.chart.addEventListener("wheel", (event) => {
  if (state.samples.length < 3) return;
  event.preventDefault();
  const currentStart = state.viewportStart ?? 0;
  const currentEnd = state.viewportEnd ?? state.samples.length - 1;
  const currentLength = currentEnd - currentStart + 1;
  const nextLength = Math.max(3, Math.min(state.samples.length, Math.round(currentLength * (event.deltaY > 0 ? 1.25 : 0.8))));
  const rect = elements.chart.getBoundingClientRect();
  const fraction = Math.max(0, Math.min(1, (event.clientX - rect.left) / rect.width));
  const anchor = currentStart + Math.round((currentLength - 1) * fraction);
  state.viewportStart = Math.max(0, Math.min(anchor - Math.round(nextLength * fraction), state.samples.length - nextLength));
  state.viewportEnd = state.viewportStart + nextLength - 1;
  drawChart();
}, { passive: false });
elements.chart.addEventListener("pointerdown", (event) => {
  if (state.samples.length < 3) return;
  elements.chart.setPointerCapture(event.pointerId);
  state.drag = { x: event.clientX, start: state.viewportStart ?? 0, end: state.viewportEnd ?? state.samples.length - 1 };
});
elements.chart.addEventListener("pointermove", (event) => {
  if (!state.drag) return;
  const rect = elements.chart.getBoundingClientRect();
  const span = state.drag.end - state.drag.start;
  const delta = Math.round((event.clientX - state.drag.x) / Math.max(1, rect.width) * span);
  const length = span + 1;
  const start = Math.max(0, Math.min(state.samples.length - length, state.drag.start - delta));
  state.viewportStart = start;
  state.viewportEnd = start + length - 1;
  drawChart();
});
elements.chart.addEventListener("pointerup", () => { state.drag = null; });
elements.chart.addEventListener("pointercancel", () => { state.drag = null; });
new ResizeObserver(drawChart).observe(elements.chart);
updateClock();
setInterval(updateClock, 1000);
const savedRateValue = localStorage.getItem("embeddedx.mcxc162.recordRateMs");
const savedLock = localStorage.getItem("embeddedx.mcxc162.intervalLocked");
const savedTrainingLock = localStorage.getItem("embeddedx.mcxc162.trainingLocked");
const savedAnomalyAlert = localStorage.getItem("embeddedx.mcxc162.anomalyAlert");
const savedAnomalyAlarm = localStorage.getItem("embeddedx.mcxc162.anomalyAlarm");
if (savedLock === "false" || savedLock === "true") state.intervalLocked = savedLock === "true";
if (savedTrainingLock === "false" || savedTrainingLock === "true") state.trainingLocked = savedTrainingLock === "true";
if (savedAnomalyAlert === "false" || savedAnomalyAlert === "true") state.anomalyAlert = savedAnomalyAlert === "true";
if (savedAnomalyAlarm === "false" || savedAnomalyAlarm === "true") state.anomalyAlarm = savedAnomalyAlarm === "true";
const savedRateIndex = RECORD_RATES.indexOf(Number(savedRateValue));
elements.rate.value = Number.isInteger(savedRateIndex) && savedRateIndex >= 0 && savedRateIndex < RECORD_RATES.length
  ? String(savedRateIndex)
  : "2";
elements.rateLabel.textContent = formatRate(selectedRate());
selectTrainingSamples(state.trainingSamples);
updateRecorderUi();
updateControlPills();
drawChart();
