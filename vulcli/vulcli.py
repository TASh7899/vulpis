#!/usr/bin/env python3
import os
from os.path import expanduser
from re import S
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
        print(f"Directory is already a Vulpis project ({MARKER_FILE} exists")
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
            print("--- clean successfull ---")
        except Exception as e:
            print(f"--- Error Cleaning Build Directory: {e} ---")

    else:
        print("--- Directory already clean ---")




def build(root_dir):
    build_dir = os.path.join(root_dir, "build")
    print(f"--- Building Vulpis Project in {os.getcwd()}")

    check_vcpkg(root_dir)

    system_os = platform.system()
    
    # 1. Select the correct preset based on the OS
    if system_os == "Windows":
        preset_name = "windows-default"
    elif system_os == "Darwin":
        preset_name = "mac-default"
    else:
        preset_name = "linux-default"
    
    print(f"--- Detected {system_os}, using preset: {preset_name} ---")

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
            # Note: We use the detected preset_name here as well
            cmd = f'"{found_vcvars}" x64 && cmake --preset {preset_name} && cmake --build build'
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
        subprocess.run(["cmake", "--build", "build"], cwd=root_dir, check=True)
    except subprocess.CalledProcessError:
        print("--- Build Failed ---")
        sys.exit(1)

def run(root_dir):
    build_dir = os.path.join(root_dir, "build")
    executable = find_executable(build_dir)

    if not executable:
        print("Executable not found. Building first...")
        build(root_dir)
        executable = find_executable(build_dir)

    if not executable:
        print("Critical Error: Build succeeded but executable was not found.")
        sys.exit(1)

    print("--- Running vulpis ---")
    try:
        subprocess.run([executable], cwd=build_dir)
    except KeyboardInterrupt:
        print("\nExiting...")


def check_vcpkg(root_dir):
    vcpkg_dir = os.path.join(root_dir, "third_party", "vcpkg")

    is_windows = platform.system() == "Windows"
    script_name = "bootstrap-vcpkg.bat" if is_windows else "bootstrap-vcpkg.sh"
    script_path = os.path.join(vcpkg_dir, script_name)

    exe_name = "vcpkg.exe" if is_windows else "vcpkg"
    vcpkg_exe = os.path.join(vcpkg_dir, exe_name)


    if not os.path.exists(script_path):
        print("--- Initializing vcpkg submodule ---")
        subprocess.run(["git", "submodule", "update", "--init", "--recursive"], cwd=root_dir, check=True)


    if not os.path.exists(vcpkg_exe):
        print("--- 🛠️  Bootstrapping vcpkg (First run only) ---")

        if is_windows:
            subprocess.run([script_name], cwd=vcpkg_dir, shell=True, check=True)
        else:
            os.chmod(script_path, 0o755)
            subprocess.run([script_path], cwd=vcpkg_dir, shell=False, check=True)
    print("--- vcpkg is ready ---")

if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("Usage: vulcli [init | build | run | vcpkg]")
        sys.exit(1)

    cmd = sys.argv[1]
    if cmd == "init":
        init()
        sys.exit(1)
    
    project_root = find_project_root()
    if not project_root:
        print(" Error: Not inside a Vulpis project.")
        print(f" Could not find '{MARKER_FILE}' in this or any parent directory.")
        print(" Run 'vulcli init' to start a new project here.")
        sys.exit(1)

    if cmd == "build":
        build(project_root)
    elif cmd == "run":
        run(project_root)
    elif cmd == "clean":
        clean(project_root)
    elif cmd == "vcpkg":
        check_vcpkg(project_root)
    else:
        print(f"Unknow command: {cmd}")



