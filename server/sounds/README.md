# Roll-a-Ball Derby — Server Sound Files

Place WAV files in this directory and the server will play them automatically on game events.

## Required files

| File | Trigger |
|------|---------|
| `score_0.wav` | A player scores and earns 0 points (passed through) |
| `score_1.wav` | A player scores 1 point |
| `score_2.wav` | A player scores 2 points (streak) |
| `score_3.wav` | A player scores 3 points (big streak) |
| `game_started.wav` | "GO!" — game transitions to running |
| `game_paused.wav` | Game paused by admin |
| `game_resumed.wav` | Game resumed |
| `game_reset.wav` | Game reset to idle |
| `countdown_tick.wav` | Each countdown beat (3, 2, 1…) |
| `countdown_go.wav` | Countdown reaches zero |
| `winner.wav` | A player crosses the finish line |
| `took_lead.wav` | A player takes the lead |
| `became_last.wav` | A player falls to last place |
| `streak_zero.wav` | A player's scoring streak is broken |
| `streak_three.wav` | A player reaches a 3-score streak |

## Configuration

Sound playback is controlled via environment variables in `.env`:

```
SOUND_ENABLED=true        # Set to 'false' to disable all audio
SOUND_PLAYER=aplay        # Optional: override OS audio player (default: auto-detect)
```

## Notes

- Missing files are silently skipped — the game will run fine without them.
- Supported formats depend on your OS audio player (WAV recommended for broadest compatibility).
- On Windows the server uses PowerShell to play audio. On Linux, `aplay` or `mpg123`. On macOS, `afplay`.
- Run `npm install` in `server/` after adding `play-sound` to `package.json` if you haven't already.
