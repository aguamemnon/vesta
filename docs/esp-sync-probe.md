# ESP Sync Probe — Vesta v1.2.16

This **opt-in, passive** tool tests why the Player ESP box **and** skeleton appear behind an opponent when the observer strafes, even with a stationary bot.

## Where to find it

1. Start CS2 and Vesta. Leave **Performance Mode** enabled; keep your usual ESP box/skeleton.
2. Open **Misc → General → Engine → ESP Sync Probe**. It defaults to OFF.
3. A small panel appears near the top-right of the game. Pick a bot on screen.
4. With a **stationary bot**, stand still for a few seconds, then strafe quickly left-right for ~20 seconds.
5. Repeat while the bot moves and finally while you also move. Avoid changing CS2 graphics/settings between stages.
6. Open Misc again and click **Save ESP Sync CSV**, then **Logs**. Send the file
   `%TEMP%\vesta\logs\vesta-esp-sync.csv`. A screenshot or recording is **not required**.
7. **Clear Sync Samples** starts a fresh capture for another test.

Nothing is transmitted over the network. The file contains only numerical timing / projection measurements, no player names, Steam IDs, physical addresses, or game world coordinates.

## On-screen marker legend

- **Cyan circle (O):** one enemy head bone projected by the latest successfully sampled **raw** camera matrix, before the v1.2.13 compensation.
- **Orange cross (X):** **the same exact head bone** projected by the camera matrix captured with the player pose frame.
- **Green dot (+):** the v1.2.13 compensated *visual* projection when prediction is accepted. Missing green dot means prediction was bypassed that frame.
- **Connecting line:** purely the on-screen **camera/projection discrepancy** for an identical 3D point, measured in pixels.
- The sampled target is the on-screen enemy pose closest to screen center. The probe does not switch targets, move the mouse, or press fire.

If both markers are displaced together from the visible CS2 player model, the discrepancy may instead be caused by game-side interpolation, old world positions/bones or compositor timing. **A zero-pixel delta does not mean the ESP matches CS2**: the two probes may both be stale relative to the game's rendered model.

## HUD / CSV columns

Starting in v1.2.10 the CSV samples **after each attempted Present** (up to 10 Hz); the
reported ages therefore use the completion timestamp of that same frame, not the
timestamp of the next frame's drawing pass. The display panel still shows the latest
known Present breakdown, i.e. from the previous frame.

- `pose_age_ms`: age of the latest published immutable pose frame when DXGI returned, not server or display latency (measured from publication/completion time starting v1.2.14).
- `world_age_ms`: age of the player read pass at the end of that Present attempt.
- `bone_age_ms`: age of this bone set at the end of that Present attempt; grows through fallback reuse.
- `pose_camera_age_ms`: age of the pose-frame camera at the end of that Present attempt.
- `camera_gap_ms`: time between the pose camera reading and the later camera reading at render time.
- `camera_projection_shift_px`: distance between the two 2D projections of the **same** bone.
- `camera_rotation_deg` (v1.2.12+): shortest-angle orientation difference between the two camera snapshots, in degrees.
- `camera_translation_units` (v1.2.12+): distance between their camera origins in game units (only this scalar is saved, never positions).
- `camera_prediction_shift_px` (v1.2.13+): pixel displacement between raw and bounded compensated projections of the same point; 0 when prediction is bypassed.
- `camera_prediction_applied` (v1.2.13+): 1 if the compensated green marker was valid for the sampled point, else 0.
- `pose_acquisition_ms` (v1.2.14+): time between starting the pose worker sampling pass and completing its immutable frame. This is acquisition time, not DXGI/compositor latency.
- `stale_world_players` (v1.2.15+): number of otherwise eligible players skipped during the pose pass because their world snapshot exceeded the visual freshness budget (50 ms).
- `stale_fallback_bones` (v1.2.15+): number of otherwise reusable cached bone sets that exceeded their visual budget (50 ms). Starting in v1.2.16, these counters are retained even in sampled frames with no visible target, as long as a pose frame exists.
- `probe_status` (v1.2.16+): 0=selected pose, 1=pose unavailable, 2=camera unavailable, 3=no visible pose, 4=invalid viewport, 5=probe not drawn. The 10 Hz recorder now logs presentation attempts even without an available visual pose. Position-dependent fields may be `-1` (not measured); do not interpret that sentinel as a real position/timing value.
- `present_call_ms`: time for the **current** `present_frame` operation (including retries, but not later GPU display time).
- `present_dropped`: whether this present attempt was dropped.
- `simulation_tick`: game tick value from the sampled player; negative values, such as -1 in the three previous captures, are invalid.
- `simulation_time` / `simulation_valid`: available game simulation-time fallback, or an explicit missing/invalid status.
- `sim_unchanged_ms`: time since this selected player's simulation tick or time **last changed while being observed**. It is **not** a measurement of server-to-client latency; -1 means unavailable.
- `frame_build_ms`: time from the start of frame construction to the end of ImGui/DX11 draw command submission; includes any synchronous work executed there.
- `pacing_wait_ms`: duration of the overlay frame pacing call immediately before building this frame. This may include waiting for DXGI's latency object or the FPS timer. A large value here is deliberately different from a large DXGI Present call time.
- `native_dxgi_present_ms`: summed wall time **inside** the actual `IDXGISwapChain::Present` calls.
- `present_retry_wait_ms`: summed time inside 1ms waits after `DXGI_ERROR_WAS_STILL_DRAWING`.
- `present_calls` / `present_retries`: number of native calls / times DXGI returned WAS_STILL_DRAWING during that present attempt.
- `pose_to_present_ms`, `world_to_present_ms`, `bone_to_present_ms`: source capture age **when Present returned**, not when the image reached the physical display.
- `draw_frames` / `repeated_pose_frames`: number of diagnostic draw frames and repeats of the same pose sequence during the 10Hz interval. A repeated pose isn't necessarily a failed bone read.
- `performance_mode` / `gpu_effects` / `display_refresh_hz`: active display settings, so ON/OFF captures cannot be mislabelled.
- `pose_target_hz`: configured asynchronous pose reader target (165 Hz when the display is correctly detected at 165 Hz and no lower explicit FPS limit was chosen).
- `gpu_vendor_id`: PCI vendor ID of the **actual DX11 rendering adapter** (e.g. 4318/NVIDIA, 4098/AMD). The overlay uses the same DX11 renderer; this does **not** claim a proprietary CUDA/ROCm rendering path.
- `fallback_bones`: whether this presentation used cached bones after a failed read.
- `pose_sequence`: pose frame generation, useful to spot repeated presentation of the same data.
- `elapsed_ms`: relative capture time.

