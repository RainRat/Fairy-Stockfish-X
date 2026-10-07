#!/usr/bin/env python3
"""Exercise parameter ownership across successful and partial network loads."""
from pathlib import Path
import subprocess
import sys
import tempfile


engine = Path(sys.argv[1]).resolve()
source = Path(__file__).resolve().parents[1] / "src/nn-3475407dc199.nnue"
data = source.read_bytes()
with tempfile.TemporaryDirectory(prefix="fsx-nnue-loading-") as directory:
    root = Path(directory)
    valid = root / "chess-valid.nnue"
    valid.write_bytes(data)
    commands = ["setoption name Use NNUE value true"]
    exports = []
    failed_exports = []
    for index, length in enumerate((32, len(data) // 2, len(data) - 1)):
        broken = root / f"chess-truncated-{index}.nnue"
        broken.write_bytes(data[:length])
        failed = root / f"failed-{index}.nnue"
        exported = root / f"export-{index}.nnue"
        commands += [
            f"setoption name EvalFile value {broken}",
            f"export_net {failed}",
            f"setoption name EvalFile value {valid}",
            "position startpos moves e2e4 e7e5",
            "eval",
            f"export_net {exported}",
        ]
        failed_exports.append(failed)
        exports.append(exported)
    variant = root / "variants.ini"
    variant.write_text(
        "[nnue-layout:chess]\nmaxFile = 7\ncastling = false\n"
        "nnueAlias = chess\nstartFen = 6k/7/7/7/7/7/7/K6 w - - 0 1\n"
    )
    failed_exports.append(root / "failed-layout.nnue")
    exports.append(root / "export-layout.nnue")
    commands += [
        f"setoption name VariantPath value {variant}",
        "setoption name UCI_Variant value nnue-layout",
        f"export_net {failed_exports[-1]}",
        "setoption name UCI_Variant value chess",
        "position startpos moves e2e4 e7e5",
        "eval",
        f"export_net {exports[-1]}",
    ]
    commands.append("quit")
    result = subprocess.run(
        [str(engine)], input="\n".join(commands) + "\n", text=True,
        capture_output=True, timeout=60, check=True,
    )
    evaluations = [line for line in result.stdout.splitlines()
                   if line.startswith("Final evaluation")]
    assert len(evaluations) == len(exports) and len(set(evaluations)) == 1, result.stdout
    assert all("NNUE" in line for line in evaluations), result.stdout
    assert result.stdout.count("Failed to export a net") == len(failed_exports), result.stdout
    assert all(not path.exists() for path in failed_exports)
    assert all(path.read_bytes() == data for path in exports)
print("nnue loading regression passed")
