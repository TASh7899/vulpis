#!/usr/bin/env python3

import os
import sys
import subprocess
import platform
import shutil
import argparse
import zipfile 
import json

MARKER_FILE = ".vulpis"
EXECUTABLE_NAME = "vulpis"

def find_project_root():
    current_dir = os.getcwd()
    while True:
        if os.path.exists(os.path.join(current_dir, MARKER_FILE)):
            return current_dir
        parent_dir = os.path.dirname(current_dir)
        if parent_dir == current_dir:
            return None
        current_dir = parent_dir

def get_tool_path(build_dir, tool_name, in_staging=False):
    """Safely locates compiled engine tools, handling OS extensions and CMake config subfolders."""
    exe_name = tool_name + (".exe" if platform.system() == "Windows" else "")
    base_dir = os.path.join(build_dir, "staging") if in_staging else build_dir

    possible_paths = [
            os.path.join(base_dir, exe_name),
            os.path.join(base_dir, "Release", exe_name),
            os.path.join(base_dir, "Debug", exe_name)
            ]
    for p in possible_paths:
        if os.path.exists(p):
            return p
    return None

def find_executable(build_dir):
    return get_tool_path(build_dir, EXECUTABLE_NAME)

def init():
    if os.path.exists(MARKER_FILE):
        print(f"Directory is already a Vulpis project ({MARKER_FILE} exists)")
        return
    with open(MARKER_FILE, "w") as f:
        f.write("Vulpis Project Config\n")
    print(f"Initialized Vulpis project in: {os.getcwd()}")
    print(f"{MARKER_FILE} created")

def clean(root_dir):
    build_dir = os.path.join(root_dir, "build")
    print(f"--- Cleaning build Directory: {build_dir} ---")
    if os.path.exists(build_dir):
        try:
            shutil.rmtree(build_dir)
            print("--- Clean successful ---")
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
            shutil.copy(source, target)
    except Exception as e:
        print(f"Warning: Could not update compile_commands.json: {e}")

def compile_lua(root_dir, build_dir):
    src_dir = os.path.join(root_dir, "src")
    staging_dir = os.path.join(build_dir, "staging", "src")

    luac_path = shutil.which("luac")
    if not luac_path:
        print("Critical Error: 'luac' compiler not found on your system path.")
        print("Please ensure Lua is installed on your OS.")
        sys.exit(1)

    print("--- Compiling Lua to bytecode ---")
    for root, dirs, files in os.walk(src_dir):
        for file in files:
            if not file.endswith(".lua"):
                continue
            lua_file = os.path.join(root, file)
            rel_path = os.path.relpath(root, src_dir)
            out_dir = staging_dir if rel_path == "." else os.path.join(staging_dir, rel_path)
            os.makedirs(out_dir, exist_ok=True)

            luac_file = os.path.join(out_dir, file.replace(".lua", ".luac"))
            try:
                subprocess.run([luac_path, "-s", "-o", luac_file, lua_file], check=True)
                print(f"  [Compiled] {file} -> {os.path.basename(luac_file)}")
            except subprocess.CalledProcessError as e:
                print(f"  [!] Failed to compile {file}: {e}")
                sys.exit(1)

def pack(root_dir, build_dir):
    baker_path = get_tool_path(build_dir, "asset_baker", in_staging=False)

    if not baker_path:
        print(f"Critical Error: 'asset_baker' not found in {build_dir}. Please build the project first.")
        sys.exit(1)

    assets_dir = os.path.join(root_dir, "assets")
    staging_src = os.path.join(build_dir, "staging", "src")
    output_vpak = os.path.join(build_dir, "app.vpak")

    print(f"--- Packing VFS Archive: {output_vpak} ---")
    try:
        subprocess.run([baker_path, assets_dir, staging_src, output_vpak], check=True)
        print("--- Packing Complete ---")
    except subprocess.CalledProcessError as e:
        print(f"--- Packing Failed: {e} ---")
        sys.exit(1)

