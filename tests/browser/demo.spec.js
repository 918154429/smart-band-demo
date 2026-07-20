const { test, expect } = require("@playwright/test");

const viewports = [
  { name: "small portrait", width: 320, height: 568 },
  { name: "compact landscape", width: 667, height: 375 },
];

async function openDemo(page, viewport) {
  await page.setViewportSize(viewport);
  await page.goto("/demo/index.html");
  await expect(page.locator("#content")).not.toBeEmpty();
}

async function expectVisibleButtonsAccessible(page) {
  const failures = await page.locator("button:visible").evaluateAll((buttons) => {
    const parseColor = (value) => {
      const parts = value.match(/[\d.]+/g)?.map(Number) || [];
      return { r: parts[0] || 0, g: parts[1] || 0, b: parts[2] || 0,
        a: parts.length > 3 ? parts[3] : 1 };
    };
    const mix = (front, back) => ({
      r: front.r * front.a + back.r * (1 - front.a),
      g: front.g * front.a + back.g * (1 - front.a),
      b: front.b * front.a + back.b * (1 - front.a),
      a: 1,
    });
    const effectiveBackground = (element) => {
      let result = { r: 255, g: 255, b: 255, a: 1 };
      const layers = [];
      for (let node = element; node; node = node.parentElement) {
        const color = parseColor(getComputedStyle(node).backgroundColor);
        if (color.a > 0) layers.push(color);
      }
      layers.reverse().forEach((layer) => { result = mix(layer, result); });
      return result;
    };
    const luminance = (color) => {
      const channel = (value) => {
        const normalized = value / 255;
        return normalized <= 0.04045 ? normalized / 12.92 :
          ((normalized + 0.055) / 1.055) ** 2.4;
      };
      return 0.2126 * channel(color.r) + 0.7152 * channel(color.g) +
        0.0722 * channel(color.b);
    };
    const contrast = (front, back) => {
      const lighter = Math.max(luminance(front), luminance(back));
      const darker = Math.min(luminance(front), luminance(back));
      return (lighter + 0.05) / (darker + 0.05);
    };

    return buttons.flatMap((button) => {
      const rect = button.getBoundingClientRect();
      const foreground = parseColor(getComputedStyle(button).color);
      const background = effectiveBackground(button);
      const ratio = contrast(foreground, background);
      const name = button.getAttribute("aria-label") || button.textContent.trim();
      const errors = [];
      if (rect.width < 43.9 || rect.height < 43.9) {
        errors.push(`${name}: ${rect.width.toFixed(1)}x${rect.height.toFixed(1)}`);
      }
      if (button.textContent.trim() && ratio < 4.5) {
        errors.push(`${name}: contrast ${ratio.toFixed(2)}`);
      }
      return errors;
    });
  });
  expect(failures).toEqual([]);
}

