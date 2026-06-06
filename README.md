# Space Dilemma

> 🇵🇹 [Versão em Português disponível aqui](README-PT.md)

Simulation of a monitoring system for a chemical mixing chamber aboard a spacecraft, developed in C++ with OpenGL/freeglut.

## Context

During a spacecraft refuelling operation, two unstable chemical components — **Alfa** and **Beta** — are combined in a mixing chamber. Their ratio is influenced by external factors (hose pressure, chamber pressure, temperature) and must be constantly monitored by an operator:

- If **Alfa > 70%**, the system enters **pre-ignition**
- If **Beta > 80%**, **valve corrosion** occurs
- If either substance reaches **100%**, a **critical failure** occurs

## Features

- Visual bars for Alfa and Beta with alert thresholds
- Always-visible alert topbar
- External factors panel with real-unit values (bar, kPa, °C)
- External factor phase system (Stable → Drift → Critical)
- Two navigable screens:
  - **Screen 1** — Real-time monitoring with clickable buttons
  - **Screen 2** — Call the team to stabilise external factors
- Temporary notification when the team finishes a stabilisation
- Log system in `logs.txt` with timestamps for all relevant events

## Technologies

- **C++17**
- **OpenGL** — 2D rendering with orthographic projection
- **freeglut** — window, keyboard, mouse and timer management

## Build

### Linux / macOS

Install dependencies (Ubuntu/Debian):
```bash
sudo apt install freeglut3-dev
```

Build:
```bash
g++ -o space_dilemma main.cpp -lGL -lGLU -lglut
```

### Windows (MinGW)

```bash
g++ -o space_dilemma main.cpp -lfreeglut -lopengl32 -lglu32
```

## Running

```bash
./space_dilemma
```

The `logs.txt` file is created automatically in the same folder as the executable.

### Alternative with Just

If you have [just](https://github.com/casey/just) installed, you can build and run with a single command:

```bash
just run
```

## Controls

### Screen 1 — Monitoring

| Key | Action |
|---|---|
| `A` | Increase Alfa by 5% |
| `B` | Increase Beta by 5% |
| `H` | Print system state to console |
| `2` | Go to Screen 2 |

The Alfa and Beta buttons are also directly clickable on screen with the mouse.

### Screen 2 — Control

| Key | Action |
|---|---|
| `M` | Call team for Hose Pressure |
| `C` | Call team for Chamber Pressure |
| `T` | Call team for Chamber Temperature |
| `1` | Return to Screen 1 |
