const PAGE_FACE = 0;
const PAGE_HEART = 1;
const PAGE_STEPS = 2;
const PAGE_APPS = 3;
const PAGE_COUNT = 4;
const STEP_GOAL_MIN = 1000;
const STEP_GOAL_MAX = 50000;
const STEP_GOAL_DELTA = 1000;
const ICON_ROOT = "../openvela_app/smart_band/assets/generated_icons";

const apps = [
  { id: "weather", title: "Weather", icon: "weather.png", color: "#f5c66e" },
  { id: "calculator", title: "Calculator", icon: "calculator.png", color: "#80cbc3" },
  { id: "timer", title: "Timer", icon: "timer.png", color: "#a98bd6" },
  { id: "game2048", title: "2048", icon: "game2048.png", color: "#f08d88" },
  { id: "stopwatch", title: "Stopwatch", icon: "stopwatch.png", color: "#73a1d6" },
  { id: "mines", title: "Mines", icon: "mines.png", color: "#8aa8d8" },
  { id: "tetris", title: "Tetris", icon: "tetris.png", color: "#62bfb6" },
  { id: "wooden", title: "Wooden Fish", icon: "wooden_fish.png", color: "#d9a85f" },
];

const state = {
  page: PAGE_FACE,
  activeApp: null,
  ticks: 0,
  heartRate: 72,
  steps: 4260,
  stepGoal: 8000,
  battery: 88,
  charging: true,
  sleepMinutes: 468,
  temperature: 29,
  stress: "Low",
  timerSeconds: 5 * 60,
  timerRunning: false,
  stopwatchSeconds: 0,
  stopwatchRunning: false,
  merits: 0,
  woodenMessage: "Tap gently",
  woodenFloat: "",
  lastTapAt: 0,
  calcDisplay: "0",
  calcStored: null,
  calcOp: null,
  calcFresh: true,
  board2048: [],
  score2048: 0,
  minesSize: 6,
  minesCount: 6,
  minesOpen: new Set(),
  minesFlags: new Set(),
  minesMap: new Set(),
  minesStatus: "Find safe cells",
  tetrisCells: new Set(),
  tetrisPiece: { x: 2, y: 0 },
};

const contentElement = document.getElementById("content");

function currentViewKey() {
  return state.activeApp ? `app:${state.activeApp}` : `page:${state.page}`;
}

function syncNode(current, next) {
  if (!current || !next || current.nodeType !== next.nodeType || current.nodeName !== next.nodeName) {
    current?.replaceWith(next?.cloneNode(true));
    return;
  }

  if (current.nodeType === Node.TEXT_NODE) {
    if (current.nodeValue !== next.nodeValue) current.nodeValue = next.nodeValue;
    return;
  }

  Array.from(current.attributes).forEach((attribute) => {
    if (!next.hasAttribute(attribute.name)) current.removeAttribute(attribute.name);
  });
  Array.from(next.attributes).forEach((attribute) => {
    if (current.getAttribute(attribute.name) !== attribute.value) {
      current.setAttribute(attribute.name, attribute.value);
    }
  });

  const currentChildren = Array.from(current.childNodes);
  const nextChildren = Array.from(next.childNodes);
  const length = Math.max(currentChildren.length, nextChildren.length);
  for (let index = length - 1; index >= 0; index -= 1) {
    const currentChild = current.childNodes[index];
    const nextChild = nextChildren[index];
    if (!nextChild && currentChild) currentChild.remove();
  }
  nextChildren.forEach((nextChild, index) => {
    const currentChild = current.childNodes[index];
    if (!currentChild) current.append(nextChild.cloneNode(true));
    else syncNode(currentChild, nextChild);
  });
}

function patchContent(target, html) {
  const viewKey = currentViewKey();
  if (target.dataset.view !== viewKey) {
    Reflect.set(target, "innerHTML", html, target);
    target.dataset.view = viewKey;
    return;
  }

  const template = document.createElement("template");
  template.innerHTML = html;
  const nextChildren = Array.from(template.content.childNodes);
  const currentChildren = Array.from(target.childNodes);
  const length = Math.max(currentChildren.length, nextChildren.length);
  for (let index = length - 1; index >= 0; index -= 1) {
    if (!nextChildren[index] && target.childNodes[index]) target.childNodes[index].remove();
  }
  nextChildren.forEach((nextChild, index) => {
    const currentChild = target.childNodes[index];
    if (!currentChild) target.append(nextChild.cloneNode(true));
    else syncNode(currentChild, nextChild);
  });
}