def pack_scripts(root_dir, build_dir):
    compile_lua(root_dir, build_dir)

    output_vpak = os.path.join(build_dir, "app.vpak")
    if not os.path.exists(output_vpak):
        print("Error: app.vpak doesn't exist yet! You must run 'vulcli pack' at least once to bake assets.")
        sys.exit(1)

    temp_vpak = os.path.join(build_dir, "app_temp.vpak")
    staging_src = os.path.join(build_dir, "staging", "src")

    print(f"--- Injecting Lua scripts into {os.path.basename(output_vpak)} ---")
    try:
        with zipfile.ZipFile(output_vpak, 'r') as zin, zipfile.ZipFile(temp_vpak, 'w', zipfile.ZIP_DEFLATED) as zout:
            for item in zin.infolist():
                if not item.filename.startswith("src/"): 
                    zout.writestr(item, zin.read(item.filename))

            for root, dirs, files in os.walk(staging_src):
                for file in files:
                    if file.endswith(".luac"):
                        full_path = os.path.join(root, file)
                        rel_path = os.path.relpath(full_path, staging_src)
                        internal_path = "src/" + rel_path.replace("\\", "/") 
                        zout.write(full_path, internal_path)

        os.replace(temp_vpak, output_vpak)
        print("--- Fast Script Injection Complete ---")
    except Exception as e:
        print(f"--- Script Injection Failed: {e} ---")
        if os.path.exists(temp_vpak):
            os.remove(temp_vpak)
        sys.exit(1)

def build(root_dir, is_release=False, cmake_flags=None):
    if cmake_flags is None:
        cmake_flags = []

    system_os = platform.system()
    machine_arch = platform.machine().lower()

    os_prefix = "linux"
    if system_os == "Windows":
        os_prefix = "windows"
        dynamic_triplet = "x64-windows-dynamic"
    elif system_os == "Darwin":
        if machine_arch in ("arm64", "aarch64"):
            os_prefix = "mac-arm64"
            dynamic_triplet = "arm64-osx-dynamic"
        else:
            os_prefix = "mac"
            dynamic_triplet = "x64-osx-dynamic"
    else:
        dynamic_triplet = "x64-linux-dynamic"

    preset_name = f"{os_prefix}-{'release' if is_release else 'default'}"
    build_folder = os.path.join("build", "release" if is_release else "debug")
    build_dir = os.path.join(root_dir, build_folder)

    cmake_flags.append(f"-DVCPKG_TARGET_TRIPLET={dynamic_triplet}")
    print(f"--- Building Vulpis Project ({'Release' if is_release else 'Debug'}) ---")

    check_vcpkg(root_dir)

    if system_os == "Windows" and shutil.which("cl") is None:
        print("--- MSVC Compiler not found. Loading environment... ---")
        vcvars_paths = [
                r"C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvarsall.bat",
                r"C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat"
                ]
        found_vcvars = next((p for p in vcvars_paths if os.path.exists(p)), None)

        if found_vcvars:
            flags_str = " ".join(cmake_flags)
            cmd = f'"{found_vcvars}" x64 && cmake --preset {preset_name} {flags_str} && cmake --build {build_folder}'
            subprocess.run(cmd, shell=True, cwd=root_dir, check=True)
            return 
        sys.exit(1)

    if not os.path.exists(build_dir):
        os.makedirs(build_dir)

    try:
        if shutil.which("ninja") and "-G" not in " ".join(cmake_flags):
            cmake_flags.extend(["-G", "Ninja"])

        config_cmd = ["cmake", "--preset", preset_name] + cmake_flags
        subprocess.run(config_cmd, cwd=root_dir, check=True)
        subprocess.run(["cmake", "--build", build_folder], cwd=root_dir, check=True)

        compile_db = os.path.join(root_dir, build_folder, "compile_commands.json")
        if os.path.exists(compile_db):
            update_compile_db(root_dir, is_release)

        if is_release:
            exe_path = find_executable(build_dir)
            if exe_path and system_os != "Windows":
                try:
                    subprocess.run(["strip", exe_path], check=True)
                except Exception: pass
            print(f"--- Release Build Complete: {exe_path} ---")
    except subprocess.CalledProcessError:
        print("--- Build Failed ---")
        sys.exit(1)

