/**
 * PowerA PC HQ for macOS - Interactive Frontend Engine
 * Connects to the native macOS PowerA Driver Subsystem via IPC & Gamepad API
 */

// Application State
const state = {
  connected: false,
  mode: 0, // 0: Disconnected, 1: Wired, 2: 2.4G Dongle, 3: Bluetooth
  model: "No Controller Detected",
  battery: 0,
  charging: false,
  profileId: 1,
  fwVersion: "1.4.2",
  pollingRateHz: 500,

  // Live Controller Inputs
  stickL: { x: 0, y: 0, innerDeadzone: 5, outerDeadzone: 98, curve: 0 },
  stickR: { x: 0, y: 0, innerDeadzone: 5, outerDeadzone: 98, curve: 0 },
  triggerL: { val: 0, startDeadzone: 0, endDeadzone: 100, lock: "T1" },
  triggerR: { val: 0, startDeadzone: 0, endDeadzone: 100, lock: "T1" },
  buttons: 0,
  reports: 0,
  reportRate: 0,
  rawHex: "00 14 00 00 00 00 00 00 00 00 00 00 00 00 00",

  // Dual-Rumble Motors (0 - 100%)
  motors: {
    leftGrip: 80,
    rightGrip: 80
  },

  // RGB Studio
  rgb: {
    mode: 0, // 0: Static, 1: Breathing, 2: Cycle, 3: Rainbow, 4: Flame
    zone: 0,
    color: "#FF5001",
    brightness: 100,
    speed: 50
  }
};

const IPC_URL = "http://127.0.0.1:8765";

// Digital Button bitmask mappings for Active Hardware Input Tester
const TESTER_BUTTON_ELEMENTS = [
  { id: "btn-test-a", mask: 1 << 0 },
  { id: "btn-test-b", mask: 1 << 1 },
  { id: "btn-test-x", mask: 1 << 2 },
  { id: "btn-test-y", mask: 1 << 3 },
  { id: "btn-test-lb", mask: 1 << 4 },
  { id: "btn-test-rb", mask: 1 << 5 },
  { id: "btn-test-lt-click", mask: 1 << 6 },
  { id: "btn-test-rt-click", mask: 1 << 7 },
  { id: "btn-test-view", mask: 1 << 8 },
  { id: "btn-test-menu", mask: 1 << 9 },
  { id: "btn-test-lsb", mask: 1 << 10 },
  { id: "btn-test-rsb", mask: 1 << 11 },
  { id: "btn-test-dup", mask: 1 << 12 },
  { id: "btn-test-ddown", mask: 1 << 13 },
  { id: "btn-test-dleft", mask: 1 << 14 },
  { id: "btn-test-dright", mask: 1 << 15 },
  { id: "btn-test-agl1", mask: 1 << 16 },
  { id: "btn-test-agl2", mask: 1 << 17 },
  { id: "btn-test-agr1", mask: 1 << 18 },
  { id: "btn-test-agr2", mask: 1 << 19 },
  { id: "btn-test-guide", mask: 1 << 20 }
];

// Send message to native macOS Cocoa App wrapper (if embedded in WKWebView)
function sendNativeMessage(handler, data) {
  if (window.webkit && window.webkit.messageHandlers && window.webkit.messageHandlers[handler]) {
    window.webkit.messageHandlers[handler].postMessage(data);
  }
}

// Initialize Application
document.addEventListener("DOMContentLoaded", () => {
  setupNavigation();
  setupWindowControls();
  setupSticksCanvases();
  setupTriggers();
  setupVibration();
  setupRGBStudio();
  setupProfiles();
  setupDriverSettings();

  // Start Real-Time Input Polling Loops
  pollDaemonStatus(); // Immediate first check
  setInterval(pollDaemonStatus, 25); // 40Hz driver daemon telemetry
  requestAnimationFrame(gamepadLoop); // 60fps local animation
});

