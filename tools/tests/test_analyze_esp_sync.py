"""Deterministic, offline tests for Vesta's passive ESP Sync CSV analyzer."""
import importlib.util
import unittest
from pathlib import Path

SCRIPT = Path(__file__).resolve().parents[1] / "analyze_esp_sync.py"
SPEC = importlib.util.spec_from_file_location("vesta_esp_sync_analyzer", SCRIPT)
assert SPEC and SPEC.loader
analyzer = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(analyzer)


class TestEspSyncAnalyzer(unittest.TestCase):
    def test_present_age_is_not_pose_acquisition_age(self):
        rows = [
            {"pose_age_ms": str(age), "world_age_ms": str(age + 8),
             "present_call_ms": str(present), "display_refresh_hz": "165",
             "camera_projection_shift_px": str(shift),
             "draw_frames": "10", "repeated_pose_frames": "1"}
            for age, present, shift in (
                (10, 5, 0), (11, 6, 3), (17, 12, 7), (21, 16, 12))
        ]
        report = analyzer.summarize(rows)
        self.assertEqual(report["samples"], 4)
        self.assertEqual(report["pose_age_before_present_est_ms"]["median"], 5)
        self.assertEqual(report["world_age_before_present_est_ms"]["median"], 13)
        self.assertEqual(report["present_call_over_refresh_budget_ratio"], 0.5)
        self.assertEqual(report["projection_shift_over_5px_ratio"], 0.5)
        self.assertAlmostEqual(report["repeated_pose_fraction_of_draw_frames"], 0.1)
        self.assertIsNone(report["camera_rotation_deg_vs_projection_shift_r"])

    def test_unavailable_or_negative_fields_are_not_fabricated(self):
        rows = [
            {"pose_age_ms": "3", "present_call_ms": "5", "sim_unchanged_ms": "-1",
             "camera_gap_ms": "-1", "display_refresh_hz": "0"},
            {"pose_age_ms": "NaN", "present_call_ms": "", "camera_rotation_deg": "-1"},
        ]
        report = analyzer.summarize(rows)
        self.assertEqual(report["pose_age_before_present_est_ms"]["count"], 0)
        self.assertEqual(report["world_age_before_present_est_ms"]["count"], 0)
        self.assertEqual(report["sim_unchanged_ms"]["count"], 0)
        self.assertEqual(report["camera_gap_ms"]["count"], 0)
        self.assertEqual(report["present_budget_samples"], 0)
        self.assertIsNone(report["present_call_over_refresh_budget_ratio"])

    def test_empty_group_is_safe(self):
        report = analyzer.summarize([])
        self.assertEqual(report["samples"], 0)
        self.assertIsNone(report["projection_shift_over_5px_ratio"])
        self.assertIsNone(report["pose_age_vs_present_call_r"])
        self.assertIsNone(report["present_call_over_refresh_budget_ratio"])


if __name__ == "__main__":
    unittest.main()
