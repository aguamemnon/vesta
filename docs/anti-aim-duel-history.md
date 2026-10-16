# Anti-Aim Observer / Duel History (experimental)

Version: 1.2.2-preview. The observer is an opt-in diagnostic, not a resolver.

## Settings

In **Visuals → Players**, enable **AA observations** to collect per-player orientation anomalies.
**Record combat observations** enables the separate duel timeline. Both default to **off**.
Click **Open duel history** to view the latest events, and **Export CSV** to save
`vesta-aa-duel-history.csv` under Vesta's `research_logs` folder.

## What the data means

- `orientation_alert_changed`: the heuristic changed to orange or red. This is *not* evidence of cheating.
- `enemy_fired_observed`: an enemy's observed `last_fired_time` advanced.
- `enemy_damaged_unattributed` / `enemy_died_unattributed`: an enemy's sampled health changed; **no attacker identity has been verified**.
- `local_damaged_unattributed` / `local_died_unattributed`: a local health change without verified attacker.
- `aim_target_observed_not_fired`: the Aim Assist selected a target; this is **not** a shot or hit. The stored FOV is the selected target's angular error at selection time.

Records store time, player identity per pawn/life, sampled enemy yaw, camera yaw, AA status,
simulation tick and health changes. Event history is capped at 512 entries in memory.
Aim selections are sampled at most every 250 ms per target. Export is requested manually,
not continuously written. The CSV includes `attacker_confirmed=false` for all sampled
health-change records.

### Important limitations

- Some kills and deaths may not be detected if the corresponding pawn snapshot is absent.
- The client may not have authoritative server hit information; a death does not prove the
  victim had Anti-Aim, and missing shots do not prove a resolver failure.
- Camera yaw is **not** a measurement of the mouse delta issued by the Aim Assist.
- A restart clears the in-memory history, so export before closing Vesta.
- Reused pose data and fallback eye angles are excluded from the orientation heuristic.
- To identify real attackers, use independently verified server or demo events in a future version.

## Validation checklist

1. Compile the Windows x64 configuration in GitHub Actions (Release preset).
2. Run with the observer off; original ESP and combat behavior should be unchanged.
3. Enable the observer; confirm no errors when players join, die or respawn.
4. Enable duel logging. Verify health events, enemy firing and aim target selections.
5. Verify all health changes are described as **unattributed**.
6. Export CSV; inspect quoting, column count, timestamps and AA status.
7. Evaluate false positives on legitimate quick mouse turns and dropped/duplicate ticks.

No Ragebot, aiming resolver, attacker assumption, auto-fire change or aim steering code is
added by this analyzer.