const contentProxy = new Proxy(contentElement, {
  get(target, property) {
    const value = Reflect.get(target, property, target);
    return typeof value === "function" ? value.bind(target) : value;
  },
  set(target, property, value) {
    if (property === "innerHTML") {
      patchContent(target, value);
      return true;
    }
    return Reflect.set(target, property, value, target);
  },
});

const el = {
  watch: document.getElementById("watch"),
  content: contentProxy,
  dots: Array.from(document.querySelectorAll(".dots span")),
  prevBtn: document.getElementById("prevBtn"),
  nextBtn: document.getElementById("nextBtn"),
  statusLive: document.getElementById("statusLive"),
};

function announce(message) {
  if (el.statusLive.textContent !== message) el.statusLive.textContent = message;
}

function clamp(value, min, max) {
  return Math.min(Math.max(value, min), max);
}

function two(value) {
  return String(value).padStart(2, "0");
}

function icon(name, className = "") {
  return `<img class="icon-img ${className}" src="${ICON_ROOT}/${name}" alt="" />`;
}

function nowParts() {
  const now = new Date();
  const weekdays = ["SUN", "MON", "TUE", "WED", "THU", "FRI", "SAT"];
  const months = ["JAN", "FEB", "MAR", "APR", "MAY", "JUN", "JUL", "AUG", "SEP", "OCT", "NOV", "DEC"];
  return {
    time: `${two(now.getHours())}:${two(now.getMinutes())}`,
    date: `${weekdays[now.getDay()]} ${two(now.getDate())} ${months[now.getMonth()]}`,
  };
}

function formatDuration(seconds) {
  return `${two(Math.floor(seconds / 60))}:${two(seconds % 60)}`;
}

function simulateHealthData() {
  const pulseWave = (state.ticks * 7 + 11) % 23;
  const motionWave = (state.ticks * 5 + 3) % 9;

  state.heartRate = clamp(66 + pulseWave + Math.floor(motionWave / 3), 55, 135);
  state.steps += 4 + (state.ticks % 6);
  if (state.steps > 99999) state.steps %= state.stepGoal;
  state.battery = clamp(88 - Math.floor(state.ticks / 240), 5, 100);
  state.charging = Math.floor(state.ticks / 20) % 2 === 0;
  state.temperature = 28 + (state.ticks % 5);
}

function stepProgress() {
  return clamp(Math.floor((state.steps * 100) / state.stepGoal), 0, 100);
}

function batteryMarkup() {
  return `
    <div class="battery" aria-label="battery ${state.battery}%">
      <span class="battery-shell"><span style="width:${state.battery}%"></span></span>
      ${state.charging ? '<span class="charge">⚡</span>' : ""}
      <strong>${state.battery}%</strong>
    </div>`;
}

function metricCard(kind, iconName, label, value) {
  return `
    <article class="metric-card ${kind}">
      <span class="icon-orb">${icon(iconName)}</span>
      <span class="divider" aria-hidden="true"></span>
      <div class="metric-copy">
        <p class="metric-label">${label}</p>
        <p class="metric-value">${value}</p>
      </div>
    </article>`;
}

function renderFace() {
  const now = nowParts();
  const [hour, minute] = now.time.split(":");
  const sleepHours = Math.floor(state.sleepMinutes / 60);
  const sleepMinutes = state.sleepMinutes % 60;

  el.content.innerHTML = `
    ${batteryMarkup()}
    <div class="leaf-mark" aria-hidden="true"><span></span><strong></strong><span></span></div>
    <div class="date-row">${now.date}</div>
    <div class="time-display" aria-label="current time ${now.time}">
      <span class="time-part">${hour}</span>
      <span class="colon"><span></span><span></span></span>
      <span class="time-part">${minute}</span>
    </div>
    <div class="ornament"><span>HR</span></div>
    <div class="cards">
      ${metricCard("sleep", "sleep.png", "Sleep", `${sleepHours}<small>h</small> ${sleepMinutes}<small>m</small>`)}
      ${metricCard("heart", "heart.png", "Heart Rate", `${state.heartRate}<small> bpm</small>`)}
      ${metricCard("stress", "stress.png", "Stress", state.stress)}
      ${metricCard("weather", "weather.png", "Weather", `${state.temperature}<small>°C</small>`)}
    </div>`;
}

