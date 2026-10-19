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
