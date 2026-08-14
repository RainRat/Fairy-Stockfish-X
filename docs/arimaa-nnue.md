# Arimaa NNUE research profile

FSX now contains a contained `arimaa` profile and a matching NNUE feature
contract. It is intended as a research starting point for self-play and
supervised training while keeping the ordinary variant engine unchanged.

## Build and run

```sh
tests/build.sh ARCH=x86-64-modern largeboards=yes all=yes nnue=yes \
  arimaa=yes \
  EXE=stockfish-arimaa
src/stockfish-arimaa
```

The UCI setup is:

```text
setoption name VariantPath value src/variants.ini
setoption name UCI_Variant value arimaa
position startpos
go depth 8
```

The profile implements the standard two-rank setup phase through pockets and
mandatory drops. After setup, a turn contains one to four same-side steps and
is exchanged as one complete-turn token, for example
`b2b3,a1b1,e4e5,c1d1`. A one-step token ends the turn early; a fourth step
hands the turn over automatically. Semicolons are accepted as an unambiguous
alternative separator for clients that need one. `0000` is not used as an
Arimaa turn delimiter.

Intermediate steps are applied transactionally while generating or searching
turns, but Arimaa FEN and the NNUE input are always at a completed-turn
boundary. The internal continuation counter is never serialized.

The rules currently covered are custom rabbit/cat/dog/horse/camel/elephant
pieces, strength-sensitive freezing with orthogonal support, trap removal,
pushes and pulls, no ordinary captures, and rabbit goal flags.

## NNUE contract

The profile uses the standard position-derived `HalfKAv2Variants` feature family
with no king anchor and six board piece types (rabbit, cat, dog, horse, camel,
elephant). The compound-turn bookkeeping is an engine rule, not an NNUE input,
so this feature contract matches the external `variant-nnue-tools` trainer.
Name networks with the `arimaa-` prefix; `nnueAlias` makes the loader accept that
prefix for the `arimaa` variant.

Training records should preserve the variant name, the exact boundary FEN, side
to move, game result, target-generation method, and the profile/config revision.
Do not add a partial-turn field to the NNUE record or mix these records with
orthodox chess or other FSX NNUE feature contracts.

The external `~/variant-nnue-tools` trainer can create the matching trainer
configuration through its `trainer_config` command. This work does not generate
training data.
