# ESP Sync CSV offline analyzer

The passive analysis tool `tools/analyze_esp_sync.py` reads CSVs already exported by the optional ESP Sync Probe. It does **not** capture the screen, connect to the game, change ESP positions, or alter render/polling rates.

## Usage

From the repository directory, with Python 3.10+:

```powershell
python tools/analyze_esp_sync.py "$env:TEMP\vesta\logs\vesta-esp-sync.csv"
```

For a controlled 40-second session, clear samples first, leave the bot stationary, and make these movements in order: 0–10 s still, 10–25 s rotate the camera without walking, 25–40 s strafe and rotate. Export a single CSV at the end.

```powershell
python tools/analyze_esp_sync.py "$env:TEMP\vesta\logs\vesta-esp-sync.csv" --phases still:0:10000 rotate:10000:25000 move_rotate:25000:40000 --json esp-sync-report.json
```

**Important:** Phases are selected by elapsed capture time; the script cannot infer actual mouse or movement input from the exported CSV. If the phases weren't performed at the expected time, use correct intervals. It never claims automatic detection of movement.

Reported values include per-phase median/p95/max for timing and projection discrepancy, Pearson correlation between camera time gap and projection shift, proportion of fallback bones and dropped presents, and fraction of rendered frames with repeated poses.

**Interpretation:** Increasing shift with camera rotation can be consistent with camera/pose timing disagreement. Correlation alone does not establish causation. Two projections can also agree while both differ from the game image. Present timings measure CPU calls, **not** photon arrival at the monitor. The analyzer does not measure end-to-end CS2 input-to-display lag.

No runtime component or 165 Hz display settings are modified by this tool.


## Camera motion telemetry (next diagnostic build)

The probe appends two columns without changing the existing CSV column order:
- `camera_rotation_deg`: shortest-angle difference between the pose-captured and render-time camera orientation (degrees).
- `camera_translation_units`: Euclidean difference between those two camera origins (CS2 game units).

These are calculated from the camera samples **already present** in the probe and require no extra reads. They measure change *between two independently sampled states*, not the mouse's actual input, the game's final displayed camera or precise end-to-end visual latency.

For a new-format CSV, request automatic **observed-motion** buckets:

```powershell
python tools/analyze_esp_sync.py "$env:TEMP\vesta\logs\vesta-esp-sync.csv" --motion-split --json esp-sync-report.json
```

Buckets are `no_change_observed`, `rotation_only_observed`, `translation_only_observed`, `both_observed`, and `invalid`. The default thresholds are 0.05 degrees and 0.10 game units, overridable using `--rotation-threshold` and `--translation-threshold`. A lack of measured change between snapshots does **not** prove the player was stationary: measurements can coincide within a game frame, and 10 Hz log rows can miss movements. Use these groups as comparative evidence only.

The analyzer continues to accept old CSV files without motion columns, provided `--motion-split` is omitted. Avoid comparing runs made with different Performance Mode, GPU Effects or game settings as a controlled performance benchmark.
