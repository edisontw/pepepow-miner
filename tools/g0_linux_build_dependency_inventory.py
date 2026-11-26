#!/usr/bin/env python3
"""Record actual Linux CMake compile inputs and link membership.

This is a technical build inventory. It records compiler-generated Make
dependency files and GNU ld map files; it does not assign legal status.
"""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import platform
import re
import shlex
import subprocess
import sys


CORE_PIN = "bc08108ceccd957c79040dc7fedc1d56d8d47f97"
OPENLIBM_PIN = "9fbeafcd4f1b6ef6aa3946c1c8faead50f38a94d"


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def run(*args: str) -> str:
    return subprocess.check_output(args, text=True).strip()


def git_head(root: Path) -> tuple[str, bool]:
    head = run("git", "-C", str(root), "rev-parse", "HEAD")
    dirty = bool(run("git", "-C", str(root), "status", "--porcelain"))
    return head, dirty


def git_blobs(root: Path) -> dict[str, str]:
    raw = subprocess.check_output(["git", "-C", str(root), "ls-tree", "-r", "-z", "HEAD"])
    result: dict[str, str] = {}
    for record in raw.split(b"\0"):
        if not record:
            continue
        metadata, name = record.split(b"\t", 1)
        fields = metadata.decode("ascii").split()
        if len(fields) == 3 and fields[1] == "blob":
            result[name.decode("utf-8", errors="surrogateescape")] = fields[2]
    return result


def make_words(text: str) -> list[str]:
    """Split a Make depfile RHS, handling escaped whitespace and continuations."""
    words: list[str] = []
    word: list[str] = []
    index = 0
    while index < len(text):
        char = text[index]
        if char == "\\" and index + 1 < len(text):
            following = text[index + 1]
            if following == "\n":
                index += 2
                continue
            if following == "\r" and index + 2 < len(text) and text[index + 2] == "\n":
                index += 3
                continue
            word.append(following)
            index += 2
            continue
        if char == "$" and index + 1 < len(text) and text[index + 1] == "$":
            word.append("$")
            index += 2
            continue
        if char.isspace():
            if word:
                words.append("".join(word))
                word.clear()
            index += 1
            continue
        word.append(char)
        index += 1
    if word:
        words.append("".join(word))
    return words


def depfile_paths(depfile: Path, build_root: Path) -> tuple[Path, list[Path]]:
    text = depfile.read_text(encoding="utf-8", errors="replace")
    return depfile_paths_from_text(text, build_root, depfile)


def depfile_paths_from_text(text: str, build_root: Path, source_label: Path) -> tuple[Path, list[Path]]:
    separator = text.find(":")
    if separator < 0:
        raise ValueError(f"Malformed compiler dependency output: {source_label}")
    target_words = make_words(text[:separator])
    dependencies = make_words(text[separator + 1 :])
    if len(target_words) != 1:
        raise ValueError(f"Unexpected dependency target in {source_label}")

    def absolute(value: str) -> Path:
        path = Path(value)
        return path.resolve() if path.is_absolute() else (build_root / path).resolve()

    return absolute(target_words[0]), list(dict.fromkeys(absolute(value) for value in dependencies))


def compiler_dependency_output(arguments: list[str], directory: str, output: Path, source: Path) -> tuple[list[Path], list[str]]:
    """Ask the exact compiler command to traverse includes for assembly TUs lacking .d files."""
    command = []
    index = 0
    while index < len(arguments):
        value = arguments[index]
        if value == "-o":
            index += 2
            continue
        if value == "-c":
            index += 1
            continue
        if not value.startswith("-"):
            candidate = Path(value)
            if not candidate.is_absolute():
                candidate = (Path(directory) / candidate).resolve()
            else:
                candidate = candidate.resolve()
            if candidate == source.resolve():
                index += 1
                continue
        command.append(value)
        index += 1
    command.extend(["-M", "-MT", str(output), str(source)])
    result = subprocess.check_output(command, cwd=directory, text=True, stderr=subprocess.PIPE)
    _, dependencies = depfile_paths_from_text(result, Path(directory), source)
    return dependencies, command