for (const viewport of viewports) {
  test(`${viewport.name} keeps the watch and navigation operable`, async ({ page }) => {
    await openDemo(page, viewport);

    const layout = await page.evaluate(() => {
      const controls = document.querySelector(".controls").getBoundingClientRect();
      const watch = document.querySelector(".watch").getBoundingClientRect();
      return {
        documentWidth: document.documentElement.scrollWidth,
        viewportWidth: window.innerWidth,
        controls: { left: controls.left, right: controls.right,
          top: controls.top, bottom: controls.bottom },
        watch: { left: watch.left, right: watch.right, top: watch.top, bottom: watch.bottom },
        screen: (() => {
          const rect = document.querySelector(".screen").getBoundingClientRect();
          return { top: rect.top, bottom: rect.bottom };
        })(),
        cards: Array.from(document.querySelectorAll(".metric-card"), (card) => {
          const rect = card.getBoundingClientRect();
          return { top: rect.top, bottom: rect.bottom };
        }),
        header: (() => {
          const dateElement = document.querySelector(".date-row");
          const dateRange = document.createRange();
          dateRange.selectNodeContents(dateElement);
          const date = dateRange.getBoundingClientRect();
          const battery = document.querySelector(".battery").getBoundingClientRect();
          return {
            date: { left: date.left, right: date.right, top: date.top, bottom: date.bottom },
            battery: { left: battery.left, right: battery.right,
              top: battery.top, bottom: battery.bottom },
          };
        })(),
        contentOverflow: getComputedStyle(document.querySelector("#content")).overflow,
      };
    });

    expect(layout.documentWidth).toBeLessThanOrEqual(layout.viewportWidth);
    for (const rect of [layout.controls, layout.watch]) {
      expect(rect.left).toBeGreaterThanOrEqual(0);
      expect(rect.top).toBeGreaterThanOrEqual(0);
      expect(rect.right).toBeLessThanOrEqual(viewport.width);
      expect(rect.bottom).toBeLessThanOrEqual(viewport.height);
    }
    expect(layout.contentOverflow).toMatch(/auto|scroll/);
    for (const card of layout.cards) {
      expect(card.top).toBeGreaterThanOrEqual(layout.screen.top);
      expect(card.bottom).toBeLessThanOrEqual(layout.screen.bottom);
    }
    const headerOverlaps = !(
      layout.header.date.right <= layout.header.battery.left ||
      layout.header.battery.right <= layout.header.date.left ||
      layout.header.date.bottom <= layout.header.battery.top ||
      layout.header.battery.bottom <= layout.header.date.top
    );
    expect(headerOverlaps).toBe(false);
    await expectVisibleButtonsAccessible(page);
  });
}

test("live announcements are isolated from the changing UI", async ({ page }) => {
  await openDemo(page, viewports[0]);
  await expect(page.locator("[aria-live]")).toHaveCount(1);
  await expect(page.locator("#statusLive")).toHaveAttribute("aria-live", "polite");
  expect(await page.locator("#content").evaluate((node) => node.closest("[aria-live]"))).toBeNull();
});

test("one-second updates preserve the focused control and DOM identity", async ({ page }) => {
  await openDemo(page, viewports[0]);
  await page.getByRole("button", { name: "下一页" }).click();
  await page.waitForTimeout(260);
  await page.getByRole("button", { name: "下一页" }).click();
  await page.waitForTimeout(260);
  await page.getByRole("button", { name: "下一页" }).click();
  await page.waitForTimeout(260);
  await page.getByRole("button", { name: "Timer" }).click();

  const timerButton = page.locator('[data-action="timerToggle"]');
  await timerButton.focus();
  await timerButton.evaluate((button) => { window.__focusedTimerButton = button; });
  await page.waitForTimeout(1_150);

  expect(await page.evaluate(() =>
    document.activeElement === window.__focusedTimerButton &&
    window.__focusedTimerButton.isConnected)).toBe(true);
});

test("all application controls meet target-size and contrast requirements", async ({ page }) => {
  await openDemo(page, viewports[0]);
  for (let index = 0; index < 3; index += 1) {
    await page.getByRole("button", { name: "下一页" }).click();
    await page.waitForTimeout(260);
  }
  await expectVisibleButtonsAccessible(page);

  const appNames = ["Weather", "Calculator", "Timer", "2048", "Stopwatch",
    "Mines", "Tetris", "Wooden Fish"];
  for (const name of appNames) {
    await page.getByRole("button", { name }).click();
    await expectVisibleButtonsAccessible(page);
    await page.getByRole("button", { name: "返回应用列表" }).click();
  }
});

test("reduced-motion preference removes visible transitions", async ({ page }) => {
  await page.emulateMedia({ reducedMotion: "reduce" });
  await openDemo(page, viewports[0]);
  const animated = await page.locator("#watch *").evaluateAll((nodes) => nodes.flatMap((node) => {
    const style = getComputedStyle(node);
    const durations = `${style.animationDuration},${style.transitionDuration}`
      .split(",").map((value) => Number.parseFloat(value) || 0);
    return durations.some((duration) => duration > 0.01) ? [node.className || node.tagName] : [];
  }));
  expect(animated).toEqual([]);
});