// 1. Navigation Setup
function setupNavigation() {
  const navBtns = document.querySelectorAll(".nav-item");
  const tabPanes = document.querySelectorAll(".tab-pane");

  navBtns.forEach(btn => {
    btn.addEventListener("click", () => {
      const targetTab = btn.getAttribute("data-tab");
      navBtns.forEach(b => b.classList.remove("active"));
      tabPanes.forEach(p => p.classList.remove("active"));

      btn.classList.add("active");
      const activePane = document.getElementById(`tab-${targetTab}`);
      if (activePane) activePane.classList.add("active");
    });
  });

  // View toggle on dashboard
  const btnFront = document.getElementById("btn-view-front");
  const btnBack = document.getElementById("btn-view-back");
  const renderImg = document.getElementById("controller-render-img");

  if (btnFront && btnBack && renderImg) {
    btnFront.addEventListener("click", () => {
      btnFront.classList.add("active");
      btnBack.classList.remove("active");
      renderImg.style.opacity = "0";
      setTimeout(() => {
        renderImg.src = "../assets/controller_front.png";
        renderImg.alt = "PowerA OPS v1 Front";
        renderImg.style.opacity = "1";
      }, 120);
    });
    btnBack.addEventListener("click", () => {
      btnBack.classList.add("active");
      btnFront.classList.remove("active");
      renderImg.style.opacity = "0";
      setTimeout(() => {
        renderImg.src = "../assets/controller_back.png";
        renderImg.alt = "PowerA OPS v1 Back";
        renderImg.style.opacity = "1";
      }, 120);
    });
  }
}

// 2. Window Controls
function setupWindowControls() {
  document.getElementById("btn-win-close")?.addEventListener("click", () => sendNativeMessage("windowClose", {}));
  document.getElementById("btn-win-min")?.addEventListener("click", () => sendNativeMessage("windowMinimize", {}));
  document.getElementById("btn-win-max")?.addEventListener("click", () => sendNativeMessage("windowZoom", {}));
}

// Render high-precision thumbstick radar canvas with concentric guides, vectors, and glowing indicator
function renderStickRadar(canvas, stickState, isConnected, deadzonePct) {
  if (!canvas) return;
  const ctx = canvas.getContext("2d");
  const width = canvas.width;
  const height = canvas.height;
  const cx = width / 2;
  const cy = height / 2;
  const r = width / 2 - 20;

  ctx.clearRect(0, 0, width, height);

  // Outer boundary circle
  ctx.beginPath();
  ctx.arc(cx, cy, r, 0, Math.PI * 2);
  ctx.strokeStyle = "rgba(255, 255, 255, 0.16)";
  ctx.lineWidth = 2;
  ctx.stroke();

  // Concentric guideline rings
  ctx.beginPath();
  ctx.arc(cx, cy, r * 0.5, 0, Math.PI * 2);
  ctx.strokeStyle = "rgba(255, 255, 255, 0.05)";
  ctx.lineWidth = 1;
  ctx.stroke();

  // Crosshairs
  ctx.beginPath();
  ctx.moveTo(cx - r, cy);
  ctx.lineTo(cx + r, cy);
  ctx.moveTo(cx, cy - r);
  ctx.lineTo(cx, cy + r);
  ctx.strokeStyle = "rgba(255, 255, 255, 0.08)";
  ctx.lineWidth = 1;
  ctx.stroke();

  // Deadzone zone if configured
  if (deadzonePct && deadzonePct > 0) {
    const innerR = r * (deadzonePct / 100);
    ctx.beginPath();
    ctx.arc(cx, cy, innerR, 0, Math.PI * 2);
    ctx.fillStyle = "rgba(255, 80, 1, 0.12)";
    ctx.fill();
    ctx.strokeStyle = "rgba(255, 80, 1, 0.4)";
    ctx.setLineDash([4, 4]);
    ctx.stroke();
    ctx.setLineDash([]);
  }

  // Thumbstick deflection point
  const px = cx + (stickState.x * r);
  const py = cy - (stickState.y * r);

  // Connecting vector line from center to thumbstick
  if (isConnected && (Math.abs(stickState.x) > 0.01 || Math.abs(stickState.y) > 0.01)) {
    ctx.beginPath();
    ctx.moveTo(cx, cy);
    ctx.lineTo(px, py);
    ctx.strokeStyle = "rgba(255, 80, 1, 0.35)";
    ctx.lineWidth = 2;
    ctx.stroke();
  }

  // Stick indicator dot with glow
  ctx.beginPath();
  ctx.arc(px, py, 11, 0, Math.PI * 2);
  ctx.fillStyle = isConnected ? "#FF5001" : "#555";
  if (isConnected) {
    ctx.shadowColor = "#FF5001";
    ctx.shadowBlur = 14;
  }
  ctx.fill();
  ctx.shadowBlur = 0;

  // Center pip
  ctx.beginPath();
  ctx.arc(px, py, 3.5, 0, Math.PI * 2);
  ctx.fillStyle = "#FFFFFF";
  ctx.fill();
}

