# Roll-a-Ball Derby — Game Assets

Shared SVG assets and theme manifests for all clients (display, web admin, preview).

## Directory Structure

```
clients/assets/
└── themes/
    ├── shared/
    │   ├── player-colors.json    — 16-colour palette (hex + Pixi 0x values)
    │   └── preview.html          — Animated asset catalog + colour swatches
    ├── horse/
    │   ├── sprite.svg            — Horse + jockey, flying gallop pose
    │   ├── track-bg.svg          — English countryside (800×100)
    │   ├── finish-flag.svg       — Classic 4×4 checkered flag
    │   └── theme.json            — Theme manifest
    └── camel/
        ├── sprite.svg            — Dromedary + rider, keffiyeh streaming
        ├── track-bg.svg          — Arabian desert sunset (800×100)
        ├── finish-flag.svg       — Arabian tri-band pennant
        └── theme.json            — Theme manifest
```

All assets are served by the game server at `/assets/` (mapped from `clients/assets/`).

## Sprite Design

All sprites use **pure white fills** (`#ffffff`) with grey shading for details. This makes them fully tintable by Pixi.js:

```js
sprite.tint = 0xE53E3E; // red player
```

Grey shading (e.g. helmet at `#d9d9d9`) renders at 85% of the tint colour — natural shading is preserved.

## theme.json Schema

```json
{
  "id": "horse",
  "name": "Horse Derby",
  "description": "...",
  "assets": {
    "sprite": "/assets/themes/horse/sprite.svg",
    "trackBg": "/assets/themes/horse/track-bg.svg",
    "finishFlag": "/assets/themes/horse/finish-flag.svg"
  },
  "palette": { "trackGrass": "#4a7c59", ... },
  "playerColors": [
    { "index": 0, "name": "Racing Red", "hex": "#E53E3E", "pixi": "0xE53E3E" },
    ...
  ]
}
```

## Player Colours

Defined in `shared/player-colors.json`. 16 colours, indexed 0–15. The display client assigns colour by player index. The web admin mirrors the same palette via an inline copy in `state.js`.

Preview all assets: open `themes/shared/preview.html` directly in a browser (SVGs loaded via `fetch`).
