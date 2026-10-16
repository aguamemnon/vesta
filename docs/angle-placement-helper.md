# Angle Placement Helper — Misc (v1.2.4 preview)

This prototype is a **passive, local geometry overlay**. It does not access
enemy locations, attacker status, crosshair targets, Aim Assist settings or
game movement inputs. It does not establish whether a position provides a
favorable peek or guarantee a first-sight advantage.

## Enable

Go to **Misc → Movement → Angle Placement Helper** (disabled by default).

The menu provides:
- **Show wall distance**: an approximate raycast measurement to the solid wall
  directly ahead at camera height.
- **Show lateral markers**: draw markers to either side of the player, aligned
  with the wall tangent, when a short sphere sweep and a downward floor ray
  succeed. These are examples of *geometrically reachable local positions*,
  not recommended tactical destinations.
- **Wall scan distance**: 64–300 units (192 default).
- **Marker spacing**: 20–96 units (40 default).
- **Refresh**: 0.1–1.0 seconds (0.25 default).
- **Guide color**: default light blue.

The renderer uses a small bounded number of collision queries per refresh.
It runs in the Misc render pass, regardless of whether player ESP is enabled.
Its settings are kept under `misc.m_angle_placement` in saved profiles.

## Limitations

- An approximate local wall probe is **not** 3D pathfinding. Stairs, floors,
  moving doors, and props can invalidate a visually accessible marker.
- The camera origin is an approximation of eye height; marker positions are
  independently snapped to surfaces found below the eye at that height.
- It does **not** score positions against enemy sightlines or use opponent
  state to identify favorable peek positions.
- Nothing moves the player or rotates the camera automatically.
- If the map collision model is unavailable, the feature draws nothing.

## Validation

1. Check there are no markers when the setting is disabled.
2. Verify the helper displays independently of the player ESP in Misc.
3. Face flat walls and 90° corners at various distances.
4. Verify side markers disappear when lateral clearance is blocked.
5. Test crouching, changing maps, and collision-model initialization.
6. Confirm menu setting persistence and unchanged Aim Assist operation.
