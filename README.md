# Sprite Sheet Studio

> A standalone C++17 desktop application and engine module for automatically detecting, aligning, previewing, and exporting sprite sheets. Designed as a reusable asset pipeline that will later integrate directly into the Wisdom Park ecosystem.

---

## Overview

Sprite Sheet Studio is a professional sprite sheet processing tool built from the ground up in modern C++.

The project is being developed as an independent application first, allowing the processing engine to mature before being integrated as a native module inside **Wisdom Park**.

The architecture separates the **Core Engine** from the **UI**, making the processing library completely reusable in future applications.

---

# Current Status

**Development Stage:** Milestone 5 Complete

### Completed Features

- Modern CMake project structure
- Modular Core/UI architecture
- Headless processing engine
- Project management system
- PNG image importing
- Infinite zoom & pan viewport
- Coordinate inspector
- Sprite auto detection
- Connected Component Labeling (CCL)
- Bounding box visualization
- Sprite selection
- Multi-selection
- Toggle selection
- Pivot editing
- Baseline editing
- Manual numeric editing
- Undo / Redo
- Pivot visualization
- Baseline visualization
- Interactive editor viewport

---

# Architecture

```
SpriteSheetStudio
│
├── Core
│   ├── Data Models
│   ├── Processing
│   ├── Commands
│   ├── Systems
│   ├── Export
│   ├── AI Interfaces
│   └── Studio Engine Facade
│
└── UI
    ├── Panels
    ├── Rendering
    └── Main Application
```

The project follows a strict separation between:

- **Core** → processing engine
- **UI** → rendering and interaction

This allows the Core library to be embedded into Wisdom Park without modification.

---

# Current Functionality

## Image Import

Supports importing PNG sprite sheets.

Displays:

- image dimensions
- pixel buffer
- zoomable viewport

---

## Viewport

Interactive editor featuring:

- Mouse wheel zoom
- Middle mouse panning
- Coordinate inspector
- Image centering
- Infinite navigation

---

## Automatic Sprite Detection

Uses Connected Component Labeling (CCL) to detect every isolated sprite.

Automatically computes:

- bounding rectangles
- sprite identifiers

Each detected sprite receives its own editable metadata.

---

## Sprite Editing

Every sprite supports:

- selection
- multi-selection
- pivot editing
- baseline editing
- manual numeric editing

The original image is **never modified**.

Only metadata changes.

---

## Visualization

Viewport overlays include:

- Bounding boxes
- Pivot points
- Baseline lines
- Sprite labels
- Selection highlighting

Everything updates in real time.

---

## Undo / Redo

Command Pattern implementation.

Supported operations:

- Move Pivot
- Move Baseline
- Numeric edits

Undo:

```
Ctrl + Z
```

Redo:

```
Ctrl + Shift + Z
```

---

# Controls

| Action | Shortcut |
|----------|----------|
| Import PNG | O |
| Auto Detect Sprites | Ctrl + D |
| Select Sprite | Left Click |
| Add Selection | Shift + Click |
| Toggle Selection | Ctrl + Click |
| Edit Pivot | Alt + Drag |
| Edit Baseline | Ctrl + Alt + Drag |
| Reset Pivot & Baseline | Double Click |
| Numeric Edit | N |
| Undo | Ctrl + Z |
| Redo | Ctrl + Shift + Z |
| Toggle Pivot Display | P |
| Toggle Baseline Display | L |
| Zoom | Mouse Wheel |
| Pan | Middle Mouse Drag |

---

# Design Philosophy

Sprite Sheet Studio is built around a **non-destructive editing pipeline**.

Instead of modifying pixels directly, the application stores metadata describing each sprite:

- Bounding Box
- Pivot
- Baseline
- Animation Membership

This allows repeated edits without degrading image quality.

---

# Technology

- C++17
- SFML
- CMake
- Modern STL
- Command Pattern
- Observer Pattern
- Facade Pattern

---

# Project Goals

Sprite Sheet Studio aims to eliminate the repetitive manual work involved in preparing sprite sheets for games.

The long-term vision includes:

- Automatic sprite detection
- Automatic baseline alignment
- Animation timeline
- Uniform grid generation
- Atlas packing
- Metadata export
- AI-assisted cleanup
- AI frame interpolation
- Direct Wisdom Park integration

---

# Roadmap

## Milestone 1
- ✅ Core architecture
- ✅ Data models
- ✅ Project management

## Milestone 2
- ✅ Image importing
- ✅ Interactive viewport
- ✅ Zoom & pan

## Milestone 3
- ✅ Viewport improvements
- ✅ Coordinate tools
- ✅ Navigation

## Milestone 4
- ✅ Connected Component Labeling
- ✅ Automatic sprite detection
- ✅ Bounding box rendering

## Milestone 5
- ✅ Sprite selection
- ✅ Multi-selection
- ✅ Pivot editing
- ✅ Baseline editing
- ✅ Undo / Redo
- ✅ Interactive editing tools

## Next Milestones

- Animation timeline
- Playback system
- Baseline auto-alignment
- Uniform grid packing
- Texture atlas generation
- JSON/XML export
- AI processing plugins
- Wisdom Park integration

---

# Future Integration with Wisdom Park

The Core engine is intentionally designed as a standalone library.

Once production-ready, the entire processing engine can be imported directly into Wisdom Park, allowing sprite editing to become a native part of the asset pipeline without rewriting the processing logic.

---

# License

This project is currently under active development.