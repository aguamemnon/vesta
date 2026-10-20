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

    def test_probe_status_distinguishes_dropped_visual_data_from_present_timing(self):
        rows = [
            {"probe_status": "0", "pose_age_ms": "9", "present_call_ms": "5",
             "display_refresh_hz": "165", "camera_projection_shift_px": "3"},
            {"probe_status": "1", "pose_age_ms": "-1", "present_call_ms": "8",
             "display_refresh_hz": "165", "camera_projection_shift_px": "-1"},
            {"probe_status": "3", "pose_age_ms": "15", "present_call_ms": "10",
             "display_refresh_hz": "165", "camera_projection_shift_px": "-1"},
            {"probe_status": "5", "pose_age_ms": "-1", "present_call_ms": "12",
             "display_refresh_hz": "165", "camera_projection_shift_px": "-1"},
        ]
        report = analyzer.summarize(rows)
        self.assertEqual(report["samples"], 4)
        self.assertEqual(report["probe_status_samples"], 4)
        self.assertEqual(report["probe_status_counts"]["pose_unavailable"], 1)
        self.assertEqual(report["probe_status_counts"]["no_visible_pose"], 1)
        self.assertEqual(report["probe_status_counts"]["not_drawn"], 1)
        self.assertEqual(report["probe_selected_ratio"], 0.25)
        self.assertEqual(report["probe_unavailable_ratio"], 0.75)
        self.assertEqual(report["pose_age_ms"]["count"], 1)
        self.assertEqual(report["pose_age_ms"]["median"], 9)
        self.assertEqual(report["projection_shift_over_5px_ratio"], 0.0)
        self.assertEqual(report["present_call_ms"]["count"], 4)
        self.assertEqual(report["present_call_over_refresh_budget_ratio"], 0.75)

    def test_legacy_csv_has_no_invented_probe_status(self):
        report = analyzer.summarize([{
            "pose_age_ms": "11", "present_call_ms": "6",
            "camera_projection_shift_px": "7"
        }])
        self.assertIsNone(report["probe_status_counts"])
        self.assertIsNone(report["probe_selected_ratio"])
        self.assertEqual(report["pose_age_ms"]["median"], 11)


if __name__ == "__main__":
    unittest.main()
