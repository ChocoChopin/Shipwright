#!/usr/bin/env python3
"""Inventory temporal *candidates* and actor coverage; never modify game source.

Run from anywhere: python scripts/native-simulation/inventory.py
Default output is docs/native-simulation/inventory/. Only Git-tracked C/C++
sources in the explicit runtime roots are read. No ROMs/assets/build outputs are
opened. Regex matches are leads for manual review, not conversion instructions.
"""

from __future__ import annotations

import argparse
import collections
import csv
import hashlib
import json
import re
import subprocess
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
ROOTS = (
    (".", "soh/src"),
    (".", "soh/include"),
    (".", "soh/soh"),
    ("libultraship", "src"),
    ("libultraship", "include"),
)
EXTENSIONS = {".c", ".cpp", ".cc", ".cxx", ".h", ".hpp", ".inc"}
# Multiple rules can match one source line. Classes are deliberately candidates.
# I (semantic ambiguity) is never assigned automatically.
RULE_DEFS = (
    ("update_rate", "ABDG", r"\bR_UPDATE_RATE\b"),
    ("timer_identifier", "DGI", r"\b\w*(?:timer|cooldown|countdown|life)\w*\b"),
    ("decrement_macro", "D", r"\bDECR\s*\("),
    ("frame_identifier", "AG", r"\b\w*(?:frame|Frame)\w*\b"),
    ("frame_modulus", "G", r"(?:frame\w*[^;\n]{0,80}(?:%|&)|(?:%|&)[^;\n]{0,80}frame)"),
    ("increment_decrement", "ADEG", r"(?:\+\+|--)[\w*(]|[\w)\]](?:\+\+|--)"),
    ("position_rotation_add", "ABH", r"(?:pos|rot|yaw|pitch|roll|angle|rotation)\w*(?:\.[xyz])?\s*[+\-]="),
    ("velocity_acceleration", "B", r"\b\w*(?:velocity|gravity|accel|speedXZ|linearVelocity)\w*\b"),
    ("multiply_assign", "C", r"\*="),
    ("smooth_approach", "CI", r"\b(?:Math|Camera)_(?:\w*Smooth\w*|\w*Approach\w*|\w*Lerp\w*)\s*\("),
    ("step_helper", "AEH", r"\bMath_\w*StepTo\w*\s*\("),
    ("movement_helper", "ABH", r"\b(?:Actor_(?:Move\w*|UpdatePos\w*|UpdateVelocity\w*)|SkelAnime_UpdateTranslation)\s*\("),
    ("rng_call", "F", r"\b(?:Rand_\w+|rand|srand|random|random_device|mt19937|Audio_NextRandom|RandInit|next32)\s*\("),
    ("animation", "ADG", r"\b(?:\w*Animation_\w+|SkelAnime_\w+|playSpeed|morphWeight|morphRate)\b"),
    ("texture_scroll", "AG", r"\b\w*(?:TexScroll|TextureScroll)\w*\b"),
    ("collision", "EH", r"\b(?:CollisionCheck|Collider|BgCheck|DynaPoly|DynaPolyActor)_\w+\s*\("),
    ("audio_event", "DEG", r"\b(?:Audio_|Sfx_|SEQCMD_|AudioOcarina_|Audio_Play|AudioSfx_)\w*\s*\("),
    ("state_action", "EI", r"\b(?:actionFunc|actionIndex|stateFlags\w*|csCtx|pauseCtx|gameOverCtx)\b"),
    ("day_time", "ADG", r"\b(?:dayTime|skyboxTime|gTimeSpeed|timeIncrement|sunPos)\b"),
    ("draw_dependency", "EH", r"\b(?:\w*PostLimbDraw\w*|\w*OverrideLimbDraw\w*|isDrawn|ACTOR_FLAG_INSIDE_CULLING_VOLUME)\b"),
    ("input_edges", "DEG", r"\b(?:press|rel|cur|prev)\.button\b|\bCHECK_BTN_\w+\s*\("),
)
# I needs human judgment. Rules may indicate an ambiguity review, but cannot
# diagnose it: remove I from emitted mathematical-class candidates.
RULES = [(name, classes.replace("I", ""), re.compile(pattern, re.I))
         for name, classes, pattern in RULE_DEFS]


