#!/usr/bin/env python3
"""Read-only Mach-O dependency and runpath audit for a deployed QingView app."""
import argparse
import datetime
import hashlib
import json
from pathlib import Path
import re
import subprocess


def command(*args):
    return subprocess.check_output(args, text=True)


def rpaths(path):
    lines = command("otool", "-l", str(path)).splitlines()
    result = []
    for index, line in enumerate(lines):
        if line.strip() == "cmd LC_RPATH":
            for detail in lines[index + 1:index + 4]:
                match = re.match(r"\s*path (.*?) \(offset \d+\)", detail)
                if match:
                    result.append(match.group(1))
    return result


def dependencies(path):
    return [match.group(1) for line in command("otool", "-L", str(path)).splitlines()
            if (match := re.match(r"\s+(.+?) \(compatibility version", line))]


def install_ids(path):
    lines = command("otool", "-D", str(path)).splitlines()
    return {line.strip() for line in lines[1:] if line.strip() and not line.endswith(":" )}


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("bundle", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    app = args.bundle.resolve()
    executable_dir = app / "Contents/MacOS"
    executable = executable_dir / "QingView"
    executable_rpaths = rpaths(executable)
    magic = {b"\xfe\xed\xfa\xce", b"\xce\xfa\xed\xfe", b"\xfe\xed\xfa\xcf", b"\xcf\xfa\xed\xfe",
             b"\xca\xfe\xba\xbe", b"\xbe\xba\xfe\xca", b"\xca\xfe\xba\xbf", b"\xbf\xba\xfe\xca"}
    binaries = set()
    for candidate in app.rglob("*"):
        if candidate.is_file():
            with candidate.open("rb") as stream:
                if stream.read(4) in magic:
                    binaries.add(candidate.resolve())
    records = []
    problems = []

    def expanded(value, loader):
        return Path(value.replace("@executable_path", str(executable_dir))
                         .replace("@loader_path", str(loader.parent))).resolve()

    def bundled(path):
        return path == app or app in path.parents

    for binary in sorted(binaries):
        raw_paths = rpaths(binary)
        own_paths = [expanded(value, binary) for value in raw_paths]
        inherited_paths = [expanded(value, executable) for value in executable_rpaths]
        search_paths = list(dict.fromkeys(own_paths + inherited_paths))
        path_records = []
        for raw, resolved in zip(raw_paths, own_paths):
            valid = bundled(resolved)
            path_records.append({"value": raw, "resolved": str(resolved), "exists": resolved.exists(),
                                 "insideBundle": valid})
            if not valid:
                problems.append({"file": str(binary.relative_to(app)), "externalRpath": raw})
        ids = install_ids(binary)
        dependency_records = []
        for dependency in dependencies(binary):
            if dependency in ids:
                continue
            record = {"value": dependency}
            if dependency.startswith(("/System/Library/", "/usr/lib/")):
                record.update({"kind": "system", "resolved": dependency, "valid": True})
            else:
                if dependency.startswith("@rpath/"):
                    suffix = dependency[len("@rpath/"):]
                    resolved = next((path / suffix for path in search_paths if (path / suffix).exists()), None)
                elif dependency.startswith(("@loader_path/", "@executable_path/")):
                    resolved = expanded(dependency, binary)
                else:
                    resolved = Path(dependency).resolve()
                valid = resolved is not None and resolved.exists() and bundled(resolved.resolve())
                record.update({"kind": "bundled", "resolved": str(resolved) if resolved else None,
                               "valid": valid})
                if not valid:
                    problems.append({"file": str(binary.relative_to(app)), "unresolvedDependency": dependency})
            dependency_records.append(record)
        architectures = command("lipo", "-archs", str(binary)).strip().split()
        if "arm64" not in architectures:
            problems.append({"file": str(binary.relative_to(app)), "missingArchitecture": "arm64"})
        records.append({"file": str(binary.relative_to(app)), "architectures": architectures,
                        "sha256": hashlib.sha256(binary.read_bytes()).hexdigest(),
                        "rpaths": path_records, "dependencies": dependency_records})

    result = {"passed": not problems,
              "testedAt": datetime.datetime.now(datetime.timezone(datetime.timedelta(hours=8))).isoformat(timespec="seconds"),
              "applicationBundle": str(app), "binaryCount": len(records),
              "dependencyCount": sum(len(record["dependencies"]) for record in records),
              "systemLibraryPolicy": "Apple system frameworks and /usr/lib libraries may reside in the dyld shared cache",
              "problems": problems, "binaries": records}
    args.output.write_text(json.dumps(result, ensure_ascii=False, indent=2) + "\n")
    print(json.dumps({key: result[key] for key in ["passed", "binaryCount", "dependencyCount", "problems"]}, ensure_ascii=False))
    return 0 if result["passed"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