function renderHeartPage() {
  const progress = clamp(Math.floor((state.heartRate * 100) / 160), 0, 100);

  el.content.innerHTML = `
    ${batteryMarkup()}
    <section class="detail-page">
      <h1 class="detail-title">Heart Rate</h1>
      <div class="detail-hero heart">
        <span class="hero-icon">${icon("heart.png")}</span>
        <p class="detail-number">${state.heartRate}<small> bpm</small></p>
        <p class="source">Sensor HR</p>
        <div class="progress"><span style="width:${progress}%"></span></div>
      </div>
      <div class="mini-grid">
        <div class="mini-card"><span>Resting</span><strong>62</strong></div>
        <div class="mini-card"><span>Status</span><strong>${state.heartRate > 110 ? "High" : "Good"}</strong></div>
        <div class="mini-card"><span>Battery</span><strong>${state.battery}%</strong></div>
        <div class="mini-card"><span>Stress</span><strong>${state.stress}</strong></div>
      </div>
    </section>`;
}

function renderStepsPage() {
  const progress = stepProgress();

  el.content.innerHTML = `
    ${batteryMarkup()}
    <section class="detail-page steps-page">
      <h1 class="detail-title">Activity</h1>
      <div class="detail-hero stress">
        <span class="hero-icon">${icon("steps.png")}</span>
        <p class="detail-number">${state.steps.toLocaleString("zh-CN")}</p>
        <p class="source">Sensor Steps</p>
        <div class="progress"><span style="width:${progress}%"></span></div>
      </div>
      <div class="goal-row">
        <button type="button" data-action="goalDown" aria-label="减少步数目标">-</button>
        <div><span>Goal</span><strong>${state.stepGoal.toLocaleString("zh-CN")}</strong></div>
        <button type="button" data-action="goalUp" aria-label="增加步数目标">+</button>
      </div>
      <div class="mini-grid">
        <div class="mini-card"><span>Progress</span><strong>${progress}%</strong></div>
        <div class="mini-card"><span>Remain</span><strong>${Math.max(0, state.stepGoal - state.steps).toLocaleString("zh-CN")}</strong></div>
      </div>
    </section>`;
}

function renderAppsPage() {
  el.content.innerHTML = `
    ${batteryMarkup()}
    <section class="apps-page">
      <h1 class="detail-title">Apps</h1>
      <div class="app-grid">
        ${apps.map((app) => `
          <button class="app-icon" type="button" data-app="${app.id}" style="--app-color:${app.color}">
            ${icon(app.icon)}
            <span>${app.title}</span>
          </button>`).join("")}
      </div>
    </section>`;
}

function appHeader(title) {
  return `
    <button class="back-button" type="button" data-action="back" aria-label="返回应用列表">‹</button>
    ${batteryMarkup()}
    <h1 class="app-title">${title}</h1>`;
}

function renderWeatherApp() {
  el.content.innerHTML = `
    ${appHeader("Weather")}
    <section class="weather-app">
      <div class="weather-temp">
        ${icon("weather.png")}
        <strong>${state.temperature}°C</strong>
        <span>ambient temp sensor</span>
      </div>
      <div class="mini-grid">
        <div class="mini-card"><span>Range</span><strong>${state.temperature - 1}/${state.temperature + 3}</strong></div>
        <div class="mini-card"><span>Wind</span><strong>E4</strong></div>
        <div class="mini-card"><span>Sky</span><strong>Cloudy</strong></div>
        <div class="mini-card"><span>Humidity</span><strong>60%</strong></div>
      </div>
    </section>`;
}

function renderCalculatorApp() {
  const buttons = ["C", "÷", "×", "⌫", "7", "8", "9", "-", "4", "5", "6", "+", "1", "2", "3", "=", "0", "."];

  el.content.innerHTML = `
    ${appHeader("Calculator")}
    <section class="calculator-app">
      <div class="calc-display">${state.calcDisplay}</div>
      <div class="calc-grid">
        ${buttons.map((item) => `<button type="button" data-calc="${item}" class="${item === "=" ? "equals" : ""}">${item}</button>`).join("")}
      </div>
    </section>`;
}

