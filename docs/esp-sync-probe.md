# ESP Sync Probe — Vesta v1.2.10

This **opt-in, passive** tool tests why the Player ESP box **and** skeleton appear behind an opponent when the observer strafes, even with a stationary bot.

## Where to find it

1. Start CS2 and Vesta. Leave **Performance Mode** enabled; keep your usual ESP box/skeleton.
2. Open **Misc → General → Engine → ESP Sync Probe**. It defaults to OFF.
3. A small panel appears near the top-right of the game. Pick a bot on screen.
4. With a **stationary bot**, stand still for a few seconds, then strafe quickly left-right for ~20 seconds.
5. Repeat while the bot moves and finally while you also move. Avoid changing CS2 graphics/settings between stages.
6. Open Misc again and click **Save ESP Sync CSV**, then **Logs**. Send the file
   `%TEMP%\vesta\logs\vesta-esp-sync.csv` alongside a screenshot or short recording of the markers.
7. **Clear Sync Samples** starts a fresh capture for another test.

Nothing is transmitted over the network. The file contains only numerical timing / projection measurements, no player names, Steam IDs, physical addresses, or game world coordinates.

## On-screen marker legend

- **Cyan circle (O):** one enemy head bone projected by the latest successfully sampled camera matrix, as the normal ESP does.
- **Orange cross (X):** **the same exact head bone** projected by the camera matrix captured with the player pose frame.
- **Connecting line:** purely the on-screen **camera/projection discrepancy** for an identical 3D point, measured in pixels.
- The sampled target is the on-screen enemy pose closest to screen center. The probe does not switch targets, move the mouse, or press fire.

If both markers are displaced together from the visible CS2 player model, the discrepancy may instead be caused by game-side interpolation, old world positions/bones or compositor timing. **A zero-pixel delta does not mean the ESP matches CS2**: the two probes may both be stale relative to the game's rendered model.

## HUD / CSV columns

Starting in v1.2.10 the CSV samples **after each attempted Present** (up to 10 Hz); the
reported ages therefore use the completion timestamp of that same frame, not the
timestamp of the next frame's drawing pass. The display panel still shows the latest
known Present breakdown, i.e. from the previous frame.

- `pose_age_ms`: age of the pose frame when the DXGI call returned, not server or display latency.
- `world_age_ms`: age of the player read pass at the end of that Present attempt.
- `bone_age_ms`: age of this bone set at the end of that Present attempt; grows through fallback reuse.
- `pose_camera_age_ms`: age of the pose-frame camera at the end of that Present attempt.
- `camera_gap_ms`: time between the pose camera reading and the later camera reading at render time.
- `camera_projection_shift_px`: distance between the two 2D projections of the **same** bone.
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
- `fallback_bones`: whether this presentation used cached bones after a failed read.
- `pose_sequence`: pose frame generation, useful to spot repeated presentation of the same data.
- `elapsed_ms`: relative capture time.

The CSV is a fixed-memory **up to 10 samples/s** ring (up to 180 seconds). **It writes to disk only on the Save button**, never in the frame loop. New saves overwrite the previous CSV, so copy the file elsewhere before starting the next test if necessary.

## What this test does and does not establish

The world sampler nominally runs at 64 Hz. The rendering pose sampler runs at 60 Hz in Performance Mode and the DX11 overlay is capped at the lower of 90 Hz or monitor refresh. The code may reuse a bone set for up to 100 ms after a read failure. This tool timestamps the three acquisition stages independently.

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

The test does not modify the AIM, RCS, Triggerbot, game input, visual ESP policy, refresh cadence, or DX11 Present mode. Turn the probe OFF when done.
