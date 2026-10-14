# PatternAnalyzer

**PatternAnalyzer** is a high-performance, interactive C++ desktop application built with **OpenGL** and **Dear ImGui** designed for comparing, analyzing, and inspecting image patterns and tile-based differences across image pairs.

---

## Features

### 1. Dual Viewing Modes
* **Pattern View:** Overlays computed difference patterns, custom templates, or motion vectors directly onto Image 1 against Image 2.
* **Juxtaposed View:** Side-by-side comparison pane allowing direct visualization of both images simultaneously with grid overlays and quick double-click file loading.

### 2. Advanced Tile Difference & Error Analysis
* **Configurable Tile Sizes:** Dynamically adjustable tile sizes with user-defined minimum and maximum limits (persisted via `settings.ini`).
* **Multiple Metrics:** 
  * Mean Squared Error (**MSE**)
  * Mean Absolute Error (**MAE / L1**)
  * Max Difference
  * Sum of Squared Differences (**SSD** Translation Search)
  * Sum of Absolute Differences (**SAD** Translation Search)
* **Pattern Gallery:** Highlight discrepancy regions using custom shapes such as Cross (`x`), Plus (`+`), Circle (`o`), Diamond (`<>`), or **Motion Vectors (`->`)**.
* **Coloring Modes:** Default, Solid Color highlight, or **ParaView's Cool-to-Warm** error metric heatmaps.

### 3. File Management & History
* **File Drag-and-Drop:** Drag image files directly onto either side of the Juxtaposed view to load them instantly.
* **Recently Opened History:** Remembers up to 10 recently opened files (`recent_files.ini`) and provides an interactive modal to open them as new base images.
* **Test Presets:** Built-in synthetic generators for testing edge cases, checkerboards, gradients, clean vs. noisy pairs, and shifts.

### 4. Interactive Navigation
* **Pan & Zoom:** Smooth mouse panning and cursor-centered zoom (plus keyboard shortcuts like `+`, `-`, and box-selection zoom via `Ctrl + Drag`).
* **Hover Tooltips:** Inspect individual pixel values at high zoom levels or tile IDs and error metrics at standard zoom levels.

---

## File formats

* Various image formats
* Raw files (use the NetCDF conversion tool under the [tools](/tools) directory to create those)

## Controls & Shortcuts

| Action | Control / Shortcut |
| :--- | :--- |
| **Pan View** | Left-Click Drag or Right-Click Drag (Pattern View) |
| **Zoom In / Out** | Mouse Scroll Wheel / `+` / `-` keys |
| **Zoom to Region** | `Ctrl` + Left-Click Drag |
| **Open File** | Main Menu (`File` -> `Open Image 1 / 2`) |

---

## Compiling and Running

### Prerequisites
* A C++17 compatible compiler (Clang, GCC, or MSVC)
* **CMake** (version 3.10 or higher)
* **OpenGL 3.3+** and **GLFW** (downloaded on configure)
* **Dear ImGui** (downloaded on configure)
* **portable-file-dialogs** (pfd) for native file pickers (included)

### Build Instructions

 Clone or open the repository directory:
 ```
 cd PatternAnalyzer
```

Create a build directory and configure the project with CMake:
```
mkdir build
cd build
cmake ..
```

Compile the application:
```
cmake --build . --config Release
```

Compile the application:
```
./PatternAnalyzer [path/to/image1.png] [path/to/image2.png]
```

---

## Background & Academic Reference

The core visualization and comparison techniques implemented in this tool are based on the research paper:
> **Comparison of Categorical Data from Meteorological Models and Observations using a Pattern-Based Approach**  
> *S. Pandey, M. Sondag, C. Krauter, S. Zellmann, T. von Landesberger*

