# Display Client

Fullscreen Pixi.js SPA served at `/display` — designed for a beamer or TV showing the live race.

## What it does

- Renders one horizontal lane per connected player
- Player figures animate smoothly left → right as scores arrive
- **Action effects pipeline** — 8 event types with distinct visual treatments:
  - `zero_roll` → shrink-bounce + red flash + 😢 popup
  - `score_1` → scale-bounce + white tint flash
  - `score_2` → bigger bounce + blue lane flash
  - `score_3` → large bounce + gold flash + ⭐ "+3!" popup
  - `streak_zero_3x` → dark pulse + 😭 "3× ZERO" popup
  - `streak_three_2x` → orange glow + 🔥 "HOT!" popup
  - `took_lead` → gold aura + 👑 "LEAD!" popup
  - `became_last` → dark-red flash + 👎 "LAST!" popup
- Winner overlay with confetti plays when a winner is declared
- Status overlays for **WAITING FOR PLAYERS** (idle) and **⏸ PAUSED** states
- Connection status dot (top-right corner): green = connected, yellow = connecting, red = disconnected
- Auto-reconnects on WebSocket disconnect with exponential back-off
- Theming support: horse 🐎 and camel 🐪 — figures and palettes update with the active theme

## How to open

Start the server, then navigate to:

```
http://<server-ip>:3000/display/
```

Add `?fullscreen=1` to auto-request browser fullscreen:

```
http://<server-ip>:3000/display/?fullscreen=1
```

## File structure

```
clients/display/
├── index.html                  Entry point; loads Pixi.js + gsap
├── css/style.css               Fullscreen reset, status-dot styles
├── vendor/                     Pixi.js + gsap bundled for offline/LAN use
└── js/
    ├── main.js                 Pixi app init, WS message routing
    ├── connection.js           WebSocket client (registers as 'display')
    ├── ThemeManager.js         Loads theme.json; provides colors & URLs
    ├── scene/
    │   ├── RaceTrack.js        All-lanes container, layout, resize
    │   └── Lane.js             Single-player lane (bg, track, sprite, name, flash overlay, popup container)
    └── effects/
        ├── ActionEffect.js     Event-driven effect dispatcher (8 event types)
        ├── ScoringEffect.js    Scale-bounce + tint-flash helper
        └── WinnerOverlay.js    Full-screen celebration + confetti
```

## Dependencies (bundled in vendor/, no build step)

| Library | Version | Purpose |
|---------|---------|---------|
| [Pixi.js](https://pixijs.com/) | v8 | 2D WebGL rendering |
| [gsap](https://gsap.com/) | v3 | Tweening / animation |

## WebSocket messages consumed

| Type | Payload | Action |
|------|---------|--------|
| `state` | Full game state | Rebuild lanes, update positions, show/hide overlays |
| `scored` | `{ playerId, playerName, points, newPosition, events }` | Tween figure + trigger action effects for each event |
| `winner` | `{ playerId, name }` | Winner overlay + confetti |
