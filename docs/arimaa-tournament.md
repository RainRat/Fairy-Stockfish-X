# Arimaa protocol tournament runner

`tools/arimaa_tournament.py` runs fixed-setup complete-turn games between an
AEI engine such as Akimot and an `arimaa=yes` FSX binary.

The runner is deliberately the referee at the protocol boundary. It translates
Akimot's physical AEI steps and FSX's coordinate notation into one canonical
turn representation, applies pushes and pulls atomically, enforces the official
four-physical-step maximum, removes unsupported trap pieces, and performs
goal, rabbit-loss, no-move, and repetition adjudication. A push may therefore
occupy two physical steps even though FSX represents it as one compact move.
It also accepts Akimot's `PieceSquarex` trap-removal annotations as
non-physical records; the referee performs the removal itself.

The initial version starts from the completed standard profile position used by
the Arimaa smoke tests. It does not generate setup placements and does not
generate NNUE data.

## Build

Build an FSX binary with the Arimaa path enabled:

```sh
tests/build.sh ARCH=x86-64-modern largeboards=yes all=yes \
  arimaa=yes nnue=yes EXE=stockfish-arimaa-large
```

Build Akimot separately using its own source instructions. The runner accepts
an executable command, so an AEI launch wrapper and its options can be passed
directly.

## Run

```sh
python3 tools/arimaa_tournament.py \
  --fsx src/stockfish-arimaa-large \
  --akimot "sh -c 'cd /path/to/akimot && exec ./akimot -e'" \
  --variant-path src/variants.ini \
  --depth 2 \
  --games 20 \
  --log .local/build/arimaa-akimot.jsonl
```

Colors alternate by game. The JSONL log records the raw engine move, the
normalized AEI steps, the FSX replay notation, the boundary FEN, a position
key, and any adjudication result. Illegal moves and protocol failures stop the
run rather than being counted as ordinary game losses.

A short native smoke test can use the repository's deterministic fake AEI
engine, which checks protocol wiring without requiring Akimot:

```sh
python3 tools/arimaa_tournament.py \
  --fsx src/stockfish-arimaa-large \
  --akimot 'python3 tests/python/fake_aei_engine.py' \
  --variant-path src/variants.ini \
  --depth 1 --games 1 --max-turns 2
```

This is only an adapter/referee smoke test, not an engine-strength test.
