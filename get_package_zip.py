
import json
import os
import subprocess

def first(h):
    return list(h.items())[0][1]

proc = subprocess.run(['conan', 'list', '-f', 'json', 'mtconnect_agent/*:*' ], capture_output=True)

js = json.loads(proc.stdout)
name = list(first(js))[0]
package = list(first(first(first(first(js))))['packages'].keys())[0]

build_dir_proc = subprocess.run(['conan', 'cache', 'path',
                            f"{name:}:{package:}"], capture_output=True)

build_dir = build_dir_proc.stdout.strip().decode('utf-8')

files = os.listdir(f"{build_dir:}")

release = [f for f in files if os.listdir(f"{build_dir:}") if f.startswith('agent') and f.endswith('.zip')]

print(f"{build_dir:}/{release[0]:}")