def run(root_dir, is_release=False):
    build_folder = os.path.join("build", "release" if is_release else "debug")
    build_dir = os.path.join(root_dir, build_folder)

    executable = find_executable(build_dir)
    if not executable:
        print("Critical Error: Executable was not found. Please run 'vulcli build' first.")
        sys.exit(1)

    env = os.environ.copy()

    if is_release:
        print("--- Running Vulpis (Release Mode / VPAK) ---")
        run_cwd = build_dir
    else:
        print("--- Running Vulpis (Dev Mode / Raw Files) ---")
        run_cwd = root_dir
        env["VULPIS_DEV_MODE"] = "1"

    try:
        subprocess.run([executable], cwd=run_cwd, env=env)
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
        print("--- Bootstrapping vcpkg (First run only) ---")
        if is_windows:
            subprocess.run([bootstrap_script], cwd=vcpkg_dir, shell=True, check=True)
        else:
            os.chmod(bootstrap_path, 0o755)
            subprocess.run([bootstrap_path], cwd=vcpkg_dir, shell=False, check=True)

def analyze_lua_dependencies(root_dir):
    modules = {
            "NETWORK": "OFF",
            "DATABASE": "OFF",
            "AUDIO": "OFF"
            }
    app_lua_path = os.path.join(root_dir, "src", "app.lua")
    if os.path.exists(app_lua_path):
        try:
            with open(app_lua_path, "r", encoding="utf-8") as f:
                for line in f:
                    line = line.strip()
                    if line and not line.startswith("--"): break
                    if line.startswith("-- @vulpis-modules:"):
                        flags_str = line.split(":", 1)[1]
                        flags = [f.strip().upper() for f in flags_str.split(",")]
                        if "NETWORK" in flags: modules["NETWORK"] = "ON"
                        if "DATABASE" in flags: modules["DATABASE"] = "ON"
                        if "AUDIO" in flags: modules["AUDIO"] = "ON"
                        break 
        except Exception: pass

    cmake_args = [
            f"-DVULPIS_MODULE_NETWORK={modules['NETWORK']}",
            f"-DVULPIS_MODULE_DATABASE={modules['DATABASE']}",
            f"-DVULPIS_MODULE_AUDIO={modules['AUDIO']}"
            ]

    vcpkg_features = []
    if modules["NETWORK"] == "ON": vcpkg_features.append("network")
    if modules["DATABASE"] == "ON": vcpkg_features.append("database")

    if vcpkg_features:
        cmake_args.append(f"-DVCPKG_MANIFEST_FEATURES={';'.join(vcpkg_features)}")
    else:
        cmake_args.append("-DVCPKG_MANIFEST_FEATURES=")

    return cmake_args


def get_or_create_manifest(root_dir):
    """Ensures a manifest exists, creating a default one if missing."""
    manifest_path = os.path.join(root_dir, "manifest.json")
    if not os.path.exists(manifest_path):
        default_manifest = {
            "app_name": "MyVulpisApp",
            "runtime_api": 5
        }
        with open(manifest_path, 'w') as f:
            json.dump(default_manifest, f, indent=4)
        print(f"--- Info: No manifest.json found. Created default template at {manifest_path} ---")
        return default_manifest
    
    with open(manifest_path, 'r') as f:
        return json.load(f)