function renderTimerApp() {
  el.content.innerHTML = `
    ${appHeader("Timer")}
    <section class="simple-app timer-app">
      <div class="big-time">${formatDuration(state.timerSeconds)}</div>
      <p class="source">${state.timerRunning ? "Running" : state.timerSeconds === 0 ? "Done" : "Ready"}</p>
      <div class="button-row four">
        <button type="button" data-action="timerDown">-1m</button>
        <button type="button" data-action="timerUp">+1m</button>
        <button type="button" data-action="timerToggle">${state.timerRunning ? "Pause" : "Start"}</button>
        <button type="button" data-action="timerReset">Reset</button>
      </div>
    </section>`;
}

function ensure2048() {
  if (state.board2048.length) return;
  state.board2048 = Array.from({ length: 4 }, () => Array(4).fill(0));
  add2048Tile();
  add2048Tile();
}

function add2048Tile() {
  const empty = [];
  state.board2048.forEach((row, r) => row.forEach((value, c) => {
    if (!value) empty.push([r, c]);
  }));
  if (!empty.length) return;
  const [r, c] = empty[Math.floor(Math.random() * empty.length)];
  state.board2048[r][c] = Math.random() < 0.9 ? 2 : 4;
}

function slideLine(line) {
  const values = line.filter(Boolean);
  const output = [];
  let gained = 0;
  for (let i = 0; i < values.length; i += 1) {
    if (values[i] === values[i + 1]) {
      output.push(values[i] * 2);
      gained += values[i] * 2;
      i += 1;
    } else {
      output.push(values[i]);
    }
  }
  while (output.length < 4) output.push(0);
  return { output, gained };
}

function move2048(dir) {
  ensure2048();
  let changed = false;
  let gained = 0;
  const next = state.board2048.map((row) => row.slice());

  for (let lane = 0; lane < 4; lane += 1) {
    let line = [];
    for (let i = 0; i < 4; i += 1) {
      if (dir === "left") line.push(state.board2048[lane][i]);
      if (dir === "right") line.push(state.board2048[lane][3 - i]);
      if (dir === "up") line.push(state.board2048[i][lane]);
      if (dir === "down") line.push(state.board2048[3 - i][lane]);
    }
    const result = slideLine(line);
    gained += result.gained;
    for (let i = 0; i < 4; i += 1) {
      const value = result.output[i];
      if (dir === "left") next[lane][i] = value;
      if (dir === "right") next[lane][3 - i] = value;
      if (dir === "up") next[i][lane] = value;
      if (dir === "down") next[3 - i][lane] = value;
    }
  }

  changed = JSON.stringify(next) !== JSON.stringify(state.board2048);
  if (changed) {
    state.board2048 = next;
    state.score2048 += gained;
    add2048Tile();
  }
}

function render2048App() {
  ensure2048();
  el.content.innerHTML = `
    ${appHeader("2048")}
    <section class="game-app">
      <div class="score-row"><span>Score</span><strong>${state.score2048}</strong></div>
      <div class="board-2048">
        ${state.board2048.flat().map((value) => `<div class="tile tile-${value || 0}">${value || ""}</div>`).join("")}
      </div>
      <div class="pad">
        <button type="button" data-action="move2048" data-dir="up" aria-label="2048 向上移动">↑</button>
        <button type="button" data-action="move2048" data-dir="left" aria-label="2048 向左移动">←</button>
        <button type="button" data-action="move2048" data-dir="down" aria-label="2048 向下移动">↓</button>
        <button type="button" data-action="move2048" data-dir="right" aria-label="2048 向右移动">→</button>
      </div>
    </section>`;
}

function renderStopwatchApp() {
  el.content.innerHTML = `
    ${appHeader("Stopwatch")}
    <section class="simple-app">
      <div class="big-time">${formatDuration(state.stopwatchSeconds)}</div>
      <p class="source">${state.stopwatchRunning ? "Running" : "Paused"}</p>
      <div class="button-row">
        <button type="button" data-action="stopwatchToggle">${state.stopwatchRunning ? "Pause" : "Start"}</button>
        <button type="button" data-action="stopwatchReset">Reset</button>
      </div>
    </section>`;
}

