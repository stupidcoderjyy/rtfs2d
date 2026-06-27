# rtfs2d — Real-Time Fluid Simulator 2D

A GPU-accelerated 2D incompressible Navier-Stokes fluid solver with Vulkan compute shaders, real-time visualization, and interactive controls.

## Features

- **Stable Fluids Algorithm** — semi-Lagrangian advection, Jacobi diffusion, pressure Poisson projection, all executed in Vulkan compute shaders
- **Four Boundary Conditions** — no-slip wall, slip wall, velocity inlet/outlet, pressure inlet/outlet, per-side configurable with multiple segments
- **Immersed Boundary Method (IBM)** — arbitrary closed polygons as obstacles; Lagrangian marker particles interpolate velocity, compute restoring force, and spread feedback to the grid
- **Physics-based Grid** — configurable mesh resolution and physical density (`dx = dy`), all shaders use specialization constants `INV_DX`/`INV_DY` for correct differencing
- **Multi-field Visualization** — speed magnitude, pressure, vorticity (with obstacle-aware block-averaged smoothing to suppress stair-step artifacts)
- **Three Color Gradients** — grayscale, jet rainbow, cool/warm diverging; runtime switchable via ImGui
- **Passive Dye Scalar** — semi-Lagrangian advection with radial ink injection on mouse press; three-layer color blending (low/mid/high) with per-layer color pickers
- **JSON Case Files** — grid params, boundary conditions, obstacle geometry (supports `C`, `P` expression syntax) loaded from a single JSON document
- **ImGui Control Panel** — pause/resume, reset, init field; field mode (gradient, field type, coefficient slider); dye mode (radius, three 0xRRGGBB color pickers); time step slider; fullscreen toggle; keyboard shortcut (Space to pause)
- **Single Command Buffer** — compute and graphics recorded in one command buffer per frame, submitted once

## Requirements

| Dependency | Version / Notes |
|---|---|
| C++20 compiler | GCC 13+, Clang 17+, MSVC 2022 |
| CMake | 4.0+ |
| Vulkan SDK | 1.3+ |
| GLFW | 3.4+ |
| spdlog | 1.x |
| nlohmann/json | 3.x |
| Dear ImGui | 1.91+ (included as git submodule under `thirdparty/imgui`) |
| Windows | MinGW-w64 via MSYS2 (primary tested platform) |

GPU requirements: any Vulkan 1.3 capable device with compute queue support. Tested on AMD R7 9700X + RTX 5070 Ti at 60 FPS with a 2560×1440 grid.

## Building

### 1. Clone and init submodules

```bash
git clone https://github.com/<your-org>/rtfs2d.git
cd rtfs2d
git submodule update --init --recursive
```

### 2. Install dependencies (MSYS2 MinGW-w64)

```bash
pacman -S mingw-w64-x86_64-cmake \
          mingw-w64-x86_64-gcc \
          mingw-w64-x86_64-ninja \
          mingw-w64-x86_64-vulkan-headers \
          mingw-w64-x86_64-vulkan-loader \
          mingw-w64-x86_64-vulkan-utility-libraries \
          mingw-w64-x86_64-glfw \
          mingw-w64-x86_64-spdlog \
          mingw-w64-x86_64-nlohmann-json \
          mingw-w64-x86_64-shaderc
```

### 3. Configure and build

```bash
cmake -S . -B build -G Ninja \
    -DCMAKE_C_COMPILER=gcc \
    -DCMAKE_CXX_COMPILER=g++ \
    -DCMAKE_BUILD_TYPE=Release \
    -DLIB_PREFIX_PATH="C:/msys64/mingw64"
cmake --build build
```

Adjust `LIB_PREFIX_PATH` for your installation prefix.

### 4. Run

```bash
./build/rtfs2d examples/C1.json
```

For debug mode (Vulkan validation layers and trace logging):

```bash
./build/rtfs2d examples/C1.json --Debug
```

## Case File Format

Cases are JSON documents placed in `examples/`. Structure:

```jsonc
{
  "case": {
    // [nx, ny] — number of grid cells in x and y
    "mesh_resolution": [2560, 1440],
    // physical cell size (dx = dy = mesh_density)
    "mesh_density": 1.38888888e-3,
    "boundary_conditions": {
      "left": [
        { "from": 0.0, "to": 1.0, "type": "velocity", "u": 0.5 }
      ],
      "right": [
        { "from": 0.0, "to": 1.0, "type": "pressure" }
      ]
      // "bottom" and "top" default to no-slip wall if omitted
    },
    // obstacle expressions (optional)
    "geometry": [
      "C(0.15, 0.95, 0.05)"           // circle: C(center_x, center_y, radius)
      // "P(x1, y1) P(x2, y2) ..."    // polygon: P vertices in physical coordinates
    ]
  }
}
```

### Boundary types

| Type string | Description |
|---|---|
| `"no_slip_wall"` | `u = v = 0` at wall |
| `"slip_wall"` | zero normal velocity, free tangential |
| `"velocity"` | prescribed velocity (`u`, `v` fields) |
| `"pressure"` | Dirichlet `p = 0`, backflow suppressed |

All coordinates (`from`, `to`) are normalized [0, 1] along the edge. Velocities are in physical m/s.

## Controls

### Mouse

| Action | Behavior |
|---|---|
| Left click + drag | Inject dye at cursor (Dye mode only) |
| Click ImGui panel | Adjust visualization settings |

### Keyboard

| Key | Behavior |
|---|---|
| Space | Pause / Resume simulation |

### ImGui Panel

| Section | Controls |
|---|---|
| Top bar | Pause/Resume, Reset (restore defaults), Init Field (reinitialize velocity field), Fullscreen/Windowed |
| Time Step | Slider (0.001 — 0.05) |
| Mode | Dropdown: Field / Dye |

**Field mode:**

| Control | Options |
|---|---|
| Gradient | Grayscale / Jet / Cool/Warm |
| Field | Speed / Pressure / Vorticity |
| Coefficient | Per-field scaling factor |

**Dye mode:**

| Control | Range |
|---|---|
| Ink Radius | 0.005 — 0.1 |
| Ink Low / Mid / High | Three-layer color blending (0xRRGGBB) |

## Project Structure

```
.
├── CMakeLists.txt              # top-level build
├── main.cpp                    # entry point, CLI parsing
├── examples/                   # JSON case files
├── shaders/                    # GLSL compute + fragment shaders
├── src/
│   ├── render/                 # window, graphics context, ImGui
│   ├── solver/                 # fluid solvers, boundary, obstacles, case data
│   ├── vulkan/                 # device, swapchain, buffers, descriptors, pipelines
│   ├── compile/                # compiler generator framework (reused)
│   └── util/                   # input/byte reader abstractions
├── thirdparty/
│   ├── imgui/                  # Dear ImGui (git submodule)
│   └── googletest/             # Google Test (git submodule)
└── test/                       # unit tests
```

## License

MIT