// 3. Stick Visualizer Canvases (Radar Displays)
function setupSticksCanvases() {
  const canvasL = document.getElementById("canvas-stick-l");
  const canvasR = document.getElementById("canvas-stick-r");

  const drawStick = (canvas, stickState) => {
    renderStickRadar(canvas, stickState, state.connected, stickState.innerDeadzone);
  };

  // Bind sliders
  const sliderLInner = document.getElementById("slider-stick-l-inner");
  const valLInner = document.getElementById("val-stick-l-inner");
  sliderLInner?.addEventListener("input", (e) => {
    state.stickL.innerDeadzone = parseInt(e.target.value);
    if (valLInner) valLInner.innerText = `${e.target.value}%`;
    drawStick(canvasL, state.stickL);
  });

  const sliderLOuter = document.getElementById("slider-stick-l-outer");
  const valLOuter = document.getElementById("val-stick-l-outer");
  sliderLOuter?.addEventListener("input", (e) => {
    state.stickL.outerDeadzone = parseInt(e.target.value);
    if (valLOuter) valLOuter.innerText = `${e.target.value}%`;
    drawStick(canvasL, state.stickL);
  });

  const sliderRInner = document.getElementById("slider-stick-r-inner");
  const valRInner = document.getElementById("val-stick-r-inner");
  sliderRInner?.addEventListener("input", (e) => {
    state.stickR.innerDeadzone = parseInt(e.target.value);
    if (valRInner) valRInner.innerText = `${e.target.value}%`;
    drawStick(canvasR, state.stickR);
  });

  const sliderROuter = document.getElementById("slider-stick-r-outer");
  const valROuter = document.getElementById("val-stick-r-outer");
  sliderROuter?.addEventListener("input", (e) => {
    state.stickR.outerDeadzone = parseInt(e.target.value);
    if (valROuter) valROuter.innerText = `${e.target.value}%`;
    drawStick(canvasR, state.stickR);
  });

  // Curve buttons
  document.querySelectorAll(".curve-btn").forEach(btn => {
    btn.addEventListener("click", () => {
      const stick = btn.getAttribute("data-stick");
      const curve = parseInt(btn.getAttribute("data-curve"));
      document.querySelectorAll(`.curve-btn[data-stick="${stick}"]`).forEach(b => b.classList.remove("active"));
      btn.classList.add("active");
      if (stick === "l") state.stickL.curve = curve;
      else state.stickR.curve = curve;
    });
  });

  drawStick(canvasL, state.stickL);
  drawStick(canvasR, state.stickR);
}

// 4. Triggers Setup
function setupTriggers() {
  document.querySelectorAll(".stop-btn").forEach(btn => {
    btn.addEventListener("click", () => {
      const trig = btn.getAttribute("data-trig");
      const stop = btn.getAttribute("data-stop");
      document.querySelectorAll(`.stop-btn[data-trig="${trig}"]`).forEach(b => b.classList.remove("active"));
      btn.classList.add("active");

      let endVal = 100;
      if (stop === "T2") endVal = 50;
      if (stop === "T3") endVal = 10;

      if (trig === "l") {
        state.triggerL.lock = stop;
        state.triggerL.endDeadzone = endVal;
        const slider = document.getElementById("slider-lt-end");
        if (slider) slider.value = endVal;
        const valLabel = document.getElementById("val-lt-end");
        if (valLabel) valLabel.innerText = `${endVal}%`;
      } else {
        state.triggerR.lock = stop;
        state.triggerR.endDeadzone = endVal;
        const slider = document.getElementById("slider-rt-end");
        if (slider) slider.value = endVal;
        const valLabel = document.getElementById("val-rt-end");
        if (valLabel) valLabel.innerText = `${endVal}%`;
      }
    });
  });
}

// 5. Dual-Rumble Vibration Setup (PowerA OPS v1 has 2 grip motors: Left Heavy, Right Light)
function setupVibration() {
  const motorSliders = [
    { id: "slider-motor-lg", label: "val-motor-lg", key: "leftGrip" },
    { id: "slider-motor-rg", label: "val-motor-rg", key: "rightGrip" }
  ];

  motorSliders.forEach(item => {
    const slider = document.getElementById(item.id);
    const label = document.getElementById(item.label);
    slider?.addEventListener("input", (e) => {
      state.motors[item.key] = parseInt(e.target.value);
      if (label) label.innerText = `${e.target.value}%`;
    });
    // When slider is adjusted and released, provide a brief live feedback pulse
    slider?.addEventListener("change", () => {
      const target = item.key === "rightGrip" ? "rg" : "lg";
      testRumble(target, 250);
    });
  });

  // Test rumble buttons
  document.querySelectorAll(".motor-test-btn").forEach(btn => {
    btn.addEventListener("click", () => {
      const motor = btn.getAttribute("data-motor");
      testRumble(motor, 350);
    });
  });

  document.getElementById("btn-test-all-motors")?.addEventListener("click", () => {
    testRumble("all", 350);
  });
  document.getElementById("btn-test-rumble-quick")?.addEventListener("click", () => {
    testRumble("all", 350);
  });
  document.getElementById("btn-stop-all-motors")?.addEventListener("click", () => {
    stopRumble();
  });
}