function resetMines(count = state.minesCount) {
  state.minesCount = count;
  state.minesOpen = new Set();
  state.minesFlags = new Set();
  state.minesMap = new Set();
  state.minesStatus = count === 4 ? "Easy" : count === 8 ? "Hard" : "Normal";
  while (state.minesMap.size < count) {
    state.minesMap.add(Math.floor(Math.random() * state.minesSize * state.minesSize));
  }
}

function mineNeighborCount(index) {
  const size = state.minesSize;
  const r = Math.floor(index / size);
  const c = index % size;
  let count = 0;
  for (let y = -1; y <= 1; y += 1) {
    for (let x = -1; x <= 1; x += 1) {
      if (x === 0 && y === 0) continue;
      const nr = r + y;
      const nc = c + x;
      if (nr >= 0 && nr < size && nc >= 0 && nc < size && state.minesMap.has(nr * size + nc)) count += 1;
    }
  }
  return count;
}

function openMine(index) {
  if (!state.minesMap.size) resetMines();
  if (state.minesMap.has(index)) {
    state.minesStatus = "Boom";
    state.minesMap.forEach((cell) => state.minesOpen.add(cell));
  } else {
    state.minesOpen.add(index);
    state.minesStatus = "Safe";
  }
}

function renderMinesApp() {
  if (!state.minesMap.size) resetMines();
  const cells = Array.from({ length: state.minesSize * state.minesSize }, (_, index) => {
    const open = state.minesOpen.has(index);
    const mine = state.minesMap.has(index);
    const label = open ? (mine ? "✹" : mineNeighborCount(index) || "") : "";
    return `<button type="button" class="${open ? "open" : ""}" data-mine="${index}" aria-label="扫雷格 ${index + 1}${open ? `，${label || "空"}` : "，未打开"}">${label}</button>`;
  }).join("");

  el.content.innerHTML = `
    ${appHeader("Mines")}
    <section class="mines-app">
      <div class="score-row"><span>${state.minesStatus}</span><strong>${state.minesCount} mines</strong></div>
      <div class="difficulty">
        <button type="button" data-action="minesReset" data-mines="4">Easy</button>
        <button type="button" data-action="minesReset" data-mines="6">Normal</button>
        <button type="button" data-action="minesReset" data-mines="8">Hard</button>
      </div>
      <div class="mine-board">${cells}</div>
    </section>`;
}

function resetTetris() {
  state.tetrisCells = new Set(["42", "43", "44", "45"]);
  state.tetrisPiece = { x: 2, y: 0 };
}

function renderTetrisApp() {
  if (!state.tetrisCells.size) resetTetris();
  const cells = [];
  for (let r = 0; r < 8; r += 1) {
    for (let c = 0; c < 6; c += 1) {
      const active = r === state.tetrisPiece.y && (c === state.tetrisPiece.x || c === state.tetrisPiece.x + 1);
      const fixed = state.tetrisCells.has(String(r * 6 + c));
      cells.push(`<span class="${active ? "active" : fixed ? "fixed" : ""}"></span>`);
    }
  }

  el.content.innerHTML = `
    ${appHeader("Tetris")}
    <section class="tetris-app">
      <div class="tetris-board">${cells.join("")}</div>
      <div class="pad">
        <button type="button" data-action="tetrisMove" data-dir="left" aria-label="俄罗斯方块向左">←</button>
        <button type="button" data-action="tetrisMove" data-dir="down" aria-label="俄罗斯方块向下">↓</button>
        <button type="button" data-action="tetrisMove" data-dir="right" aria-label="俄罗斯方块向右">→</button>
        <button type="button" data-action="tetrisReset">New</button>
      </div>
    </section>`;
}

function renderWoodenApp() {
  el.content.innerHTML = `
    ${appHeader("Wooden Fish")}
    <section class="wooden-app">
      <div class="capybara">
        <span class="lotus"></span>
        <span class="body"></span>
        <span class="head"></span>
        <span class="ear left"></span>
        <span class="ear right"></span>
        <span class="eye left"></span>
        <span class="eye right"></span>
        <span class="nose"></span>
        <span class="mallet"></span>
        ${state.woodenFloat ? `<strong class="float">${state.woodenFloat}</strong>` : ""}
      </div>
      <p class="merit">Merit ${state.merits}</p>
      <p class="source">${state.woodenMessage}</p>
      <div class="button-row">
        <button type="button" data-action="woodenTap">Tap</button>
        <button type="button" data-action="woodenReset">Reset</button>
      </div>
    </section>`;
}

