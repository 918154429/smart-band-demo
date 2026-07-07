const PAGE_FACE = 0;
const PAGE_HEART = 1;
const PAGE_STEPS = 2;
const PAGE_COUNT = 3;
const STEP_GOAL = 8000;

const state = {
  page: PAGE_FACE,
  ticks: 0,
  heartRate: 68,
  steps: 4260,
  battery: 96,
  sleepMinutes: 468,
  weather: 24,
};

const el = {
  watch: document.getElementById("watch"),
  content: document.getElementById("content"),
  dots: Array.from(document.querySelectorAll(".dots span")),
  prevBtn: document.getElementById("prevBtn"),
  nextBtn: document.getElementById("nextBtn"),
};

const icons = {
  leaf: `
    <svg viewBox="0 0 80 54" aria-hidden="true">
      <path d="M39 35C24 34 14 23 14 8c17 0 27 9 28 25C42 34 41 35 39 35Z" fill="currentColor" opacity=".9"/>
      <path d="M43 34C44 18 53 8 66 5c6 16-4 29-20 32-2 0-3-1-3-3Z" fill="currentColor" opacity=".85"/>
      <path d="M40 28C36 17 39 8 48 0c10 11 9 24-2 34-3 2-5 0-6-6Z" fill="currentColor" opacity=".72"/>
    </svg>`,
  heartLine: `
    <svg viewBox="0 0 64 64" aria-hidden="true">
      <path d="M32 54S9 39 9 23c0-8 6-14 14-14 5 0 8 2 11 6 3-4 7-6 12-6 8 0 14 6 14 14 0 16-28 31-28 31Z" fill="none" stroke="currentColor" stroke-width="4" stroke-linejoin="round"/>
    </svg>`,
  moon: `
    <svg viewBox="0 0 64 64" aria-hidden="true">
      <path d="M41 50c-15 0-27-12-27-27 0-5 1-9 3-13C8 16 4 25 6 35c3 15 17 25 32 22 8-2 15-7 19-14-5 5-10 7-16 7Z" fill="currentColor"/>
      <path d="M45 15l2 4 4 1-4 2-2 4-2-4-4-2 4-1 2-4ZM53 25l1 3 3 1-3 1-1 3-2-3-3-1 3-1 2-3Z" fill="currentColor"/>
    </svg>`,
  heart: `
    <svg viewBox="0 0 64 64" aria-hidden="true">
      <path d="M32 54S9 39 9 23c0-8 6-14 14-14 5 0 8 2 11 6 3-4 7-6 12-6 8 0 14 6 14 14 0 16-28 31-28 31Z" fill="currentColor"/>
      <path d="M18 32h9l4-12 7 22 4-10h7" fill="none" stroke="#f08d88" stroke-linecap="round" stroke-linejoin="round" stroke-width="4"/>
    </svg>`,
  calm: `
    <svg viewBox="0 0 64 64" aria-hidden="true">
      <path d="M32 18a7 7 0 1 0 0-14 7 7 0 0 0 0 14Z" fill="none" stroke="currentColor" stroke-width="4"/>
      <path d="M24 24c-8 3-14 10-15 21 7 1 13-2 18-8M40 24c8 3 14 10 15 21-7 1-13-2-18-8" fill="none" stroke="currentColor" stroke-linecap="round" stroke-width="4"/>
      <path d="M32 22v22M18 54c8 4 20 4 28 0M24 44c-4 2-8 5-11 9M40 44c4 2 8 5 11 9" fill="none" stroke="currentColor" stroke-linecap="round" stroke-width="4"/>
    </svg>`,
  weather: `
    <svg viewBox="0 0 64 64" aria-hidden="true">
      <path d="M25 34a14 14 0 1 1 25 8" fill="#ffc83d"/>
      <path d="M16 48h34a9 9 0 0 0 0-18 13 13 0 0 0-25-1 10 10 0 0 0-9 19Z" fill="currentColor"/>
      <path d="M17 14l-4-4M10 28H4M25 9V3M42 14l4-4" stroke="currentColor" stroke-linecap="round" stroke-width="4"/>
    </svg>`,
  steps: `
    <svg viewBox="0 0 64 64" aria-hidden="true">
      <path d="M24 13c5 0 9 4 9 10 0 9-5 18-12 18-6 0-9-5-9-12 0-8 5-16 12-16ZM43 31c5 0 9 4 9 10 0 8-5 14-12 14-6 0-9-5-9-11 0-7 5-13 12-13Z" fill="currentColor"/>
    </svg>`,
};

function clamp(value, min, max) {
  return Math.min(Math.max(value, min), max);
}

function two(value) {
  return String(value).padStart(2, "0");
}

function dateParts() {
  const now = new Date();
  const weekdays = ["SUN", "MON", "TUE", "WED", "THU", "FRI", "SAT"];
  const months = ["JAN", "FEB", "MAR", "APR", "MAY", "JUN", "JUL", "AUG", "SEP", "OCT", "NOV", "DEC"];
  return {
    time: `${two(now.getHours())}:${two(now.getMinutes())}`,
    date: `${weekdays[now.getDay()]} ${two(now.getDate())} ${months[now.getMonth()]}`,
  };
}

