#!/usr/bin/env python3
import os
import sys
import subprocess
import platform
import shutil

MARKER_FILE = ".vulpis"
EXECUTABLE_NAME = "vulpis"

def find_project_root():
    # Climb up from the current working directory to find the folder containing .vulpis
    current_dir = os.getcwd()

    while True:
        if os.path.exists(os.path.join(current_dir, MARKER_FILE)):
            return current_dir
        parent_dir = os.path.dirname(current_dir)
        if parent_dir == current_dir:
            return None
        current_dir = parent_dir


def find_executable(build_dir):
    # finds the binary across different os/cmake layouts
    name = EXECUTABLE_NAME + (".exe" if platform.system() == "Windows" else "" )
    possible_paths = [
        os.path.join(build_dir, name),
        os.path.join(build_dir, "Debug", name),
        os.path.join(build_dir, "Release", name),
    ]
    for path in possible_paths:
        if os.path.exists(path):
            return path
    return None

def init():
    # creates .vulpis file marker in directory
    if os.path.exists(MARKER_FILE):
        print(f"Directory is already a Vulpis project ({MARKER_FILE} exists)")
        return
    with open(MARKER_FILE, "w") as f:
        f.write("Vulpis Project Config\n")

    print(f"Initialized Vulpis project in: {os.getcwd()}")
    print(f"{MARKER_FILE} created")


def clean(root_dir):
    build_dir = os.path.join(root_dir, "build")
    print(f" --- Cleaning build Directory: {build_dir} ---")

    if os.path.exists(build_dir):
        try:
            shutil.rmtree(build_dir)
            print("--- clean successful ---")
        except Exception as e:
            print(f"--- Error Cleaning Build Directory: {e} ---")
    else:
        print("--- Directory already clean ---")


def update_compile_db(root_dir, is_release):
    build_folder = os.path.join("build", "release" if is_release else "debug")

    source = os.path.join(root_dir, build_folder, "compile_commands.json")
    target = os.path.join(root_dir, "compile_commands.json")

    try:
        if os.path.exists(target) or os.path.islink(target):
            os.remove(target)

        try:
            os.symlink(source, target)
        except OSError:
            # fallback for Windows
            shutil.copy(source, target)

    except Exception as e:
        print(f"Warning: Could not update compile_commands.json: {e}")


def build(root_dir, is_release=False):
    system_os = platform.system()
    machine_arch = platform.machine().lower()
    
    # 1. Select the correct preset prefix based on the OS and Architecture
    os_prefix = "linux"
    if system_os == "Windows":
        os_prefix = "windows"
    elif system_os == "Darwin":
        # Check for Apple Silicon (ARM)
        if machine_arch == "arm64" or machine_arch == "aarch64":
            os_prefix = "mac-arm64"
        else:
            os_prefix = "mac"
        
    preset_name = f"{os_prefix}-{'release' if is_release else 'default'}"
    build_folder = os.path.join("build", "release" if is_release else "debug")
    build_dir = os.path.join(root_dir, build_folder)

    print(f"--- Building Vulpis Project ({'Release' if is_release else 'Debug'}) in {os.getcwd()} ---")

    check_vcpkg(root_dir)

    print(f"--- Detected {system_os} ({machine_arch}), using preset: {preset_name} ---")

    # 2. AUTOMATION: Load MSVC Environment if on Windows and compiler is missing
    if system_os == "Windows" and shutil.which("cl") is None:
        print("--- MSVC Compiler not found in PATH. Attempting to load environment... ---")
        vcvars_paths = [
            r"C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvarsall.bat",
            r"C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat",
            r"C:\Program Files\Microsoft Visual Studio\2026\Community\VC\Auxiliary\Build\vcvarsall.bat"
        ]
        
        found_vcvars = next((p for p in vcvars_paths if os.path.exists(p)), None)
        
        if found_vcvars:
            cmd = f'"{found_vcvars}" x64 && cmake --preset {preset_name} && cmake --build {build_folder}'
            subprocess.run(cmd, shell=True, cwd=root_dir, check=True)
            return 
        else:
            print("Error: Visual Studio Build Tools not found. Please install 'Desktop development with C++'.")
            sys.exit(1)

    # 3. Standard Build Process (macOS, Linux, or Windows with compiler already in PATH)
    if not os.path.exists(build_dir):
        os.makedirs(build_dir)
        
    try:
        subprocess.run(["cmake", "--preset", preset_name], cwd=root_dir, check=True)
        # Point the build command explicitly to the subfolder (build/release or build/debug)

        compile_db = os.path.join(root_dir, build_folder, "compile_commands.json")
        if os.path.exists(compile_db):
            update_compile_db(root_dir, is_release)

        subprocess.run(["cmake", "--build", build_folder], cwd=root_dir, check=True)
    except subprocess.CalledProcessError:
        print("--- Build Failed ---")
        sys.exit(1)

def run(root_dir, is_release=False):
    # Route to the correct subfolder based on the flag
    build_folder = os.path.join("build", "release" if is_release else "debug")
    build_dir = os.path.join(root_dir, build_folder)

    executable = find_executable(build_dir)

    if not executable:
        print("Executable not found. Building first...")
        build(root_dir, is_release)
        executable = find_executable(build_dir)

    if not executable:
        print("Critical Error: Build succeeded but executable was not found.")
        sys.exit(1)

    print(f"--- Running vulpis ({'Release' if is_release else 'Debug'}) ---")
    try:
        subprocess.run([executable], cwd=build_dir)
    except KeyboardInterrupt:
        print("\nExiting...")


def check_vcpkg(root_dir):
    vcpkg_dir = os.path.join(root_dir, "third_party", "vcpkg")
    is_windows = platform.system() == "Windows"

    bootstrap_script = "bootstrap-vcpkg.bat" if is_windows else "bootstrap-vcpkg.sh"
    bootstrap_path = os.path.join(vcpkg_dir, bootstrap_script)

    if not os.path.exists(bootstrap_path):
        print("--- vcpkg files missing. Initializing vcpkg submodule ---")
        subprocess.run(["git", "submodule", "update", "--init", "--recursive"], cwd=root_dir, check=True)

    exe_name = "vcpkg.exe" if is_windows else "vcpkg"
    vcpkg_exe = os.path.join(vcpkg_dir, exe_name)

    if not os.path.exists(vcpkg_exe):
        print("--- 🛠️  Bootstrapping vcpkg (First run only) ---")
        if is_windows:
            subprocess.run([bootstrap_script], cwd=vcpkg_dir, shell=True, check=True)
        else:
            os.chmod(bootstrap_path, 0o755)
            subprocess.run([bootstrap_path], cwd=vcpkg_dir, shell=False, check=True)

    print("--- vcpkg is ready ---")

if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("Usage: vulcli [init | build | run | clean | vcpkg] [--release]")
        sys.exit(1)

    cmd = sys.argv[1]

    # Check for the build option flag anywhere in the arguments
    is_release = "--release" in sys.argv

    if cmd == "init":
        init()
        sys.exit(0)

    project_root = find_project_root()
    if not project_root:
        print(" Error: Not inside a Vulpis project.")
        print(f" Could not find '{MARKER_FILE}' in this or any parent directory.")
        print(" Run 'vulcli init' to start a new project here.")
        sys.exit(1)

    if cmd == "build":
        build(project_root, is_release)
    elif cmd == "run":
        run(project_root, is_release)
    elif cmd == "clean":
        clean(project_root)
    elif cmd == "vcpkg":
        check_vcpkg(project_root)
    else:
        print(f"Unknown command: {cmd}")