function renderActiveApp() {
  const app = apps.find((item) => item.id === state.activeApp);
  if (!app) {
    state.activeApp = null;
    renderAppsPage();
    return;
  }

  if (app.id === "weather") renderWeatherApp();
  if (app.id === "calculator") renderCalculatorApp();
  if (app.id === "timer") renderTimerApp();
  if (app.id === "game2048") render2048App();
  if (app.id === "stopwatch") renderStopwatchApp();
  if (app.id === "mines") renderMinesApp();
  if (app.id === "tetris") renderTetrisApp();
  if (app.id === "wooden") renderWoodenApp();
}

function render() {
  if (state.activeApp) renderActiveApp();
  else if (state.page === PAGE_HEART) renderHeartPage();
  else if (state.page === PAGE_STEPS) renderStepsPage();
  else if (state.page === PAGE_APPS) renderAppsPage();
  else renderFace();
  updateDots();
}

function updateDots() {
  el.dots.forEach((dot, index) => {
    dot.classList.toggle("active", index === state.page && !state.activeApp);
  });
}

function switchPage(direction) {
  if (state.activeApp) return;
  state.page = (state.page + direction + PAGE_COUNT) % PAGE_COUNT;
  el.content.style.setProperty("--shift", direction > 0 ? "16px" : "-16px");
  el.content.classList.add("switching");
  window.setTimeout(() => {
    render();
    announce(["表盘页面", "心率页面", "活动页面", "应用列表"][state.page]);
    el.content.style.setProperty("--shift", direction > 0 ? "-16px" : "16px");
    window.requestAnimationFrame(() => el.content.classList.remove("switching"));
  }, 120);
}

function calculate(input) {
  if (input === "C") {
    state.calcDisplay = "0";
    state.calcStored = null;
    state.calcOp = null;
    state.calcFresh = true;
    return;
  }

  if (input === "⌫") {
    state.calcDisplay = state.calcDisplay.length > 1 ? state.calcDisplay.slice(0, -1) : "0";
    return;
  }

  if (/^[0-9.]$/.test(input)) {
    if (state.calcFresh) {
      state.calcDisplay = input === "." ? "0." : input;
      state.calcFresh = false;
    } else if (!(input === "." && state.calcDisplay.includes("."))) {
      state.calcDisplay = state.calcDisplay === "0" && input !== "." ? input : state.calcDisplay + input;
    }
    return;
  }

  if (["+", "-", "×", "÷", "="].includes(input)) {
    const current = Number(state.calcDisplay);
    if (state.calcStored !== null && state.calcOp) {
      if (state.calcOp === "+") state.calcStored += current;
      if (state.calcOp === "-") state.calcStored -= current;
      if (state.calcOp === "×") state.calcStored *= current;
      if (state.calcOp === "÷") state.calcStored = current === 0 ? 0 : state.calcStored / current;
      state.calcDisplay = String(Math.round(state.calcStored * 1000) / 1000);
    } else {
      state.calcStored = current;
    }
    state.calcOp = input === "=" ? null : input;
    state.calcFresh = true;
  }
}