def source_from_compile_command(row: dict) -> tuple[Path, Path, list[str]]:
    directory = Path(row["directory"]).resolve()
    source = Path(row["file"])
    if not source.is_absolute():
        source = (directory / source).resolve()
    arguments = row.get("arguments")
    if arguments is None:
        arguments = shlex.split(row["command"])
    else:
        arguments = list(arguments)
    try:
        output_index = arguments.index("-o") + 1
        output = Path(arguments[output_index])
    except (ValueError, IndexError) as exc:
        raise ValueError(f"Compile command has no -o output: {row}") from exc
    if not output.is_absolute():
        output = (directory / output).resolve()
    return source, output, arguments


def file_record(path: Path, roots: list[tuple[str, Path]], blob_maps: dict[str, dict[str, str]]) -> dict:
    real = path.resolve(strict=True)
    category = "system-or-toolchain"
    repository = None
    relative = None
    git_blob = None
    for name, root in roots:
        try:
            relative = real.relative_to(root.resolve()).as_posix()
            category = name
            repository = str(root.resolve())
            git_blob = blob_maps.get(name, {}).get(relative)
            break
        except ValueError:
            continue
    if category == "system-or-toolchain" and real.as_posix().startswith("/usr/include/openssl/"):
        category = "OpenSSL-header"
    return {
        "path": str(path),
        "resolved_path": str(real),
        "category": category,
        "repository_root": repository,
        "repository_path": relative,
        "git_blob_sha1": git_blob,
        "sha256": sha256(real),
    }


def inventory_build(build_root: Path, family: str, roots: list[tuple[str, Path]], blob_maps: dict[str, dict[str, str]]) -> tuple[list[dict], dict[str, dict]]:
    database = build_root / "compile_commands.json"
    if not database.is_file():
        raise ValueError(f"Missing compile database: {database}")
    commands = json.loads(database.read_text(encoding="utf-8"))
    command_by_output: dict[Path, tuple[Path, list[str], dict]] = {}
    for row in commands:
        source, output, arguments = source_from_compile_command(row)
        if output in command_by_output:
            raise ValueError(f"Duplicate compile output: {output}")
        command_by_output[output] = (source, arguments, row)

    translation_units = []
    file_paths: set[Path] = set()
    seen_outputs: set[Path] = set()
    for depfile in sorted(build_root.rglob("*.o.d")):
        output, dependencies = depfile_paths(depfile, build_root)
        compiled = command_by_output.get(output)
        if compiled is None:
            raise ValueError(f"Dependency file has no compile database entry: {depfile} ({output})")
        source, arguments, row = compiled
        seen_outputs.add(output)
        file_paths.update(dependencies)
        try:
            target = re.search(r"CMakeFiles/([^/]+)\.dir/", output.as_posix()).group(1)
        except AttributeError as exc:
            raise ValueError(f"Cannot determine CMake target for {output}") from exc
        translation_units.append({
            "target": target,
            "source": str(source),
            "source_sha256": sha256(source),
            "object": str(output),
            "object_sha256": sha256(output),
            "depfile": str(depfile),
            "depfile_sha256": sha256(depfile),
            "compiler_dependency_list_available": True,
            "compile_directory": row["directory"],
            "compile_command": row.get("command", shlex.join(arguments)),
            "included_inputs": [str(path) for path in dependencies],
        })
    for output in sorted(set(command_by_output) - seen_outputs):
        source, arguments, row = command_by_output[output]
        source = source.resolve(strict=True)
        dependencies, dependency_command = compiler_dependency_output(
            arguments, row["directory"], output, source
        )
        file_paths.update(dependencies)
        try:
            target = re.search(r"CMakeFiles/([^/]+)\.dir/", output.as_posix()).group(1)
        except AttributeError as exc:
            raise ValueError(f"Cannot determine CMake target for {output}") from exc
        translation_units.append({
            "target": target,
            "source": str(source),
            "source_sha256": sha256(source),
            "object": str(output),
            "object_sha256": sha256(output),
            "depfile": None,
            "depfile_sha256": None,
            "compiler_dependency_list_available": True,
            "dependency_capture_method": "same compiler, same include and preprocessor flags, -M traversal (CMake emitted no .o.d file for this translation unit)",
            "dependency_capture_command": shlex.join(dependency_command),
            "compile_directory": row["directory"],
            "compile_command": row.get("command", shlex.join(arguments)),
            "included_inputs": [str(path) for path in dependencies],
        })
    file_rows = {str(path): file_record(path, roots, blob_maps) for path in sorted(file_paths)}
    return translation_units, file_rows


