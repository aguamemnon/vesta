# Threat Vision Zone — v1.2.3-preview

**Status: experimental. This is not a hit probability, real visibility check,
or authoritative line-of-fire test.**

## Access and controls

In **Visuals → Players → Skeleton & Hitboxes**, enable
**Threat Vision Zone** and open its settings popup:

- **Clip to map walls** (default on): horizontal rays from an estimated eye
  height terminate at the first collision. If the collision map is unavailable,
  nothing is shown until it is ready.
- **Suspected enemies only** (default off): display only enemies assessed as
  suspicious or repeated anomalies by the optional Anti-Aim Observer. Enable
  that observer separately before using this filter.
- **Fill danger sector**: translucent triangles; can be disabled to show only
  sector boundaries.
- **Half angle**: 5–85 degrees; a 35° default half-angle gives a 70° sector.
- **Range**: 150–2000 Source units. **Ray count**: 6–24.
- **Max enemies**: 1–8 nearest visible enemies; default 4.
- **Refresh**: 0.08–0.8 seconds; recomputes earlier if a target moves or
  turns significantly, with a minimum update interval.
- Separate colors for normal (blue), suspicious (orange) and persistent
  anomalies (red).

All settings are saved in the current Vesta ESP profile and remain disabled by
default for backward compatibility.

## Rendering and performance

The renderer selects the nearest live enemy pawns whose sampled pose is valid,
then traces at most **24 rays x 8 players** on a cache refresh. World
geometry is sampled at approximate eye height; polygon triangles are projected
onto a flat plane at the target's feet. Map changes clear the per-player cache.
Rays are not recalculated at the ESP display frame rate. It never changes the
Aim Assist, mouse input, hitbox selection or firing behavior.

## Accuracy and limitations

1. Yaw is the last available observed eye angle, not necessarily the enemy's
   true server view, especially under Anti-Aim.
2. Wall clipping only applies in the horizontal ray plane. Vertical obstacles,
   doors, floors, uneven terrain and elevations can make the projected footprint
   misleading.
3. Enemy scope magnification, weapon range, recoil, spread, network latency and
   the actual threat level are **not** modelled.
4. The markers show the horizontal directions an opponent might cover, not
   proof the opponent sees you or that you would take damage.
5. The overlay is skipped for invalid eye angles, missing collision maps with
   clipping enabled, reused bone caches, dead players, and players filtered by
   the ESP's own visibility rules.

## Manual validation

- Confirm no change when the option is off.
- Test map-wall clipping on known 90° corners and a long flat corridor.
- Test with a ducking/standing target, a target switching view direction,
  and a map change.
- Check transparency and ESP consistency at different screen resolutions.
- Confirm CPU/frame-time remains acceptable with 8 targets and 24 rays.
- Test `Suspected enemies only` with the AA observer on and off.