function handleAction(action, target) {
  if (action === "back") state.activeApp = null;
  if (action === "goalDown") state.stepGoal = clamp(state.stepGoal - STEP_GOAL_DELTA, STEP_GOAL_MIN, STEP_GOAL_MAX);
  if (action === "goalUp") state.stepGoal = clamp(state.stepGoal + STEP_GOAL_DELTA, STEP_GOAL_MIN, STEP_GOAL_MAX);
  if (action === "timerDown" && !state.timerRunning) state.timerSeconds = Math.max(0, state.timerSeconds - 60);
  if (action === "timerUp" && !state.timerRunning) state.timerSeconds = Math.min(99 * 60, state.timerSeconds + 60);
  if (action === "timerToggle") {
    if (state.timerSeconds <= 0) state.timerSeconds = 5 * 60;
    state.timerRunning = !state.timerRunning;
  }
  if (action === "timerReset") {
    state.timerSeconds = 5 * 60;
    state.timerRunning = false;
  }
  if (action === "stopwatchToggle") state.stopwatchRunning = !state.stopwatchRunning;
  if (action === "stopwatchReset") {
    state.stopwatchSeconds = 0;
    state.stopwatchRunning = false;
  }
  if (action === "move2048") move2048(target.dataset.dir);
  if (action === "minesReset") resetMines(Number(target.dataset.mines));
  if (action === "tetrisMove") {
    const dir = target.dataset.dir;
    if (dir === "left") state.tetrisPiece.x = Math.max(0, state.tetrisPiece.x - 1);
    if (dir === "right") state.tetrisPiece.x = Math.min(4, state.tetrisPiece.x + 1);
    if (dir === "down") state.tetrisPiece.y = Math.min(6, state.tetrisPiece.y + 1);
  }
  if (action === "tetrisReset") resetTetris();
  if (action === "woodenTap") {
    const now = Date.now();
    state.merits += 1;
    state.woodenFloat = "Merit +1";
    state.woodenMessage = now - state.lastTapAt < 450 ? "Too fast, breathe" : "Good rhythm";
    state.lastTapAt = now;
  }
  if (action === "woodenReset") {
    state.merits = 0;
    state.woodenFloat = "";
    state.woodenMessage = "Merit becomes your luck";
  }
  render();
  const actionSummary = {
    back: "已返回应用列表",
    goalDown: `步数目标 ${state.stepGoal}`,
    goalUp: `步数目标 ${state.stepGoal}`,
    timerToggle: state.timerRunning ? "计时器已开始" : "计时器已暂停",
    timerReset: "计时器已重置",
    stopwatchToggle: state.stopwatchRunning ? "秒表已开始" : "秒表已暂停",
    stopwatchReset: "秒表已重置",
    woodenTap: `功德 ${state.merits}`,
    woodenReset: "功德已重置",
  };
  if (actionSummary[action]) announce(actionSummary[action]);
}

function tick() {
  state.ticks += 1;
  simulateHealthData();
  if (state.timerRunning && state.timerSeconds > 0) state.timerSeconds -= 1;
  if (state.timerRunning && state.timerSeconds <= 0) {
    state.timerSeconds = 0;
    state.timerRunning = false;
    announce("计时器已完成");
  }
  if (state.stopwatchRunning) state.stopwatchSeconds += 1;
  if (state.activeApp === "tetris" && state.ticks % 2 === 0) {
    state.tetrisPiece.y += 1;
    if (state.tetrisPiece.y > 6) state.tetrisPiece = { x: 2, y: 0 };
  }
  render();
}

let startX = 0;
let startY = 0;

el.watch.addEventListener("pointerdown", (event) => {
  startX = event.clientX;
  startY = event.clientY;
});

el.watch.addEventListener("pointerup", (event) => {
  const dx = event.clientX - startX;
  const dy = event.clientY - startY;
  if (Math.abs(dx) > 42 && Math.abs(dx) > Math.abs(dy) * 1.4) {
    if (state.activeApp === "game2048") {
      move2048(dx < 0 ? "left" : "right");
      render();
    } else {
      switchPage(dx < 0 ? 1 : -1);
    }
  }
});

el.content.addEventListener("click", (event) => {
  const appButton = event.target.closest("[data-app]");
  const actionButton = event.target.closest("[data-action]");
  const calcButton = event.target.closest("[data-calc]");
  const mineButton = event.target.closest("[data-mine]");

  if (appButton) {
    state.activeApp = appButton.dataset.app;
    render();
    announce(`已打开 ${apps.find((app) => app.id === state.activeApp)?.title || "应用"}`);
    return;
  }

  if (actionButton) {
    handleAction(actionButton.dataset.action, actionButton);
    return;
  }

  if (calcButton) {
    calculate(calcButton.dataset.calc);
    render();
    return;
  }

  if (mineButton) {
    openMine(Number(mineButton.dataset.mine));
    render();
  }
});

el.prevBtn.addEventListener("click", () => switchPage(-1));
el.nextBtn.addEventListener("click", () => switchPage(1));

window.addEventListener("keydown", (event) => {
  if (event.key === "ArrowLeft") switchPage(-1);
  if (event.key === "ArrowRight") switchPage(1);
  if (state.activeApp === "game2048" && ["ArrowUp", "ArrowDown"].includes(event.key)) {
    move2048(event.key === "ArrowUp" ? "up" : "down");
    render();
  }
});

render();
window.setInterval(tick, 1000);