The CSV is a fixed-memory **up to 10 samples/s** ring (up to 180 seconds). **It writes to disk only on the Save button**, never in the frame loop. New saves overwrite the previous CSV, so copy the file elsewhere before starting the next test if necessary.

## What this test does and does not establish

The world sampler nominally runs at 64 Hz. Starting in v1.2.11, the pose sampler runs on a **dedicated worker thread**, targeting the actual detected display refresh in Performance Mode (165 Hz on a correctly configured 165 Hz monitor). The DX11 overlay independently targets the same monitor refresh rather than the former 90 Hz cap. These are requested cadences, **not guaranteed delivered frames or fresh game-world snapshots**. The worker may reuse a bone set for up to 100 ms after a failed read; the renderer can also reuse a pose if the worker has not completed the next one. This tool timestamps the three acquisition stages independently.

The v1.2.15 visual freshness budget deliberately prefers temporarily missing
visuals over displaying out-of-date locations. Do not interpret a temporary
ESP gap as a new rendering failure without comparing the counters and world
sampler timing. The normal 165 Hz display target remains unchanged.

The latest camera projection and captured-pose camera projection help detect **inconsistent camera/pose sampling**; they are not a frame-perfect oracle for the CS2 renderer. Measuring absolute alignment to CS2 would require video-frame comparison or instrumentation unavailable in this passive overlay.

### Compare two runs without confounding settings

Capture 20–30s of each run on the same map, ideally with the same stationary bot:
1. **Performance ON / GPU Effects OFF**.
2. **Performance OFF / GPU Effects OFF**.
3. Optionally **Performance ON / GPU Effects ON**.

Save and **rename/copy the CSV** after each run, since subsequent saves overwrite
the same file. A few hundred rows is enough to compare the relative proportions of
`native_dxgi_present_ms`, `present_retry_wait_ms`, `pacing_wait_ms`,
`repeated_pose_frames` and `pose_to_present_ms`. These metrics cannot
directly measure the image delay in CS2's separate renderer or the display/compositor.

The probe itself remains passive and does not modify AIM, RCS, Triggerbot, game input, refresh cadence or DX11 Present mode. The independent v1.2.13 visual lookahead **does** change ESP/chams projection using a bounded predicted matrix, but does not publish it as the shared game camera.

The v1.2.12 motion scalars are computed from the same two camera snapshots already used by the probe: no extra remote reads. The new analyzer command `python tools/analyze_esp_sync.py <csv> --motion-split` splits samples by *observed* camera movement without screenshots. No observed camera delta does not prove that the user was stationary: CS2 camera snapshots and the 10 Hz recording cadence may miss intermediate movement. See [offline analysis](esp-sync-offline-analysis.md). Turn the probe OFF when done.