def git(directory: Path, *args: str) -> str:
    return subprocess.check_output(
        ["git", "-C", str(directory), *args], encoding="utf-8", errors="replace"
    ).strip()


def runtime_files() -> list[Path]:
    paths: set[Path] = set()
    for repository, subtree in ROOTS:
        checkout = ROOT / repository
        if not (checkout / subtree).is_dir():
            raise RuntimeError(f"Missing source root {checkout / subtree}; initialize submodules first")
        tracked = git(checkout, "ls-files", "-z", "--", subtree)
        for name in tracked.split("\0"):
            if name and Path(name).suffix.lower() in EXTENSIONS:
                paths.add(checkout / name)
    return sorted(paths, key=lambda path: path.relative_to(ROOT).as_posix())


def write_csv(path: Path, fields: list[str], rows: list[dict]) -> None:
    with path.open("w", encoding="utf-8", newline="") as stream:
        writer = csv.DictWriter(stream, fields, lineterminator="\n")
        writer.writeheader()
        writer.writerows(rows)


def actors(paths: list[Path], counts: dict[str, int]) -> tuple[list[dict], list[dict]]:
    table = ROOT / "soh/include/tables/actor_table.h"
    definitions = re.compile(
        r"/\*\s*(0x[0-9A-Fa-f]+)\s*\*/\s*DEFINE_ACTOR(_INTERNAL)?\(\s*(\w+),\s*(\w+),\s*(\w+)\)"
    )
    descriptions = dict(re.findall(
        r'\{\s*(ACTOR_\w+),\s*"([^"\n]+)"\s*\}',
        (ROOT / "soh/soh/ActorDB.cpp").read_text(encoding="utf-8"),
    ))
    by_dir: dict[str, list[str]] = collections.defaultdict(list)
    init_sources: dict[str, list[str]] = collections.defaultdict(list)
    for path in paths:
        rel = path.relative_to(ROOT).as_posix()
        if "/overlays/actors/" in rel and path.suffix == ".c":
            by_dir[path.parent.name.lower()].append(rel)
        if path.suffix == ".c":
            for name in re.findall(r"\bActorInit\s+(\w+)_InitVars\s*=", path.read_text(encoding="utf-8", errors="replace")):
                init_sources[name].append(rel)
    rows = []
    mapped: set[str] = set()
    for line_number, line in enumerate(table.read_text(encoding="utf-8").splitlines(), 1):
        match = definitions.search(line)
        if not match:
            continue
        actor_id, internal, name, enum, allocation = match.groups()
        if name == "Player":
            sources = by_dir["ovl_player_actor"]
        elif internal:
            sources = init_sources.get(name, [])
        else:
            sources = by_dir.get("ovl_" + name.lower(), [])
        mapped.update(sources)
        rows.append({
            "actor_id": actor_id, "enum": enum, "name": name,
            "description": descriptions.get(enum, ""), "allocation": allocation,
            "table_line": line_number, "sources": ";".join(sources),
            "candidate_lines": sum(counts.get(source, 0) for source in sources),
        })
    unmatched = [{"source": source, "candidate_lines": counts.get(source, 0)}
                 for values in by_dir.values() for source in values if source not in mapped]
    return rows, sorted(unmatched, key=lambda row: row["source"])


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, default=ROOT / "docs/native-simulation/inventory")
    args = parser.parse_args()
    output = args.output.resolve()
    # Follow the permanent project-local disposable-artifact constraint.
    if not output.is_relative_to(ROOT):
        parser.error("--output must be inside this repository workspace")
    output.mkdir(parents=True, exist_ok=True)
    paths = runtime_files()
    candidates = []
    files = []
    class_counts: collections.Counter[str] = collections.Counter()
    rule_counts: collections.Counter[str] = collections.Counter()
    for path in paths:
        rel = path.relative_to(ROOT).as_posix()
        raw = path.read_bytes()
        source = raw.decode("utf-8", errors="replace")
        line_count = 0
        file_classes: collections.Counter[str] = collections.Counter()
        for line_number, line in enumerate(source.splitlines(), 1):
            matches = [(name, classes) for name, classes, pattern in RULES if pattern.search(line)]
            if not matches:
                continue
            classes = sorted(set("".join(classes for _, classes in matches)))
            rule_ids = [name for name, _ in matches]
            candidates.append({"path": rel, "line": line_number,
                               "classes": "".join(classes), "rules": ";".join(rule_ids)})
            line_count += 1
            class_counts.update(classes)
            file_classes.update(classes)
            rule_counts.update(rule_ids)
        files.append({"path": rel, "source_sha256": hashlib.sha256(raw).hexdigest(),
                      "lines": len(source.splitlines()), "candidate_lines": line_count,
                      "class_counts": dict(sorted(file_classes.items()))})
    actor_rows, unmatched = actors(paths, {row["path"]: row["candidate_lines"] for row in files})
    write_csv(output / "timing-candidates.csv", ["path", "line", "classes", "rules"], candidates)
    write_csv(output / "actors.csv", ["actor_id", "enum", "name", "description", "allocation",
                                       "table_line", "sources", "candidate_lines"], actor_rows)
    write_csv(output / "unmapped-actor-sources.csv", ["source", "candidate_lines"], unmatched)
    summary = {
        "schema": 1,
        "purpose": "Heuristic source leads and coverage denominators; not a complete audit or conversion status",
        "source_commit": git(ROOT, "log", "-1", "--format=%H", "--", "soh/src", "soh/soh", "soh/include"),
        "libultraship_commit": git(ROOT / "libultraship", "rev-parse", "HEAD"),
        "roots": [{"repository": repo, "subtree": subtree} for repo, subtree in ROOTS],
        "extensions": sorted(EXTENSIONS),
        "limits": [
            "Regex matches include comments, strings, declarations, reads and false positives.",
            "No AST, alias/dataflow, call-graph, semantic unit or update-vs-draw analysis.",
            "Unknown fields and helper-induced timing can be missed; zero matches never proves rate safety.",
            "E/H candidates identify review sites, not proof of resolution consequences. I requires human classification.",
            "Actor table excludes unset slots and dynamic registrations; unmapped overlays are separately enumerated.",
            "Assets, resource tables outside include, build tooling and third-party libultraship/extern sources excluded.",
        ],
        "source_files": len(files), "source_lines": sum(row["lines"] for row in files),
        "files_with_candidates": sum(bool(row["candidate_lines"]) for row in files),
        "candidate_lines": len(candidates), "actor_table_entries": len(actor_rows),
        "actors_missing_sources": [row["enum"] for row in actor_rows if not row["sources"]],
        "unmapped_actor_sources": len(unmatched),
        "class_candidate_counts": dict(sorted(class_counts.items())),
        "rule_candidate_counts": dict(sorted(rule_counts.items())),
        "rules": [{"id": name, "candidate_classes": classes.replace("I", ""), "regex": pattern}
                  for name, classes, pattern in RULE_DEFS],
        "files": files,
    }
    (output / "summary.json").write_text(json.dumps(summary, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    print(json.dumps({key: summary[key] for key in (
        "source_files", "source_lines", "files_with_candidates", "candidate_lines",
        "actor_table_entries", "actors_missing_sources", "unmapped_actor_sources",
    )}, indent=2))


if __name__ == "__main__":
    main()
