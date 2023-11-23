#
# open_explorer.py
#
def open_file_explorer():
    import subprocess
    from pathlib import Path
    from SCons.Script import Import
    from SCons.Script import DefaultEnvironment
    env = DefaultEnvironment()
    Import("env")
    BUILD_PATH = Path(env['PROJECT_BUILD_DIR'], env['PIOENV'])
    script = f"{BUILD_PATH}"
    import platform
    current_OS = platform.system()
    if current_OS == 'Windows':
        try:
            subprocess.run(["start", " ", script], shell=True)
        except Exception as e:
            print(f"Could not open File Explorer, an error occurred: {e}")
    elif current_OS == 'Linux':
        try:
            subprocess.run(["xdg-open", script])
        except Exception as e:
            print(f"Could not open File Explorer, an error occurred: {e}")