def generate_app_installer(root_dir):
    """
    Generates robust application installers that query the system's Vulpis runtime
    for API compatibility before proceeding with the installation.
    """
    system_os = platform.system()
    machine_arch = platform.machine().lower()
    dist_dir = os.path.join(root_dir, "dist")
    build_dir = os.path.join(root_dir, "build", "release")
    vpak_path = os.path.join(build_dir, "app.vpak")

    manifest = get_or_create_manifest(root_dir)

    app_name = manifest.get("app_name", "VulpisApp")
    required_api = manifest.get("runtime_api", 1)

    pkg_name = f"{app_name.lower()}-{system_os.lower()}-{machine_arch}"
    target_dist_dir = os.path.join(dist_dir, pkg_name)

    if os.path.exists(target_dist_dir):
        shutil.rmtree(target_dist_dir)
    os.makedirs(target_dist_dir, exist_ok=True)

    if os.path.exists(vpak_path):
        shutil.copy2(vpak_path, os.path.join(target_dist_dir, "app.vpak"))
    else:
        print(f"Critical Error: {vpak_path} not found. Run 'vulcli pack --release' first.")
        sys.exit(1)

    print(f"--- Generating API-Aware Application Installer for {system_os} ---")

    if system_os == "Linux":
        install_sh = f"""#!/bin/bash
set -e

APP_NAME="{app_name}"
REQUIRED_API={required_api}
INSTALL_DIR="/opt/$APP_NAME"

echo "Locating Vulpis Runtime..."
RUNTIME_BIN=$(command -v vulpis-runtime || true)

if [ -z "$RUNTIME_BIN" ]; then
    echo "Error: 'vulpis-runtime' not found in PATH."
    echo "Please install the Vulpis framework first."
    exit 1
fi

echo "Verifying API compatibility..."
# Safely extract the API integer from the runtime's JSON output
SYSTEM_API=$("$RUNTIME_BIN" --runtime-info | python3 -c "import sys, json; print(json.load(sys.stdin).get('api', 0))")

if [ "$SYSTEM_API" -lt "$REQUIRED_API" ]; then
    echo "Error: $APP_NAME requires Vulpis API level $REQUIRED_API."
    echo "Your system is running API level $SYSTEM_API. Please update your runtime."
    exit 1
fi

echo "Installing $APP_NAME..."
sudo mkdir -p "$INSTALL_DIR"
sudo cp app.vpak "$INSTALL_DIR/"

echo "Registering OS UI..."
cat <<EOF | sudo tee /usr/share/applications/$APP_NAME.desktop > /dev/null
[Desktop Entry]
Name=$APP_NAME
Exec=$RUNTIME_BIN --mount "$INSTALL_DIR/app.vpak"
Icon=application-default
Type=Application
Categories=Utility;Application;
Terminal=false
EOF

sudo update-desktop-database
echo "Installation complete! $APP_NAME is now in your application menu."
"""
        sh_path = os.path.join(target_dist_dir, "install.sh")
        with open(sh_path, "w") as f:
            f.write(install_sh)
        os.chmod(sh_path, 0o755)

    elif system_os == "Windows":
        ps1_content = f"""$ErrorActionPreference = "Stop"

$AppName = "{app_name}"
$RequiredApi = {required_api}
$AppInstallDir = Join-Path $env:LOCALAPPDATA $AppName
$StartMenu = "$env:APPDATA\\Microsoft\\Windows\\Start Menu\\Programs\\$AppName.lnk"

Write-Host "Locating Vulpis Runtime..."
$RuntimeCommand = Get-Command "vulpis-runtime.exe" -ErrorAction SilentlyContinue
if (-not $RuntimeCommand) {{
    Write-Host "ERROR: 'vulpis-runtime' not found in system PATH." -ForegroundColor Red
    Write-Host "Please install the Vulpis framework first."
    exit 1
}}
$RuntimeBin = $RuntimeCommand.Source

Write-Host "Verifying API compatibility..."
$InfoJson = & $RuntimeBin --runtime-info | ConvertFrom-Json
if ($InfoJson.api -lt $RequiredApi) {{
    Write-Host "ERROR: $AppName requires Vulpis API level $RequiredApi." -ForegroundColor Red
    Write-Host "Your system is running API level $($InfoJson.api). Please update your runtime."
    exit 1
}}

Write-Host "Installing $AppName..."
if (-not (Test-Path $AppInstallDir)) {{ New-Item -ItemType Directory -Path $AppInstallDir -Force | Out-Null }}
Copy-Item -Path ".\\app.vpak" -Destination $AppInstallDir -Force

Write-Host "Registering OS UI Shortcut..."
$WshShell = New-Object -comObject WScript.Shell
$Shortcut = $WshShell.CreateShortcut($StartMenu)
$Shortcut.TargetPath = $RuntimeBin
$Shortcut.Arguments = "--mount `"$AppInstallDir\\app.vpak`""
$Shortcut.WorkingDirectory = $AppInstallDir
$Shortcut.Save()

Write-Host "Installation Complete! $AppName is available in your Start Menu." -ForegroundColor Green
"""
        ps1_path = os.path.join(target_dist_dir, "install.ps1")
        with open(ps1_path, "w") as f:
            f.write(ps1_content)

    elif system_os == "Darwin":
        app_dir = os.path.join(target_dist_dir, f"{app_name}.app")
        macos_dir = os.path.join(app_dir, "Contents", "MacOS")
        resources_dir = os.path.join(app_dir, "Contents", "Resources")

        os.makedirs(macos_dir, exist_ok=True)
        os.makedirs(resources_dir, exist_ok=True)
        shutil.copy2(vpak_path, os.path.join(resources_dir, "app.vpak"))

        wrapper_sh = f"""#!/bin/bash
REQUIRED_API={required_api}
VPAK_PATH="$(dirname "$0")/../Resources/app.vpak"

# macOS GUI apps don't inherently inherit the user's shell PATH, so we define the standard locations
export PATH="/usr/local/bin:/opt/vulpis/bin:$PATH"
RUNTIME_BIN=$(command -v vulpis-runtime || true)

if [ -z "$RUNTIME_BIN" ]; then
    osascript -e 'display dialog "Vulpis Runtime is missing. Please install the framework first." buttons {{"OK"}} default button "OK" with icon stop'
    exit 1
fi

SYSTEM_API=$("$RUNTIME_BIN" --runtime-info | python3 -c "import sys, json; print(json.load(sys.stdin).get('api', 0))")

if [ "$SYSTEM_API" -lt "$REQUIRED_API" ]; then
    osascript -e 'display dialog "API mismatch! App requires API '$REQUIRED_API', system has '$SYSTEM_API'." buttons {{"OK"}} default button "OK" with icon stop'
    exit 1
fi

exec "$RUNTIME_BIN" --mount "$VPAK_PATH"
"""
        wrapper_path = os.path.join(macos_dir, app_name)
        with open(wrapper_path, "w") as f:
            f.write(wrapper_sh)
        os.chmod(wrapper_path, 0o755)

        plist_content = f"""<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
    <key>CFBundleExecutable</key>
    <string>{app_name}</string>
    <key>CFBundleIdentifier</key>
    <string>com.vulpis.{app_name.lower()}</string>
    <key>CFBundleName</key>
    <string>{app_name}</string>
    <key>CFBundleIconFile</key>
    <string>AppIcon</string>
    <key>CFBundleShortVersionString</key>
    <string>1.0</string>
    <key>LSMinimumSystemVersion</key>
    <string>10.13</string>
</dict>
</plist>
"""
        with open(os.path.join(app_dir, "Contents", "Info.plist"), "w") as f:
            f.write(plist_content)
            
    else:
        print(f"Installer generation not supported for OS: {system_os}")
        
    print("--- Installer Pipeline Complete ---")





