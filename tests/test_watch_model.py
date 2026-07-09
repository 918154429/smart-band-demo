import datetime as dt
import unittest


PAGE_FACE = 0
PAGE_HEART = 1
PAGE_STEPS = 2
PAGE_APPS = 3
PAGE_COUNT = 4
STEP_GOAL = 8000
STEP_GOAL_MIN = 1000
STEP_GOAL_MAX = 50000
STEP_GOAL_DELTA = 1000


class SmartBandState:
    def __init__(self, now):
        self.page = PAGE_FACE
        self.ticks = 0
        self.heart_rate = 72
        self.steps = 1260
        self.step_goal = STEP_GOAL
        self.battery = 96
        self.temperature_c = 24
        self.update_time(now)

    def update_time(self, now):
        self.time_text = f"{now.hour:02d}:{now.minute:02d}"
        self.date_text = f"{now.year:04d}/{now.month:02d}/{now.day:02d}"

    def tick(self, now):
        self.ticks += 1
        self.update_time(now)
        pulse_wave = (self.ticks * 7 + 11) % 23
        motion_wave = (self.ticks * 5 + 3) % 9
        self.heart_rate = max(55, min(135, 66 + pulse_wave + motion_wave // 3))
        self.steps += 4 + self.ticks % 6
        self.temperature_c = max(-40, min(80, 24 + self.ticks % 3 - 1))
        if self.steps > 99999:
            self.steps %= self.step_goal
        self.battery = max(5, min(100, 96 - self.ticks // 180))

    def next_page(self):
        self.page = (self.page + 1) % PAGE_COUNT

    def prev_page(self):
        self.page = (self.page + PAGE_COUNT - 1) % PAGE_COUNT

    def step_progress(self):
        if self.steps <= 0:
            return 0
        return max(0, min(100, (self.steps * 100) // self.step_goal))

    def adjust_step_goal(self, delta):
        self.step_goal = max(STEP_GOAL_MIN, min(STEP_GOAL_MAX,
                                                self.step_goal + delta))


class WatchModelTest(unittest.TestCase):
    def test_formats_time_and_date(self):
        state = SmartBandState(dt.datetime(2026, 7, 6, 9, 5))
        self.assertEqual(state.time_text, "09:05")
        self.assertEqual(state.date_text, "2026/07/06")

    def test_page_wraps_in_both_directions(self):
        state = SmartBandState(dt.datetime(2026, 7, 6, 9, 5))
        state.prev_page()
        self.assertEqual(state.page, PAGE_APPS)
        state.next_page()
        self.assertEqual(state.page, PAGE_FACE)
        state.next_page()
        self.assertEqual(state.page, PAGE_HEART)

    def test_simulated_data_stays_readable(self):
        state = SmartBandState(dt.datetime(2026, 7, 6, 9, 5))
        for second in range(1, 600):
            state.tick(dt.datetime(2026, 7, 6, 9, 5) + dt.timedelta(seconds=second))
            self.assertGreaterEqual(state.heart_rate, 55)
            self.assertLessEqual(state.heart_rate, 135)
            self.assertGreaterEqual(state.battery, 5)
            self.assertLessEqual(state.battery, 100)
            self.assertGreaterEqual(state.temperature_c, -40)
            self.assertLessEqual(state.temperature_c, 80)
        self.assertGreater(state.steps, 1260)
        self.assertGreaterEqual(state.step_progress(), 0)
        self.assertLessEqual(state.step_progress(), 100)

    def test_step_goal_adjusts_progress(self):
        state = SmartBandState(dt.datetime(2026, 7, 6, 9, 5))
        state.steps = 4000
        self.assertEqual(state.step_progress(), 50)
        state.adjust_step_goal(STEP_GOAL_DELTA)
        self.assertEqual(state.step_goal, 9000)
        self.assertEqual(state.step_progress(), 44)
        for _ in range(20):
            state.adjust_step_goal(-STEP_GOAL_DELTA)
        self.assertEqual(state.step_goal, STEP_GOAL_MIN)
        for _ in range(100):
            state.adjust_step_goal(STEP_GOAL_DELTA)
        self.assertEqual(state.step_goal, STEP_GOAL_MAX)


if __name__ == "__main__":
    unittest.main()
