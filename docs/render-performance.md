# Overlay renderer: low-latency performance mode (v1.2.7)

## The performance bottlenecks found during the rendering review

1. **Pose collection is synchronous on the overlay render thread.** When an active player ESP requests poses, the render thread reads skeleton/bone data from the game process. The previous scheduling policy returned `0` (unlimited) for a frame-capped overlay at 240 FPS, so it could attempt bone reads **every rendered frame**. This is potentially expensive even when the ESP polygons are simple.
2. **The DX11 overlay rendered at the configured high frame rate (240 FPS by default).** A transparent full-screen swap chain and ImGui commands compete with the game for GPU/CPU time.
3. **The secondary 2D bloom pass executes for the overlay even with the menu closed.** It can be disabled independently of the main DX11 compositor and other effects.

The above are identified code paths, **not measured root causes on the user's machine**. Do not claim a numerical FPS improvement without an actual game benchmark.

## Performance Mode

Access: **Misc → General → Engine → Performance Mode (90/60 Hz)**.

It is enabled by default (including when loading older profiles without the flag). With Performance Mode enabled:

- Overlay frames are capped at **90 Hz**, or the user's lower configured cap, even if the global FPS limiter is disabled.
- Presentation bone/pose sampling is limited to **60 Hz**. Cached poses continue to render between reads. A slow sample never schedules catch-up reads.
- The postprocess **2D bloom pass is skipped**; the underlying DX11 overlay, ImGui UI, and main chams/world rendering continue to work.
- Threat Direction from v1.2.6 remains compact (one opponent, three short lines, no extra collision raycasting).
- Turning off Performance Mode restores previous pacing, sampling, and postprocessing behavior so users can compare.

The mode does not alter CS2's own FPS limiter or its graphics settings.

## Practical A/B verification

Use the **same map, camera angle, display mode, and video settings** for every run, allow a brief warm-up, then record CS2 average FPS, 1% lows, and obvious input stutters for 60 seconds:

1. Vesta closed.
2. Vesta running with Performance Mode enabled; disable unused features.
3. With Vesta running, turn Performance Mode off and observe whether stuttering returns.
4. If lag persists in Performance Mode, toggle **Misc → General → Engine → GPU Effects** off as a separate experiment. This can disable chams/effects; do not leave it off if those features are required.
5. Compare with Player ESP and Misc Geometry Probe disabled to isolate snapshot or collision costs.

Do **not** evaluate smoothness from overlay FPS alone. The goal is reducing pressure on CS2's frametimes and avoiding game input stalls.

## Further instrumentation if stutters remain

The project already has aggregate profiling zones for `render_frame`, `wait_frame_latency`, `pose_sample`, `chams`, `imgui_render`, `bloom_2d`, and `present`, compiled when `VESTA_PERF_LOG` is enabled. Collecting such traces in a separate diagnostic build is preferable to continuously writing logs in the normal release.

No change was made to the Aim Assist, game input paths, or any automatic movement logic.
