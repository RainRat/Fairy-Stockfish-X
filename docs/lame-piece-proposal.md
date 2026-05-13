# Lame Piece Refactor Proposal

This note captures the narrow shape I think the lame-piece refactor should keep.
The goal is to support the common variants cleanly without baking in extra
structure that will be hard to unwind later.

## What the model should cover

The refactor should represent a lame move as:

1. a direction entry that already exists in the piece tables
2. an optional lame-path description for that direction
3. a blocker policy describing which squares on the path matter

That is enough for the usual horse/elephant-style variants and keeps the shape
close to the current Betza parser.

## What should stay out of the core shape

I would avoid introducing a separate `stepsLame` table unless the parser really
needs it. It duplicates the existing direction tables and makes the move
generation path harder to reason about.

I would also keep lame movement separate from ski/max/dynamic rider modifiers.
Those are different mechanics. Combining them into one shared abstraction would
make the parser and attack generation less clear without adding much practical
coverage.

## Suggested representation

A small per-direction spec is enough:

- `pathOrder`: `orth-first` or `diag-first`
- `blockPolicy`: `any`, `first`, `mid`, or `last`

That covers the common MAO/MOA-style movement families without forcing the
engine to understand more structure than it needs.

## What this should not do

This should not try to define a universal lame-piece language for every future
variant idea. If a real variant needs a new path rule later, that should be added
then rather than preemptively encoded now.

The main design constraint is to stay flexible, but not overspecified.