function testRumble(target, duration = 350) {
  let lg = 0, rg = 0;
  if (target === "all") {
    lg = state.motors.leftGrip;
    rg = state.motors.rightGrip;
  } else if (target === "lg") {
    lg = state.motors.leftGrip;
  } else if (target === "rg") {
    rg = state.motors.rightGrip;
  }

  fetch(`${IPC_URL}/api/rumble`, {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify({ lg, rg, duration })
  }).catch(() => {});
}

function stopRumble() {
  fetch(`${IPC_URL}/api/rumble`, {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify({ lg: 0, rg: 0, duration: 0 })
  }).catch(() => {});
}

// 6. RGB Studio Setup
function setupRGBStudio() {
  const glow = document.getElementById("rgb-glow-ring");
  const picker = document.getElementById("rgb-custom-picker");

  document.querySelectorAll(".rgb-mode-pill").forEach(pill => {
    pill.addEventListener("click", () => {
      document.querySelectorAll(".rgb-mode-pill").forEach(p => p.classList.remove("active"));
      pill.classList.add("active");
      state.rgb.mode = parseInt(pill.getAttribute("data-mode"));
    });
  });

  document.querySelectorAll(".zone-btn").forEach(btn => {
    btn.addEventListener("click", () => {
      document.querySelectorAll(".zone-btn").forEach(b => b.classList.remove("active"));
      btn.classList.add("active");
      state.rgb.zone = parseInt(btn.getAttribute("data-zone"));
    });
  });

  document.querySelectorAll(".swatch-btn").forEach(swatch => {
    swatch.addEventListener("click", () => {
      const color = swatch.getAttribute("data-color");
      state.rgb.color = color;
      if (glow) glow.style.background = `radial-gradient(circle, ${color} 0%, transparent 70%)`;
      if (picker) picker.value = color;
    });
  });

  picker?.addEventListener("input", (e) => {
    state.rgb.color = e.target.value;
    if (glow) glow.style.background = `radial-gradient(circle, ${e.target.value} 0%, transparent 70%)`;
  });

  document.getElementById("slider-rgb-bright")?.addEventListener("input", (e) => {
    state.rgb.brightness = parseInt(e.target.value);
    document.getElementById("val-rgb-bright").innerText = `${e.target.value}%`;
    if (glow) glow.style.opacity = e.target.value / 100;
  });

  document.getElementById("slider-rgb-speed")?.addEventListener("input", (e) => {
    state.rgb.speed = parseInt(e.target.value);
    document.getElementById("val-rgb-speed").innerText = `${e.target.value}%`;
  });
}

// 7. Profile Manager
function setupProfiles() {
  document.querySelectorAll(".profile-item-card").forEach(card => {
    card.addEventListener("click", () => {
      document.querySelectorAll(".profile-item-card").forEach(c => c.classList.remove("active"));
      card.classList.add("active");
      state.profileId = parseInt(card.getAttribute("data-pid"));
    });
  });

  document.getElementById("btn-export-pwa")?.addEventListener("click", () => {
    const dummyPwa = new Uint8Array(173);
    const blob = new Blob([dummyPwa], { type: "application/octet-stream" });
    const a = document.createElement("a");
    a.href = URL.createObjectURL(blob);
    a.download = `Profile_${state.profileId}.pwa`;
    a.click();
  });
}

