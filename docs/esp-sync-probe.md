# ESP Sync Probe — Vesta v1.2.9

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

- `pose_age_ms`: time since the frame started collecting bones, not a server tick age.
- `world_age_ms`: time since the player data collection pass began.
- `bone_age_ms`: time since this player bone set was successfully read; this grows through fallback reuse.
- `pose_camera_age_ms`: time since the camera was read into the pose frame.
- `camera_gap_ms`: time between the pose camera reading and the later camera reading at render time.
- `camera_projection_shift_px`: distance between the two 2D projections of the **same** bone.
- `previous_present_call_ms`: wall time spent in the preceding DXGI Present call; **not measured end-to-end game/display latency**.
- `previous_present_dropped`: whether the preceding present attempt was dropped.
- `simulation_tick`: game tick value from the sampled player.
- `fallback_bones`: whether this presentation used cached bones after a failed read.
- `pose_sequence`: pose frame generation, useful to spot repeated presentation of the same data.
- `elapsed_ms`: relative capture time.

The CSV is a fixed-memory **10 samples/s** ring (up to 180 seconds). **It writes to disk only on the Save button**, never in the frame loop. New saves overwrite the previous CSV, so copy the file elsewhere before starting the next test if necessary.

## What this test does and does not establish

The world sampler nominally runs at 64 Hz. The rendering pose sampler runs at 60 Hz in Performance Mode and the DX11 overlay is capped at the lower of 90 Hz or monitor refresh. The code may reuse a bone set for up to 100 ms after a read failure. This tool timestamps the three acquisition stages independently.

The latest camera projection and captured-pose camera projection help detect **inconsistent camera/pose sampling**; they are not a frame-perfect oracle for the CS2 renderer. Measuring absolute alignment to CS2 would require video-frame comparison or instrumentation unavailable in this passive overlay.

The test does not modify the AIM, RCS, Triggerbot, game input, visual ESP policy, refresh cadence, or DX11 Present mode. Turn the probe OFF when done.
