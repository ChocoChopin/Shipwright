"""Compare compact action-event receipts for roll braking and slash recovery."""
import argparse
import json
from pathlib import Path


def events(path):
    return [json.loads(line) for line in Path(path).read_text(encoding="utf-8").splitlines()]


def roll_speeds(path):
    return [row["velocity"] for row in events(path) if row["action"] == "Player_Action_80842180"]


def slash_interval(path):
    rows = events(path)
    start = next(r["time_q"] for r in rows
                 if r["animation"]["resource"].endswith("link_anchor_Rside_kiru_endR"))
    stop = next(r["time_q"] for r in rows if r["time_q"] > start
                and r["animation"]["resource"].endswith("link_anchor_waitR"))
    return stop - start


def main():
    p = argparse.ArgumentParser(description=__doc__)
    for name in ("roll20", "roll_native", "roll_smooth", "slash20", "slash120", "output"):
        p.add_argument("--" + name.replace("_", "-"), required=True)
    a = p.parse_args()
    native, original, smooth = roll_speeds(a.roll_native), roll_speeds(a.roll20), roll_speeds(a.roll_smooth)
    assert len(original) == len(native) == len(smooth) == 2, "must cover two completed rolls"
    assert all(abs(x - y) < 1e-5 for x, y in zip(original, native)), "original braking differs"
    assert all(x > y + 6 for x, y in zip(smooth, native)), "smooth momentum option not exercised"
    old, new = slash_interval(a.slash20), slash_interval(a.slash120)
    assert old == new == 78, "targeted horizontal recovery interval differs"
    result = {"status": "pass", "roll20": original, "roll_native": native,
              "roll_smooth": smooth, "slash20_quanta": old, "slash120_quanta": new}
    Path(a.output).write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(result))


if __name__ == "__main__":
    main()
