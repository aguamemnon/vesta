<div align="center">

<img src=".github/assets/vesta-visuals.png" alt="Vesta Visuals menu" width="100%">

Vesta is a configurable Windows x64 project with combat assistance, a complete
visual system, movement tools, grenade lineups, an in-game radar, and a
sandboxed Lua API. The native client stays external: no injection and no driver.

**A feature-complete external cheat for Counter-Strike 2.**

<p>
  <a href="https://github.com/aguamemnon/vesta/releases"><img src=".github/assets/icon-download.svg" width="16" height="16" alt=""> <strong>Download</strong></a>
  &nbsp;&nbsp;&nbsp;
  <a href="https://read1dno.github.io/vesta/"><img src=".github/assets/icon-website.svg" width="16" height="16" alt=""> <strong>Website</strong></a>
  &nbsp;&nbsp;&nbsp;
  <a href="lua/docs/en/README.md"><img src=".github/assets/icon-docs.svg" width="16" height="16" alt=""> <strong>Lua API</strong></a>
</p>

</div>

## Fork preview: ESP Sync Probe v1.2.14

The **Performance Mode** setting at **Misc → General → Engine** keeps
Vesta's external DX11 overlay independent of CS2's own FPS limit.

- In v1.2.11+ Performance Mode targets the **detected display refresh**
  (165 Hz on a 165 Hz screen), respecting a lower explicit user limit. The
  asynchronous pose worker targets the same display rate; world snapshots
  remain bounded by their independent sampler cadence.
- 2D bloom CPU collection and the extra postprocessing pass are disabled in
  Performance Mode. Turn it off to compare original behavior.
- v1.2.8 additionally **coalesces settings writes** outside the menu to avoid
  repeatedly writing to disk on the render thread while a script edits settings.
- [Renderer performance details and A/B testing](docs/render-performance.md).

**v1.2.9:** Optional **Misc → General → Engine → ESP Sync Probe** visualizes
one enemy head projected with the **live camera** (cyan circle) and the
**pose-captured camera** (orange cross), displays pose/world/bone ages, and
records up to three minutes at 10 Hz. Click **Save ESP Sync CSV** to export
`vesta-esp-sync.csv` into the Logs folder; nothing is logged to disk until
you click Save. [Test protocol](docs/esp-sync-probe.md).

**v1.2.10:** The CSV now logs **the same frame through pose capture, build, DXGI
Present, and completion**, with native `Present` calls separated from retries
and frame pacing. It counts repeated pose frames, records valid/invalid
simulation timestamps without fabricating missing ticks, and stamps the
actual Performance Mode / GPU Effects state of each recording. This is a
diagnostic-only update; normal ESP and combat behavior are unchanged.

**v1.2.14:** The camera lookahead now advances a single rigid orientation
and eye translation, preserving perspective row geometry instead of
independently extrapolating 16 matrix elements. It rejects invalid,
incoherent or discontinuous camera samples and maintains the same 4 ms /
1.5 degree / 3 unit budget. **Misc → General → Engine → Visual Camera
Prediction** enables/disables it independently of the 165 Hz performance
settings (enabled by default, persisted per config). Pose worker frames now
use their **publication time** for freshness, while the probe exports
`pose_acquisition_ms` separately to distinguish read time from Present time.
Windows Release CI compiles and runs an independent C++ camera regression
target before packaging. These are experimental visual changes, not a
verified improvement in CS2's actual end-to-end input-to-display latency.

**v1.2.13:** Experimental visual-camera lookahead now applies a short, bounded
extrapolation of the last two camera matrices to the ESP and chams render
projection only (superseded by rigid-camera extrapolation in v1.2.14). The lookahead follows a smoothed fraction of the previous
DXGI Present duration and cannot exceed 4 ms, 1.5 degrees or 3 game units
per frame. It automatically bypasses FOV changes, camera discontinuities,
and sharp reversals. The real shared camera state, game input, aim, world
sampling cadence and 165 Hz refresh policy are unchanged. The Sync Probe
retains its *raw* camera and pose comparison and adds a green compensated
marker and two CSV columns for validation. This experimental compensation
may not match CS2's independent render timing on every system; check
alignment in-game.

**v1.2.12:** The passive ESP Sync Probe adds camera rotation and translation
scalars to the end of its CSV. `tools/analyze_esp_sync.py` can now compare
projection discrepancy against observed camera movement without a video
capture; see [offline analysis](docs/esp-sync-offline-analysis.md). No camera,
ESP, aim, DX11 present or refresh behavior is changed by these measurements.