if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Vulcli - Command-line build tool for the Vulpis Engine")
    subparsers = parser.add_subparsers(dest="cmd", required=True)

    parser_init = subparsers.add_parser("init")

    parser_build = subparsers.add_parser("build")
    parser_build.add_argument("--release", action="store_true")

    parser_pack = subparsers.add_parser("pack")
    parser_pack.add_argument("--release", action="store_true")

    parser_pack_scripts = subparsers.add_parser("pack-scripts")
    parser_pack_scripts.add_argument("--release", action="store_true")

    parser_run = subparsers.add_parser("run")
    parser_run.add_argument("--release", action="store_true")

    parser_clean = subparsers.add_parser("clean")
    parser_vcpkg = subparsers.add_parser("vcpkg")

    parser_installer = subparsers.add_parser("installer")

    args = parser.parse_args()

    if args.cmd == "init":
        init()
        sys.exit(0)

    project_root = find_project_root()
    if not project_root:
        print(" Error: Not inside a Vulpis project.")
        sys.exit(1)

    is_release = getattr(args, 'release', False)
    build_folder = os.path.join("build", "release" if is_release else "debug")
    build_dir = os.path.join(project_root, build_folder)

    release_build_dir = os.path.join(project_root, "build", "release")

    if args.cmd == "build":
        cmake_flags = analyze_lua_dependencies(project_root)
        build(project_root, is_release, cmake_flags)
        
    elif args.cmd == "pack":
        print("--- Forcing Release Mode for Packing ---")
        compile_lua(project_root, release_build_dir)
        pack(project_root, release_build_dir)
        
    elif args.cmd == "pack-scripts":
        print("--- Forcing Release Mode for Script Packing ---")
        pack_scripts(project_root, release_build_dir)
        
    elif args.cmd == "run":
        run(project_root, is_release)
        
    elif args.cmd == "clean":
        clean(project_root)
        
    elif args.cmd == "vcpkg":
        check_vcpkg(project_root)

    elif args.cmd == "installer":
        generate_app_installer(project_root)


