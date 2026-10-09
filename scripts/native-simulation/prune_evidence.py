"""Prune validated successful JSONL evidence; keep receipts/hashes and explicit examples.

Plan first with --root/--keep/--manifest. Review the manifest, then apply it with
--manifest PATH --apply. Failures, unfinished corpora and non-JSONL files stay.
"""
from __future__ import annotations
import argparse
import hashlib
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
BUILD = ROOT / "build"


def checked(path: Path) -> Path:
    absolute = path.absolute()
    resolved = path.resolve(strict=True)
    if not resolved.is_relative_to(BUILD.resolve()) or resolved == BUILD.resolve():
        raise ValueError(f"Evidence path is outside build: {path}")
    for part in (absolute, *absolute.parents):
        if part == ROOT:
            break
        if part.is_symlink() or part.is_junction():
            raise ValueError(f"Linked evidence path: {part}")
    return resolved


def digest(path: Path) -> str:
    with path.open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


def read(path):
    return json.loads(path.read_text(encoding="utf-8-sig"))


def plan(roots, keep):
    retained = [checked(path) for path in keep]
    files = {}
    corpora = []
    for root in roots:
        for receipt in checked(root).rglob("corpus_result.json"):
            checked(receipt)
            data = read(receipt)
            if data.get("status") != "pass":
                continue
            corpora.append(dict(path=str(receipt.relative_to(ROOT)), sha256=digest(receipt)))
            for fixture in data["fixtures"]:
                if fixture.get("status") != "pass":
                    continue
                for run in (receipt.parent / fixture["id"]).glob("run-*"):
                    if not (run / "invocation.json").is_file() or read(run / "invocation.json").get("status") != "pass":
                        continue
                    for path in (run / "output").glob("*.jsonl*"):
                        if not (path.name.endswith(".jsonl") or path.name.endswith(".jsonl.gz")):
                            continue
                        path = checked(path)
                        if any(path == item or path.is_relative_to(item) for item in retained):
                            continue
                        files[str(path.relative_to(ROOT))] = dict(bytes=path.stat().st_size, sha256=digest(path))
    return dict(schema=1, status="planned", scope="validated successful JSONL only; failures and receipts retained",
                retained=[str(path.relative_to(ROOT)) for path in retained], corpora=corpora,
                files=[dict(path=path, **value) for path,value in sorted(files.items())],
                bytes=sum(item["bytes"] for item in files.values()))


def apply(manifest, destination):
    if manifest["status"] != "planned":
        raise ValueError("Expected a reviewed, unapplied plan")
    # Verify the complete explicit set before any deletion. Never recursive delete.
    for receipt in manifest["corpora"]:
        if digest(checked(ROOT / receipt["path"])) != receipt["sha256"]:
            raise ValueError("Validation receipt changed")
    for item in manifest["files"]:
        path = checked(ROOT / item["path"])
        if path.stat().st_size != item["bytes"] or digest(path) != item["sha256"]:
            raise ValueError(f"Evidence changed: {path}")
    for item in manifest["files"]:
        checked(ROOT / item["path"]).unlink()
    manifest["status"] = "applied"
    destination.write_text(json.dumps(manifest, indent=2)+"\n", encoding="utf-8")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, action="append", default=[])
    parser.add_argument("--keep", type=Path, action="append", default=[])
    parser.add_argument("--manifest", type=Path, required=True)
    parser.add_argument("--apply", action="store_true")
    args = parser.parse_args()
    if args.apply:
        manifest_path = checked(args.manifest)
        result = read(manifest_path)
        apply(result, manifest_path)
    else:
        if not args.root or args.manifest.exists():
            parser.error("Provide roots and a fresh manifest path")
        checked(args.manifest.parent)
        result = plan(args.root, args.keep)
        args.manifest.write_text(json.dumps(result, indent=2)+"\n", encoding="utf-8")
    print(json.dumps({"status":result["status"],"files":len(result["files"]),"bytes":result["bytes"]}))
