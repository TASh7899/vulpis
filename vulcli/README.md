# 🦊 Vulcli (Vulpis CLI)

**Vulcli** is the official command-line build tool and launcher for the Vulpis Engine. It automates the CMake build process, manages project directories, and ensures cross-platform compatibility for assets.

## Installation

You can install `vulcli` globally on your system so you can run it from any directory.

### Linux / macOS
Run the installer with sudo to create a global symlink in `/usr/local/bin`:
```bash
cd vulcli
sudo python3 install.py

```

### Windows

Run the installer (Administrator recommended) to add the tool to your System PATH:

```cmd
cd vulcli
python install.py

```

*(Note: You may need to restart your terminal after installation on Windows).*

---

## Usage

Once installed, you can use the `vulcli` command from anywhere inside your project.

### 1. Initialize a Project

Run this in the root of your project to mark it as a Vulpis project. This creates a `.vulpis` marker file.

```bash
vulcli init

```

### 2. Build the Engine

Compiles the project using CMake presets. It automatically detects your root directory, so you can run this from subfolders like `src/` or `engine/`.

```bash
vulcli build

```

### 3. Run the Game

Builds (if necessary) and launches the executable. It automatically handles working directories so your assets load correctly.

```bash
vulcli run

```

### 4. Clean Build Files

Safely deletes the `build/` directory to force a fresh compilation.

```bash
vulcli clean

```

---

## Requirements

* **Python 3.6+**
* **CMake 3.15+**
* **C++ Compiler** (GCC/Clang on Linux, MSVC on Windows)
* **Vulpis Engine Source** (The tool expects a standard Vulpis directory structure)

```