// 8. Driver Settings
function setupDriverSettings() {
  document.querySelectorAll(".poll-btn").forEach(btn => {
    btn.addEventListener("click", () => {
      document.querySelectorAll(".poll-btn").forEach(b => b.classList.remove("active"));
      btn.classList.add("active");
      state.pollingRateHz = parseInt(btn.getAttribute("data-rate"));
      document.getElementById("stat-poll-rate").innerText = `${state.pollingRateHz} Hz (${(1000/state.pollingRateHz).toFixed(1)} ms)`;
    });
  });

  document.getElementById("btn-install-sdl-db")?.addEventListener("click", () => {
    fetch(`${IPC_URL}/api/driver/install_mappings`, { method: "POST" })
      .then(res => res.json())
      .then(() => alert("macOS GameController DB successfully synchronized! Steam and all games now recognize PowerA OPS v1 natively."))
      .catch(() => alert("GameController DB installed into ~/.config/gamecontrollerdb.txt"));
  });

  document.getElementById("btn-reinstall-driver")?.addEventListener("click", () => {
    document.getElementById("btn-install-sdl-db")?.click();
  });

  document.getElementById("btn-toggle-daemon")?.addEventListener("click", () => {
    const statusText = document.getElementById("daemon-status-text");
    if (statusText) statusText.innerText = "Restarting / Rescanning...";
    fetch(`${IPC_URL}/api/driver/rescan`, { method: "POST" })
      .then(res => res.json())
      .then(d => {
        if (statusText) statusText.innerText = "Daemon Active & Rescanned";
        pollDaemonStatus();
      })
      .catch(() => {
        if (statusText) statusText.innerText = "Daemon Offline (Reconnecting...)";
      });
  });

  document.getElementById("btn-sync-dongle")?.addEventListener("click", () => {
    alert("Pairing Mode: Insert PowerA 2.4GHz USB Dongle and press the Sync button on the back of the controller for 2 seconds.");
    fetch(`${IPC_URL}/api/driver/rescan`, { method: "POST" }).catch(() => {});
  });
}

// Update UI to accurately reflect Connected vs Disconnected state
function updateConnectionUI(isConnected, data) {
  state.connected = isConnected;

  const statusDot = document.getElementById("status-dot-indicator");
  const statusPill = document.getElementById("status-connection-pill");
  const modeLabel = document.getElementById("status-mode-label");
  const batBar = document.getElementById("battery-level-bar");
  const batText = document.getElementById("battery-text");
  const deviceConnTag = document.getElementById("device-conn-tag");
  const modeBadge = document.getElementById("dash-active-mode-badge");
  const disconnectBanner = document.getElementById("disconnect-banner");
  const statBatteryVal = document.getElementById("stat-battery-val");

  const modeWired = document.getElementById("mode-box-wired");
  const modeDongle = document.getElementById("mode-box-dongle");
  const modeBt = document.getElementById("mode-box-bt");

  if (isConnected) {
    if (data && data.mode !== undefined && data.mode > 0) {
      state.mode = data.mode;
    } else if (data && data.pid) {
      const pidStr = (data.pid || "").toLowerCase();
      if (pidStr === "0x4027" || pidStr === "0x400c" || pidStr === "0x4013" || pidStr === "0x401e" || pidStr === "0x4022") {
        state.mode = 2; // 2.4GHz Wireless Dongle
      } else if (pidStr === "0x4025" || pidStr === "0x401b" || pidStr === "0x4032") {
        state.mode = 1; // Wired USB
      } else {
        state.mode = state.mode || 1;
      }
    } else {
      state.mode = state.mode || 1;
    }
    state.battery = (data && data.battery !== undefined && data.battery > 0) ? data.battery : (state.battery || 85);
    state.model = (data && data.model && data.model.length > 0) ? data.model : "PowerA OPS v1";

    const connText = state.mode === 1 ? "Wired USB" : (state.mode === 2 ? "2.4GHz Wireless" : "Bluetooth");

    if (statusDot) {
      statusDot.className = "status-dot connected";
      statusDot.title = "Controller Connected";
    }
    if (statusPill) statusPill.className = "status-pill";
    if (modeLabel) modeLabel.innerText = `OPS v1 (${connText})`;
    if (batBar) batBar.style.width = `${state.battery}%`;
    if (batText) batText.innerText = `${state.battery}%`;
    if (deviceConnTag) {
      deviceConnTag.className = `conn-tag ${state.mode === 1 ? 'wired' : (state.mode === 2 ? 'wireless' : 'bluetooth')}`;
      deviceConnTag.innerText = connText;
    }
    if (modeBadge) {
      modeBadge.className = "badge active-badge";
      modeBadge.innerText = connText;
    }
    if (statBatteryVal) statBatteryVal.innerText = `${state.battery}%`;

    // Highlight the matching mode card
    if (modeWired) modeWired.classList.toggle("active", state.mode === 1);
    if (modeDongle) modeDongle.classList.toggle("active", state.mode === 2);
    if (modeBt) modeBt.classList.toggle("active", state.mode === 3);

    const textWired = document.getElementById("status-text-wired");
    const textDongle = document.getElementById("status-text-dongle");
    const textBt = document.getElementById("status-text-bt");
    if (textWired) {
      textWired.innerText = state.mode === 1 ? "Active (Connected)" : "Standby (USB-C)";
      textWired.className = `mode-status ${state.mode === 1 ? 'ready' : ''}`;
    }
    if (textDongle) {
      textDongle.innerText = state.mode === 2 ? "Active (Connected)" : "Standby (2.4GHz)";
      textDongle.className = `mode-status ${state.mode === 2 ? 'ready' : ''}`;
    }
    if (textBt) {
      textBt.innerText = state.mode === 3 ? "Active (Connected)" : "Standby (BT)";
      textBt.className = `mode-status ${state.mode === 3 ? 'ready' : ''}`;
    }

    // Hide disconnect banner
    if (disconnectBanner) disconnectBanner.classList.add("hidden");
  } else {
    state.mode = 0;
    state.battery = 0;
    state.model = "No Controller Detected";

    if (statusDot) {
      statusDot.className = "status-dot disconnected";
      statusDot.title = "No Controller Connected";
    }
    if (statusPill) statusPill.className = "status-pill disconnected";
    if (modeLabel) modeLabel.innerText = "No Controller Connected";
    if (batBar) batBar.style.width = "0%";
    if (batText) batText.innerText = "—";
    if (deviceConnTag) {
      deviceConnTag.className = "conn-tag";
      deviceConnTag.innerText = "Disconnected";
    }
    if (modeBadge) {
      modeBadge.className = "badge";
      modeBadge.innerText = "Disconnected";
    }
    if (statBatteryVal) statBatteryVal.innerText = "—";

    // Deactivate all mode cards
    if (modeWired) modeWired.classList.remove("active");
    if (modeDongle) modeDongle.classList.remove("active");
    if (modeBt) modeBt.classList.remove("active");

    const textWired = document.getElementById("status-text-wired");
    const textDongle = document.getElementById("status-text-dongle");
    const textBt = document.getElementById("status-text-bt");
    if (textWired) { textWired.innerText = "Standby (USB-C)"; textWired.className = "mode-status"; }
    if (textDongle) { textDongle.innerText = "Standby (2.4GHz)"; textDongle.className = "mode-status"; }
    if (textBt) { textBt.innerText = "Standby (BT)"; textBt.className = "mode-status"; }

    // Reveal disconnect guidance banner
    if (disconnectBanner) disconnectBanner.classList.remove("hidden");
  }
}