**v1.2.11:** Performance Mode now targets the display refresh (165 Hz on
a 165 Hz monitor) instead of fixed 90/60 Hz caps. Player bone sampling was
moved off the DX11 render thread into a dedicated, interruptible worker
publishing immutable pose frames; no additional game-world tick frequency is
claimed (the world sampler remains 64 Hz). Actual DX11 GPU vendor (NVIDIA,
AMD or other) is detected and shown in the menu, and stamped into the
diagnostic CSV. The optimized backend remains portable DirectX 11; it does
not load untested vendor-specific DLLs. FPS targets can still be limited by
hardware, reads and compositor pacing.

Other experimental, optional features remain available: Anti-Aim Observer,
Duel History, compact **Threat Direction** (maximum one target, no collision
raycasts in the draw loop), and **Wall Geometry Probe** in Misc. The latter
does not calculate an optimal peek route. See [orientation diagnostics](docs/anti-aim-duel-history.md)
and [local geometry limitations](docs/angle-placement-helper.md).

Windows builds are published through [GitHub Releases](https://github.com/aguamemnon/vesta/releases).

## Quick start

1. Download [`vesta.exe`](https://github.com/aguamemnon/vesta/releases).
2. Start Counter-Strike 2.
3. Run `vesta.exe`.
4. Press `Insert` to open the menu. Press `End` to close Vesta.

That is all you need. Profiles are managed from **Misc → Configs**. On a compact
keyboard, change the menu and exit keys in `%TEMP%\vesta\configs\hotkeys.cfg`, then
restart Vesta.

## Important

> [!CAUTION]
> **Lua scripts.** Scripts run in a sandbox without file, process or network
> access; only the bundled Vesta Web Radar, verified by its SHA-256, runs outside
> it. A script can still change your settings and press game keys while CS2 is in
> focus, so run only scripts you trust. Vetted scripts are published in this repository and on the Vesta website.

> [!TIP]
> **Performance.** An external overlay shares CPU and GPU resources with the
> game. If ESP appears delayed or the overlay FPS is low, select the **Fullscreen**
> render mode and set model, texture, shader, and particle detail to Low in CS2.

> [!WARNING]
> **VACnet / VAC Live — 9 October 2026.** Valve's servers appear to be running a
> VACnet model with high Recall and low Precision.
>
> - Any Triggerbot use results in a cooldown, including seemingly ordinary taps without cheats.
> - Aimbot and RCS use result in cooldowns.
> - Kills while jumping, through smoke, against players at spawn (even with a knife or grenades), or through
>   walls can trigger restrictions.
> - The SSG 08 is effectively unusable. On a brand-new, clean account, I received a cooldown within the first
>   five rounds of my first matchmaking game after getting completely legitimate kills with it. The weapon
>   appears to be fundamentally broken from a detection standpoint.
>
> The inference model running on Valve's servers appears to be a relatively simple DL model trained on sparse
> data with an implicit target.
>
> Essentially, when playing with WH only, the penalty may be triggered not by aiming through walls, but by the
> kill event itself. The model may be evaluating the timing of the kill relative to the opponent's first
> appearance from behind cover.
>
> In effect, WH can manually replicate Triggerbot behavior, with the time delta between the opponent's first
> visibility and the kill acting as a key detection signal.

## Features

Open a section only when you need its settings.

<details>
<summary><strong>Aimbot and RCS</strong></summary>

- Always, hold, or toggle activation with human-readable binds.
- Fixed, distance-scaled, and target-attached FOV modes.
- Configurable hitboxes, multipoints, prediction, smoothing, and humanizer.
- Visibility, smoke, flash, airborne, immunity, penetration, and lethal checks.
- Independent recoil control that also combines with the active aim solution.
- Global settings and overrides for every weapon group.

</details>

<details>
<summary><strong>Triggerbot and Auto Stop</strong></summary>

- Conventional and deterministic seed-based firing.
- Pre-shot delay, post-shot lockout, timing randomization, and rare outliers.
- Hitbox groups, prediction, penetration, minimum damage, and lethal logic.
- The head remains available when a low-damage weapon has no lethal body point.
- Predictive Auto Stop prepares weapon accuracy before a requested shot.
- Shared global and per-weapon profiles.

</details>

<details>
<summary><strong>Player ESP and Chams</strong></summary>

- Boxes, skeleton, head marker, view line, name, health, armor, weapon, ammo,
  distance, money, ping, and information flags.
- Drag-and-drop ESP Editor with bar anchors and contextual settings.
- Off-screen arrows, sound ESP, hitboxes, bloom, on-hit poses, and death effects.
- Separate visible and occluded chams materials with per-pixel visibility.
- Legit Sync for direct visibility, radar, sound, and smoke.
- Spectator Sync can hide the complete player presentation while observed.

</details>

<details>
<summary><strong>World, projectiles, and bomb</strong></summary>

- Dropped weapons and items with icons and distance.
- Projectile timers, bounce points, predicted paths, grenade radii, and inferno
  bounds.
- Planted-bomb world marker and compact timer/defuse panel.
- Bomb Safe Zone calculated from blast damage and map collision.
- Bullet tracers, hit markers, hit sounds, damage popups, and event log.
- Crosshair reconstruction from the active in-game crosshair settings.

</details>

<details>
<summary><strong>Radar</strong></summary>

- Players, view direction, health, armor, weapons, money, kits, grenades, bomb,
  dropped items, projectiles, and inferno bounds.
- Predicted grenade paths and accurate projectile lifetime.
- Independent visual controls and Always, Hold, or Toggle activation.
- Optional browser radar through the portable Lua script.

</details>

<details>
<summary><strong>Grenades and movement</strong></summary>

- Configurable grenade trajectory, bounces, endpoint, thickness, color, and
  bloom.
- Nade Helper lineup database with aim alignment, movement, timed jump, and
  automatic release.
- Bunny Hop, Edge Jump, and predictive Auto Stop share one input controller.

</details>

<details>
<summary><strong>Interface and automation</strong></summary>

- Movable watermark, spectator list, keybind list, event log, and bomb panel.
- English and Russian interface, DPI scaling, custom palette, and portable
  layouts.
- Popup-based advanced settings and configurable lifecycle hotkeys.
- Auto Accept performs a single external confirmation click per match prompt.

</details>

<details>
<summary><strong>Lua API</strong></summary>

- Sandboxed Lua 5.4 runtime with hot reload, isolated errors, resource budgets,
  storage, UI controls, drawing, and immutable game snapshots.
- Read the [API documentation](lua/docs/en/README.md) or browse the
  [script directory](lua/scripts).
- [Vesta Web Radar](lua/scripts/Vesta%20Web%20Radar.lua) is distributed as one
  portable Lua file. Its Cloudflare tunnel requires a VPN in Russia.

</details>

## Profiles

| Profile | Use case |
|---|---|
| [`Full legit`](configs/full-legit.cfg) | Legit Sync ESP and basic assistance without additional hidden information. |
| [`Legit`](configs/legit.cfg) | Wall information and a basic trigger configuration for playing against WH. |
| [`Semi rage`](configs/semi-rage.cfg) | Seed Trigger and aggressive settings, including deterministic jump shots. |

Download a profile and import it from **Misc → Configs**.

## Official builds

On this fork, every successful Windows x64 build pushed to `main` or
`feature/**` is configured to publish a versioned GitHub Release containing
`vesta.exe` and `SHA256SUMS.txt`. Feature-branch builds are marked as
pre-releases. Download from [this fork's Releases](https://github.com/aguamemnon/vesta/releases).
Compare the binary's SHA-256 with `SHA256SUMS.txt`:

```powershell
(Get-FileHash .\vesta.exe -Algorithm SHA256).Hash
```

This pipeline requires GitHub Actions to be enabled for the fork and write
permission for the release job. A build is not a security audit.

<details>
<summary><strong>Build from source</strong></summary>

Install Visual Studio 2022 with **Desktop development with C++**, CMake 3.20+
and Git, then run:

```powershell
cmake --preset release
cmake --build --preset release --parallel
```

The executable is written to `build/bin/vesta.exe`. CMake downloads the pinned
third-party dependencies (Dear ImGui, Lua, FreeType, meshoptimizer, LZ4, Zstandard,
nlohmann/json) unmodified during the first configure; the repository vendors none
of them.

</details>

## Help and development

- Found a reproducible problem? [Report a bug](https://github.com/Read1dno/vesta/issues/new?template=bug-report.yml).
- Have a focused idea? [Request a feature](https://github.com/Read1dno/vesta/issues/new?template=feature-request.yml).
- Want to submit a change? Read [Contributing](.github/CONTRIBUTING.md).
- Community discussion lives in the [UnknownCheats thread](https://www.unknowncheats.me/forum/counter-strike-2-a/764247-vesta-external.html).

After a CS2 update, use the latest Vesta build before reporting changed offsets
or signatures.

## License

Licensed under [Apache 2.0](LICENSE). Third-party notices are listed in
[`THIRD_PARTY_NOTICES.md`](THIRD_PARTY_NOTICES.md).

Educational and research use only. You are responsible for complying with the
terms of the software you use it with.

<div align="center">

[Website](https://read1dno.github.io/vesta/) · [UnknownCheats](https://www.unknowncheats.me/forum/counter-strike-2-a/764247-vesta-external.html) · [Telegram](https://t.me/readidno)

</div>
