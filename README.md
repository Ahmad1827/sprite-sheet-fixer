# Sprite Sheet Studio

A professional, cross-platform sprite sheet processing and animation preparation tool built with **C++17** and **SFML**.

Sprite Sheet Studio is designed as a modular engine component that automatically detects sprites, aligns animation frames, edits pivots and baselines non-destructively, previews animations in real time, and exports production-ready sprite atlases with structured metadata.

---

## Preview

![Sprite Sheet Studio Demo](Resources/gifSSF.gif)

---

## Features

### Image Import & Handling
- **PNG Sprite Sheet Support:** High-resolution texture processing.
- **Cross-Platform File Dialogs:** Native file pickers for both Linux and Windows.
- **Immutable Source Architecture:** Original assets remain untouched in memory and on disk.

### Automatic Sprite Detection
- **Connected Component Labeling (CCL):** Fast pixel-connectivity grouping.
- **Alpha-Based Extraction:** Isolate sprites based on transparency thresholds.
- **Bounding Box Generation:** Automatic bounding calculations with customizable padding and spacing.
- **Batch Detection:** Single-click workspace population for dense atlases.

### Interactive Sprite & Pivot Editing
- **Direct Manipulation:** Interactive sprite selection, bounding box resizing, and repositioning.
- **Baseline & Pivot Tuning:** Dedicated tools to align ground baselines and rotation/anchor pivots.
- **Multi-Selection Operations:** Move, group, and bulk-adjust frames simultaneously.

### Animation & Timeline Engine
- **Timeline Workspace:** Multi-frame timeline editor with drag-and-drop reordering.
- **Playback Controls:** Real-time preview with loop, pause, step, and dynamic FPS controls.
- **Automatic Animation Builder:** Auto-group sequenced frames based on spatial layout.

### Alignment Engine
- **Baseline Detection:** Automatic detection of ground contact points across sequences.
- **Frame Normalization:** Uniform positioning across disparate frame bounding dimensions.
- **Visual Diffing:** Real-time visual comparison overlay before finalizing exports.

### Export Pipeline
- **PNG Atlas:** Packed texture atlas generated with tight packing algorithms.
- **JSON Metadata:** Structured frame definitions, pivots, bounding boxes, and animation tags.
- **Configurable Layouts:** Adjustable padding, borders, and power-of-two constraints.

### Editor & Workflow
- **History Management:** Full multi-level Undo / Redo system.
- **Viewport Navigation:** Smooth pan, zoom, coordinate axes, and responsive grid overlays.
- **Dark Theme UI:** Engineered for workflow focus and tool accessibility.

---

## Architecture

The project is structured into two completely decoupled layers:

```
SpriteSheetStudio
│
├── Core
│   ├── Data Models
│   ├── Processing
│   ├── Systems
│   ├── Commands
│   ├── Export
│   ├── AI Interfaces
│   └── StudioEngineFacade
│
└── UI
    ├── Panels
    ├── Rendering
    ├── Workspace
    └── SpriteSheetStudioPanel
```

### Core Engine
The `Core` module contains all pure processing algorithms, state machines, math operations, and serialization routines. It does not depend on the rendering UI or application framework.

Key subsystems:
- Sprite detection & segmentation algorithms
- Baseline analysis & alignment transformations
- Command pattern stack (Undo/Redo execution)
- Export & packing processors

The entire engine communicates via a single public entry point:
```cpp
StudioEngineFacade
```

### UI Module
The `UI` module handles user interaction, rendering gizmos, panels, and viewports. It delegates all operations to `StudioEngineFacade`, ensuring the UI can be embedded directly into external host engines (such as **Wisdom Park**) without architectural modifications.

---

## Non-Destructive Workflow

Every transformation, crop, baseline adjustment, and pivot shift is stored strictly as metadata. The underlying source texture remains untouched throughout the workflow:

**Source Image + Metadata Operations -> Export Pipeline -> Optimized Atlas + JSON Manifest**

---

## Tech Stack

- **Language:** C++17
- **Graphics & Windowing:** SFML 2.6+
- **Build System:** CMake / MSBuild / Make
- **Packaging:** `ship` standalone packaging utility

---

## Building & Installation

### Linux / WSL

```bash
git clone [https://github.com/Ahmad1827/sprite-sheet-fixer.git](https://github.com/Ahmad1827/sprite-sheet-fixer.git)
cd sprite-sheet-fixer

mkdir build && cd build
cmake ..
make -j$(nproc)

./UI/SpriteSheetStudioApp
```

### Windows (Visual Studio)

1. Open the project folder or generated solution in Visual Studio 2022.
2. Select **Release** and **x64**.
3. Build the solution (`Ctrl + Shift + B`).

### Standalone Packaging (via Ship)

To bundle the application, all dynamic runtime dependencies, and assets into a single standalone binary:

```bash
ship -s
```

The resulting binary will be output to `dist/`.

---

## License

This project is provided for educational and portfolio purposes.