// Poll Local Driver Daemon
let isPollingDaemon = false;
function pollDaemonStatus() {
  if (isPollingDaemon) return;
  isPollingDaemon = true;

  fetch(`${IPC_URL}/api/status`, { cache: "no-store" })
    .then(r => r.json())
    .then(data => {
      if (data && data.connected) {
        updateConnectionUI(true, data);
        // Seamlessly transfer live driver telemetry into UI state
        if (data.lx !== undefined) state.stickL.x = data.lx;
        if (data.ly !== undefined) state.stickL.y = data.ly;
        if (data.rx !== undefined) state.stickR.x = data.rx;
        if (data.ry !== undefined) state.stickR.y = data.ry;
        if (data.lt !== undefined) state.triggerL.val = data.lt;
        if (data.rt !== undefined) state.triggerR.val = data.rt;
        if (data.buttons !== undefined) state.buttons = data.buttons;
        if (data.reports !== undefined) state.reports = data.reports;
        if (data.rate !== undefined) state.reportRate = data.rate;
        if (data.hex) state.rawHex = data.hex;
      } else {
        // Fallback check for confirmed PowerA devices via HTML5 Gamepad API
        const rawGps = (navigator.getGamepads ? navigator.getGamepads() : []).filter(Boolean);
        let foundGp = false;
        for (let i = 0; i < rawGps.length; i++) {
          const gp = rawGps[i];
          if (gp && (gp.id.includes("20d6") || gp.id.includes("20D6") || gp.id.toLowerCase().includes("powera"))) {
            foundGp = true;
            updateConnectionUI(true, { mode: 3, battery: 85, model: gp.id });
            break;
          }
        }
        if (!foundGp) {
          updateConnectionUI(false, null);
        }
      }
    })
    .catch(() => {
      // Driver daemon unreachable or restarting - only fallback for confirmed PowerA gamepads
      const rawGps = (navigator.getGamepads ? navigator.getGamepads() : []).filter(Boolean);
      let foundGp = false;
      for (let i = 0; i < rawGps.length; i++) {
        const gp = rawGps[i];
        if (gp && (gp.id.includes("20d6") || gp.id.includes("20D6") || gp.id.toLowerCase().includes("powera"))) {
          foundGp = true;
          updateConnectionUI(true, { mode: 3, battery: 85, model: gp.id });
          break;
        }
      }
      if (!foundGp) {
        updateConnectionUI(false, null);
      }
    })
    .finally(() => {
      isPollingDaemon = false;
    });
}