function simulateHealthData() {
  const pulseWave = (state.ticks * 7 + 11) % 20;
  const motionWave = (state.ticks * 5 + 3) % 8;

  state.heartRate = clamp(62 + pulseWave + Math.floor(motionWave / 3), 55, 135);
  state.steps += 4 + (state.ticks % 6);
  if (state.steps > 99999) state.steps %= STEP_GOAL;
  state.battery = clamp(96 - Math.floor(state.ticks / 180), 5, 100);
}

function stepProgress() {
  return clamp(Math.floor((state.steps * 100) / STEP_GOAL), 0, 100);
}

function iconOrb(name) {
  return `<span class="icon-orb">${icons[name]}</span>`;
}

function metricCard(kind, icon, label, value) {
  return `
    <article class="metric-card ${kind}">
      ${iconOrb(icon)}
      <span class="divider" aria-hidden="true"></span>
      <div class="metric-copy">
        <p class="metric-label">${label}</p>
        <p class="metric-value">${value}</p>
      </div>
    </article>`;
}

function renderFace() {
  const now = dateParts();
  const [hour, minute] = now.time.split(":");
  const sleepHours = Math.floor(state.sleepMinutes / 60);
  const sleepMinutes = state.sleepMinutes % 60;

  el.content.innerHTML = `
    <div class="leaf-mark">${icons.leaf}</div>
    <div class="date-row">${now.date}</div>
    <div class="time-display" aria-label="当前时间 ${now.time}">
      <span class="time-part">${hour}</span>
      <span class="colon"><span></span><span></span></span>
      <span class="time-part">${minute}</span>
    </div>
    <div class="ornament">${icons.heartLine}</div>
    <div class="cards">
      ${metricCard("sleep", "moon", "Sleep", `${sleepHours}<small>h</small> ${sleepMinutes}<small>m</small>`)}
      ${metricCard("heart", "heart", "Heart Rate", `${state.heartRate}<small> bpm</small>`)}
      ${metricCard("stress", "calm", "Stress", `Low`)}
      ${metricCard("weather", "weather", "Weather", `${state.weather}<small>°c</small>`)}
    </div>`;
}

function renderHeartDetail() {
  el.content.innerHTML = `
    <div class="leaf-mark">${icons.leaf}</div>
    <div class="date-row">${dateParts().date}</div>
    <section class="detail-page">
      <h1 class="detail-title">Heart Rate</h1>
      <div class="detail-hero heart">
        ${iconOrb("heart")}
        <p class="detail-number">${state.heartRate}<small> bpm</small></p>
        <div class="progress"><span style="width:${Math.floor((state.heartRate / 135) * 100)}%"></span></div>
      </div>
      <div class="mini-grid">
        <div class="mini-card"><span>Resting</span><strong>62</strong></div>
        <div class="mini-card"><span>Status</span><strong>${state.heartRate > 110 ? "High" : "Good"}</strong></div>
        <div class="mini-card"><span>Battery</span><strong>${state.battery}%</strong></div>
        <div class="mini-card"><span>Stress</span><strong>Low</strong></div>
      </div>
    </section>`;
}

function renderStepsDetail() {
  const progress = stepProgress();
  el.content.innerHTML = `
    <div class="leaf-mark">${icons.leaf}</div>
    <div class="date-row">${dateParts().date}</div>
    <section class="detail-page">
      <h1 class="detail-title">Activity</h1>
      <div class="detail-hero stress">
        ${iconOrb("steps")}
        <p class="detail-number">${state.steps.toLocaleString("zh-CN")}</p>
        <div class="progress"><span style="width:${progress}%"></span></div>
      </div>
      <div class="mini-grid">
        <div class="mini-card"><span>Goal</span><strong>${STEP_GOAL.toLocaleString("zh-CN")}</strong></div>
        <div class="mini-card"><span>Progress</span><strong>${progress}%</strong></div>
        <div class="mini-card"><span>Sleep</span><strong>7h48</strong></div>
        <div class="mini-card"><span>Weather</span><strong>${state.weather}°c</strong></div>
      </div>
    </section>`;
}

function updateDots() {
  el.dots.forEach((dot, index) => {
    dot.classList.toggle("active", index === state.page);
  });
}

function render() {
  if (state.page === PAGE_HEART) renderHeartDetail();
  else if (state.page === PAGE_STEPS) renderStepsDetail();
  else renderFace();
  updateDots();
}

function switchPage(direction) {
  state.page = (state.page + direction + PAGE_COUNT) % PAGE_COUNT;
  el.content.style.setProperty("--shift", direction > 0 ? "16px" : "-16px");
  el.content.classList.add("switching");
  window.setTimeout(() => {
    render();
    el.content.style.setProperty("--shift", direction > 0 ? "-16px" : "16px");
    window.requestAnimationFrame(() => el.content.classList.remove("switching"));
  }, 120);
}

function tick() {
  state.ticks += 1;
  simulateHealthData();
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
    switchPage(dx < 0 ? 1 : -1);
  }
});

el.prevBtn.addEventListener("click", () => switchPage(-1));
el.nextBtn.addEventListener("click", () => switchPage(1));

window.addEventListener("keydown", (event) => {
  if (event.key === "ArrowLeft") switchPage(-1);
  if (event.key === "ArrowRight") switchPage(1);
});

render();
window.setInterval(tick, 1000);