def archive_members(path: Path) -> list[str]:
    return run("ar", "t", str(path)).splitlines()


def link_map_members(path: Path) -> list[dict[str, str]]:
    pattern = re.compile(r"^\s*(\S+\.a)\(([^)]+)\)\s*$")
    members = []
    for line in path.read_text(encoding="utf-8", errors="replace").splitlines():
        match = pattern.match(line)
        if match:
            members.append({"archive": match.group(1), "member": match.group(2)})
    unique = {(item["archive"], item["member"]): item for item in members}
    return [unique[key] for key in sorted(unique)]


NOTICE_PATTERNS = (
    ("FDLIBM", re.compile(r"FDLIBM|SUN MICROSYSTEMS", re.I)),
    ("ISC (Moshier notice; see LICENSE.md)", re.compile(r"MOSHIER", re.I)),
    ("BSD-style", re.compile(r"REDISTRIBUTION AND USE IN SOURCE AND BINARY FORMS", re.I)),
    ("Public domain", re.compile(r"PUBLIC DOMAIN", re.I)),
    ("MIT", re.compile(r"SPDX-LICENSE-IDENTIFIER:\s*MIT|MIT LICENSE", re.I)),
    ("LGPL", re.compile(r"LGPL|LESSER GENERAL PUBLIC LICENSE", re.I)),
)


def source_notice_record(source: Path) -> dict:
    lines = source.read_text(encoding="utf-8", errors="replace").splitlines()[:100]
    head = "\n".join(lines)
    candidates = [name for name, pattern in NOTICE_PATTERNS if pattern.search(head)]
    signals = [
        {"line": index, "text": line.strip()}
        for index, line in enumerate(lines, 1)
        if re.search(r"copyright|SPDX-License-Identifier|permission|redistribut|license|licence|public domain", line, re.I)
    ]
    return {
        "license_header_candidates": candidates,
        "license_review_status": "REVIEW_REQUIRED" if len(candidates) != 1 else "EVIDENCE_ONLY",
        "header_notice_signals": signals,
    }


def enrich_linked_members(members: list[dict], miner_tus: list[dict], openlibm_tus: list[dict], miner_build: Path, openlibm_build: Path) -> list[dict]:
    object_maps = {
        "libpepepow_core_reference.a": {Path(row["object"]).name: row for row in miner_tus},
        "libopenlibm.a": {Path(row["object"]).name: row for row in openlibm_tus},
    }
    result = []
    for item in members:
        row = dict(item)
        archive_name = Path(item["archive"]).name
        candidate = object_maps.get(archive_name, {}).get(item["member"])
        if candidate is not None:
            row.update({
                "source": candidate["source"],
                "source_sha256": candidate["source_sha256"],
                "object_sha256": candidate["object_sha256"],
                "compiler_dependency_list_available": candidate["compiler_dependency_list_available"],
            })
            if archive_name == "libopenlibm.a":
                row.update(source_notice_record(Path(candidate["source"])))
        else:
            row["source_mapping"] = "not present in the project or OpenLibm compile databases; inspect the named system/toolchain archive"
        archive_path = Path(item["archive"])
        if not archive_path.is_absolute():
            archive_path = (miner_build / archive_path).resolve()
        if archive_path.is_file():
            row["archive_resolved_path"] = str(archive_path)
            row["archive_sha256"] = sha256(archive_path)
        result.append(row)
    return result