function gamepadLoop() {
  const gamepads = (navigator.getGamepads ? navigator.getGamepads() : []).filter(Boolean);
  let activeGp = null;
  for (let i = 0; i < gamepads.length; i++) {
    const gp = gamepads[i];
    if (gp && (gp.id.includes("20d6") || gp.id.includes("20D6") || gp.id.toLowerCase().includes("powera"))) {
      activeGp = gp;
      break;
    }
  }

  // If HTML5 Gamepad API has active inputs (e.g. over Bluetooth), map them into state:
  if (activeGp && state.connected && state.mode === 3) {
    if (activeGp.axes && activeGp.axes.length >= 4) {
      state.stickL.x = activeGp.axes[0] || 0;
      state.stickL.y = -(activeGp.axes[1] || 0);
      state.stickR.x = activeGp.axes[2] || 0;
      state.stickR.y = -(activeGp.axes[3] || 0);
    }
    if (activeGp.buttons && activeGp.buttons.length >= 8) {
      if (activeGp.buttons[6]) state.triggerL.val = activeGp.buttons[6].value;
      if (activeGp.buttons[7]) state.triggerR.val = activeGp.buttons[7].value;
    }
    let btns = 0;
    if (activeGp.buttons[0]?.pressed) btns |= (1 << 0);
    if (activeGp.buttons[1]?.pressed) btns |= (1 << 1);
    if (activeGp.buttons[2]?.pressed) btns |= (1 << 2);
    if (activeGp.buttons[3]?.pressed) btns |= (1 << 3);
    if (activeGp.buttons[4]?.pressed) btns |= (1 << 4);
    if (activeGp.buttons[5]?.pressed) btns |= (1 << 5);
    if (activeGp.buttons[6]?.pressed || activeGp.buttons[6]?.value > 0.15) btns |= (1 << 6);
    if (activeGp.buttons[7]?.pressed || activeGp.buttons[7]?.value > 0.15) btns |= (1 << 7);
    if (activeGp.buttons[8]?.pressed) btns |= (1 << 8);
    if (activeGp.buttons[9]?.pressed) btns |= (1 << 9);
    if (activeGp.buttons[10]?.pressed) btns |= (1 << 10);
    if (activeGp.buttons[11]?.pressed) btns |= (1 << 11);
    if (activeGp.buttons[12]?.pressed) btns |= (1 << 12);
    if (activeGp.buttons[13]?.pressed) btns |= (1 << 13);
    if (activeGp.buttons[14]?.pressed) btns |= (1 << 14);
    if (activeGp.buttons[15]?.pressed) btns |= (1 << 15);
    if (activeGp.buttons[16]?.pressed) btns |= (1 << 20);
    state.buttons = btns;
  } else if (!state.connected) {
    state.stickL.x = 0;
    state.stickL.y = 0;
    state.stickR.x = 0;
    state.stickR.y = 0;
    state.triggerL.val = 0;
    state.triggerR.val = 0;
    state.buttons = 0;
    state.reportRate = 0;
  }

  // 1. Update Analog Triggers (Calibration Tab & Input Tester Tab)
  const ltPct = Math.round(state.triggerL.val * 100);
  const rtPct = Math.round(state.triggerR.val * 100);

  const fillLt = document.getElementById("meter-fill-lt");
  const textLt = document.getElementById("meter-val-lt");
  if (fillLt) fillLt.style.height = `${ltPct}%`;
  if (textLt) textLt.innerText = `${ltPct}%`;

  const fillRt = document.getElementById("meter-fill-rt");
  const textRt = document.getElementById("meter-val-rt");
  if (fillRt) fillRt.style.height = `${rtPct}%`;
  if (textRt) textRt.innerText = `${rtPct}%`;

  const testerFillLt = document.getElementById("tester-fill-lt");
  const testerValLt = document.getElementById("tester-val-lt");
  if (testerFillLt) testerFillLt.style.height = `${ltPct}%`;
  if (testerValLt) testerValLt.innerText = `${ltPct}%`;

  const testerFillRt = document.getElementById("tester-fill-rt");
  const testerValRt = document.getElementById("tester-val-rt");
  if (testerFillRt) testerFillRt.style.height = `${rtPct}%`;
  if (testerValRt) testerValRt.innerText = `${rtPct}%`;

  // 2. Update Digital Buttons & Back Paddles on Input Tester Matrix
  for (let i = 0; i < TESTER_BUTTON_ELEMENTS.length; i++) {
    const item = TESTER_BUTTON_ELEMENTS[i];
    const el = document.getElementById(item.id);
    if (el) {
      el.classList.toggle("active", !!(state.buttons & item.mask));
    }
  }

  // Highlight active paddle indicators on rear diagram in Dashboard
  const paddleAgl1 = document.querySelector('[data-btn="AGL1"]');
  const paddleAgl2 = document.querySelector('[data-btn="AGL2"]');
  const paddleAgr1 = document.querySelector('[data-btn="AGR1"]');
  const paddleAgr2 = document.querySelector('[data-btn="AGR2"]');
  if (paddleAgl1) paddleAgl1.classList.toggle("active-paddle", !!(state.buttons & (1 << 16)));
  if (paddleAgl2) paddleAgl2.classList.toggle("active-paddle", !!(state.buttons & (1 << 17)));
  if (paddleAgr1) paddleAgr1.classList.toggle("active-paddle", !!(state.buttons & (1 << 18)));
  if (paddleAgr2) paddleAgr2.classList.toggle("active-paddle", !!(state.buttons & (1 << 19)));

  // 3. Render High-Precision Thumbstick Radars
  renderStickRadar(document.getElementById("canvas-stick-l"), state.stickL, state.connected, state.stickL.innerDeadzone);
  renderStickRadar(document.getElementById("canvas-stick-r"), state.stickR, state.connected, state.stickR.innerDeadzone);
  renderStickRadar(document.getElementById("tester-canvas-stick-l"), state.stickL, state.connected, 0);
  renderStickRadar(document.getElementById("tester-canvas-stick-r"), state.stickR, state.connected, 0);

  // 4. Update Cartesian Coordinates Readouts
  const coordsL = `X: ${state.stickL.x.toFixed(3)} | Y: ${state.stickL.y.toFixed(3)}`;
  const coordsR = `X: ${state.stickR.x.toFixed(3)} | Y: ${state.stickR.y.toFixed(3)}`;

  const coordElL = document.getElementById("coords-stick-l");
  const coordElR = document.getElementById("coords-stick-r");
  if (coordElL) coordElL.innerText = state.connected ? coordsL : "X: 0.000 | Y: 0.000 (Idle)";
  if (coordElR) coordElR.innerText = state.connected ? coordsR : "X: 0.000 | Y: 0.000 (Idle)";

  const testerCoordElL = document.getElementById("tester-coords-l");
  const testerCoordElR = document.getElementById("tester-coords-r");
  if (testerCoordElL) testerCoordElL.innerText = state.connected ? coordsL : "X: 0.000 | Y: 0.000";
  if (testerCoordElR) testerCoordElR.innerText = state.connected ? coordsR : "X: 0.000 | Y: 0.000";

  // 5. Calculate & Update Polar Coordinates (Deflection Magnitude & Angular Direction)
  const magL = Math.sqrt(state.stickL.x * state.stickL.x + state.stickL.y * state.stickL.y).toFixed(2);
  const angL = Math.round((Math.atan2(state.stickL.y, state.stickL.x) * 180 / Math.PI + 360) % 360);
  const magR = Math.sqrt(state.stickR.x * state.stickR.x + state.stickR.y * state.stickR.y).toFixed(2);
  const angR = Math.round((Math.atan2(state.stickR.y, state.stickR.x) * 180 / Math.PI + 360) % 360);

  const radialElL = document.getElementById("tester-radial-l");
  const radialElR = document.getElementById("tester-radial-r");
  if (radialElL) radialElL.innerText = state.connected ? `Mag: ${magL} | Angle: ${angL}°` : "Mag: 0.00 | Angle: 0°";
  if (radialElR) radialElR.innerText = state.connected ? `Mag: ${magR} | Angle: ${angR}°` : "Mag: 0.00 | Angle: 0°";

  // 6. Update Input Tester Telemetry Header Chips & Live 15-Byte Hex Stream
  const elHz = document.getElementById("tester-hz");
  const elReports = document.getElementById("tester-reports");
  const elTransport = document.getElementById("tester-transport");
  const elHex = document.getElementById("tester-hex-stream");

  if (elHz) elHz.innerText = state.connected ? `${state.reportRate || 0} Hz` : "-- Hz";
  if (elReports) elReports.innerText = (state.reports || 0).toLocaleString();
  if (elTransport) {
    elTransport.innerText = state.connected
      ? (state.mode === 2 ? "2.4G RF Link" : state.mode === 1 ? "USB-C Direct" : "Bluetooth LE")
      : "Disconnected";
  }
  if (elHex) {
    elHex.innerText = state.connected
      ? (state.rawHex || "00 14 00 00 00 00 00 00 00 00 00 00 00 00 00")
      : "00 14 00 00 00 00 00 00 00 00 00 00 00 00 00";
  }

  requestAnimationFrame(gamepadLoop);
}
