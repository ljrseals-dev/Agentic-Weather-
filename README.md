# Agentic Weather

A small C++20 terminal simulation in which three agent types (Sun, Cloud, Storm) interact with a 72 × 20 temperature/humidity field. The agents perceive local humidity gradients, steer, and alter the field. Clouds can mature into storms. The display uses ANSI truecolor and Unicode symbols.

> **This is a toy simulation, not a forecast.** Its parameters are heuristics, not calibrated meteorology; it has no real-world weather data or forecast capability.

## Build

### Linux / macOS (GCC or Clang)

```sh
g++ -std=c++20 -O2 -Wall -Wextra -Wpedantic -o weather agentic_weather_map.cpp
# or clang++
clang++ -std=c++20 -O2 -Wall -Wextra -Wpedantic -o weather agentic_weather_map.cpp
```

### Windows (MSVC)

```bat
cl /std:c++20 /O2 /EHsc agentic_weather_map.cpp
```

## Run

```sh
./weather
./weather --steps 500 --delay 40 --seed 1234
./weather --help
```

Options:
- `--steps N`: number of simulation frames (default 2000; non-negative).
- `--delay MS`: delay per frame in milliseconds (default 70; 0 disables delay).
- `--seed N`: random seed for repeatable runs (optional).
- `--help`: print usage.

Press **Ctrl+C** to stop. The terminal cursor and alternate screen are restored on normal exit.

## Controls / display

The background color encodes simulated temperature. Glyph density indicates humidity/precipitation intensity. Sun, cloud, and storm agents are drawn over the field. A terminal with ANSI color support is recommended; if your terminal/font lacks Unicode weather symbols, change `Unicode` to `false` near the top of the source.

## Secrets and credentials

**This project does not require, use, or contain API keys, passwords, tokens, cloud credentials, or other secrets.** It is entirely local and has no network calls.

If you add integrations in the future:
- Never commit secrets, access tokens, private keys, passwords, or credential files.
- Store local development secrets outside the repository (for example, in environment variables or an ignored local configuration file).
- Add any secret-file patterns to `.gitignore` before creating the file; do not rely on deleting a secret after it has been committed.
- Use GitHub Actions repository/organization **Settings → Secrets and variables → Actions** for CI credentials, and grant workflows only the permissions they need.
- Rotate/revoke a credential immediately if it is accidentally exposed, and review commit history because removing a secret from the current file does not erase it from prior commits.

No secret is needed to compile or run this simulation.

## License

MIT. See [LICENSE](LICENSE).