def ldd_records(executable: Path) -> list[dict]:
    output = run("ldd", str(executable))
    result = []
    for line in output.splitlines():
        match = re.match(r"\s*(\S+)\s+=>\s+(\S+)", line)
        if not match:
            continue
        name, resolved = match.groups()
        path = Path(resolved)
        if path.is_file():
            resolved_path = path.resolve()
            record = {"soname": name, "resolved_path": str(resolved_path), "sha256": sha256(resolved_path)}
            try:
                owner = run("dpkg-query", "-S", str(resolved_path)).splitlines()[0].partition(": ")[0]
                package = owner.split(":")[0]
                package_version = run("dpkg-query", "-W", "-f=${Version}", owner)
                copyright_path = Path("/usr/share/doc") / package / "copyright"
                record["debian_package"] = owner
                record["debian_version"] = package_version
                if copyright_path.is_file():
                    record["debian_copyright_file"] = str(copyright_path)
                    record["debian_copyright_sha256"] = sha256(copyright_path)
            except (subprocess.CalledProcessError, IndexError):
                pass
            result.append(record)
        else:
            result.append({"soname": name, "resolved_path": resolved, "sha256": None})
    return result


def source_notice_file(root: Path, name: str, blobs: dict[str, str]) -> dict:
    path = root / name
    return {
        "path": str(path),
        "repository_path": name,
        "git_blob_sha1": blobs.get(name),
        "sha256": sha256(path),
    }


def build_configuration(build_root: Path) -> str:
    cache = (build_root / "CMakeCache.txt").read_text(encoding="utf-8", errors="replace")
    match = re.search(r"^CMAKE_BUILD_TYPE:STRING=([^\r\n]*)", cache, flags=re.MULTILINE)
    if match is None or match.group(1) not in {"Debug", "Release", "RelWithDebInfo", "MinSizeRel"}:
        raise ValueError(f"Cannot determine a supported single-config CMake build type: {build_root}")
    return match.group(1)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--miner-build", type=Path, required=True)
    parser.add_argument("--openlibm-build", type=Path, required=True)
    parser.add_argument("--miner-source", type=Path, required=True)
    parser.add_argument("--core-source", type=Path, required=True)
    parser.add_argument("--openlibm-source", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    try:
        miner_build = args.miner_build.resolve()
        openlibm_build = args.openlibm_build.resolve()
        miner_source = args.miner_source.resolve()
        core_source = args.core_source.resolve()
        openlibm_source = args.openlibm_source.resolve()
        core_head, core_dirty = git_head(core_source)
        openlibm_head, openlibm_dirty = git_head(openlibm_source)
        miner_head, miner_dirty = git_head(miner_source)
        if core_head != CORE_PIN or core_dirty:
            raise ValueError(f"Core checkout is not clean at pin {CORE_PIN}: {core_head}")
        if openlibm_head != OPENLIBM_PIN or openlibm_dirty:
            raise ValueError(f"OpenLibm checkout is not clean at pin {OPENLIBM_PIN}: {openlibm_head}")
        if not (miner_build / "CMakeCache.txt").is_file() or not (openlibm_build / "CMakeCache.txt").is_file():
            raise ValueError("Both build directories must have CMakeCache.txt")
        roots = [
            ("miner", miner_source),
            ("PEPEPOW-Core", core_source),
            ("OpenLibm-source", openlibm_source),
            ("miner-build-generated", miner_build),
            ("OpenLibm-build-generated", openlibm_build),
        ]
        blob_maps = {
            "miner": git_blobs(miner_source),
            "PEPEPOW-Core": git_blobs(core_source),
            "OpenLibm-source": git_blobs(openlibm_source),
        }
        miner_tus, miner_files = inventory_build(miner_build, "miner", roots, blob_maps)
        openlibm_tus, openlibm_files = inventory_build(openlibm_build, "openlibm", roots, blob_maps)
        miner_configuration = build_configuration(miner_build)
        openlibm_configuration = build_configuration(openlibm_build)
        if miner_configuration != openlibm_configuration:
            raise ValueError(
                f"Miner/OpenLibm configurations differ: {miner_configuration} vs {openlibm_configuration}"
            )
        all_files = {**miner_files, **openlibm_files}
        maps_root = miner_build / "link-maps"
        maps = {}
        executables = {}
        runtime_packages = {}
        for target in ("pepepow-kat", "pepepow-offline-search", "offline_stratum_replay", "test_native_stratum_search", "offline_stratum_miner"):
            executable = miner_build / target
            link_map = maps_root / f"{target}.map"
            if not executable.is_file() or not link_map.is_file():
                raise ValueError(f"Missing built executable or linker map for {target}")
            maps[target] = {
                "path": str(link_map),
                "sha256": sha256(link_map),
                "archive_members_loaded": enrich_linked_members(
                    link_map_members(link_map), miner_tus, openlibm_tus, miner_build, openlibm_build
                ),
            }
            link_file = miner_build / "CMakeFiles" / f"{target}.dir" / "link.txt"
            executables[target] = {
                "path": str(executable),
                "sha256": sha256(executable),
                "link_command_file": str(link_file),
                "link_command_sha256": sha256(link_file),
                "dynamic_dependencies": ldd_records(executable),
            }
            for dependency in executables[target]["dynamic_dependencies"]:
                if "debian_package" in dependency:
                    key = dependency["debian_package"]
                    runtime_packages[key] = {
                        "package": key,
                        "version": dependency["debian_version"],
                        "copyright_file": dependency.get("debian_copyright_file"),
                        "copyright_sha256": dependency.get("debian_copyright_sha256"),
                    }
        core_archive = miner_build / "libpepepow_core_reference.a"
        openlibm_archive = openlibm_build / "libopenlibm.a"
        if not core_archive.is_file() or not openlibm_archive.is_file():
            raise ValueError("Missing static reference archives")
        cmake = run("cmake", "--version").splitlines()[0]
        gcc = run("gcc", "--version").splitlines()[0]
        gxx = run("g++", "--version").splitlines()[0]
        manifest = {
            "schema_version": 1,
            "scope": f"Actual Linux x86_64 {miner_configuration} CMake build inputs, compiler depfiles, static archives, per-executable GNU ld map membership, and runtime shared libraries. Technical evidence only; not a legal conclusion or redistribution approval.",
            "build": {
                "miner_commit": miner_head,
                "miner_worktree_dirty": miner_dirty,
                "core_commit": core_head,
                "openlibm_commit": openlibm_head,
                "configuration": miner_configuration,
                "platform": platform.platform(),
                "machine": platform.machine(),
                "libc": platform.libc_ver(),
                "cmake": cmake,
                "c_compiler": gcc,
                "cxx_compiler": gxx,
                "openssl": run("openssl", "version"),
                "openssl_dev_packages": run(
                    "dpkg-query", "-W", "-f=${binary:Package} ${Version}\\n", "libssl-dev", "libssl3t64"
                ).splitlines(),
            },
            "translation_unit_counts": {
                "miner_build": len(miner_tus),
                "openlibm_build": len(openlibm_tus),
            },
            "translation_units": {
                "miner_build": miner_tus,
                "openlibm_build": openlibm_tus,
            },
            "included_files": list(all_files.values()),
            "static_archives": [
                {"path": str(core_archive), "sha256": sha256(core_archive), "member_count": len(archive_members(core_archive)), "members": archive_members(core_archive)},
                {"path": str(openlibm_archive), "sha256": sha256(openlibm_archive), "member_count": len(archive_members(openlibm_archive)), "members": archive_members(openlibm_archive)},
            ],
            "upstream_notice_files": {
                "PEPEPOW-Core": [source_notice_file(core_source, "COPYING", blob_maps["PEPEPOW-Core"])],
                "OpenLibm": [source_notice_file(openlibm_source, "LICENSE.md", blob_maps["OpenLibm-source"])],
            },
            "executables": executables,
            "runtime_system_packages": [runtime_packages[key] for key in sorted(runtime_packages)],
            "link_maps": maps,
            "limitations": [
                f"This inventory covers one Linux x86_64 {miner_configuration} build only.",
                "System package license texts are not embedded in this technical inventory; see the third-party notice decision document.",
                "No physical Windows 10/11 build evidence is claimed.",
                "A compiler input/header inventory and linker map do not establish rights to redistribute any source or binary.",
            ],
        }
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(json.dumps(manifest, indent=2, sort_keys=True) + "\n", encoding="utf-8")
        print(f"Wrote {args.output}: {len(miner_tus)} miner translation units, {len(openlibm_tus)} OpenLibm translation units, {len(all_files)} included build inputs.")
        return 0
    except (OSError, ValueError, subprocess.CalledProcessError, json.JSONDecodeError) as exc:
        parser.exit(1, f"Inventory failed: {exc}\n")


if __name__ == "__main__":
    sys.exit(main())
