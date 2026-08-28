This is part of the bachelor thesis "Proofs for SAT solving with cascading
preprocessing" written by Anna Görth in 2026 at KIT.
Supervison by Dr. Dominik Schreiber. 

# Proof Checker

Verifies a chain of SAT proofs: a sequence of proof steps in which each step
transforms one CNF formula into the next, with the last step deriving the empty
clause. Individual steps may use different proof formats — currently SR (which
subsumes RAT, RUP and PR), LRAT, and the parallel proof framework PalRUP.

## Requirements

- CMake ≥ 3.16
- A C++20 compiler
- `make` and `git`
- Network access at build time

The external checkers ([dsr-trim / lsr-check](https://github.com/ccodel/dsr-trim)
and [PalRUP-Check](https://github.com/rubenGoetz/PalRUP-Check)) do **not** need
to be installed separately — the build clones them at pinned commits into
`extern/` and builds them automatically.

Developed and tested on Linux with GCC 11.5 and CMake 3.31.

## Build

```sh
cmake -B build
cmake --build build
```

The binary is written to `build/proof_checker`. The first build takes noticeably
longer because the external checkers are fetched and compiled.

## Usage

```sh
proof_checker <formula.cnf> <proof-dir>
```

- `formula.cnf` — the original formula in DIMACS CNF, before any preprocessing.
- `proof-dir` — a directory holding the proof chain (see below).

## Proof directory layout

The directory is described entirely by its file names — there is no manifest.

| File | Role |
| --- | --- |
| `step<N>.<ext>` | The proof for step *N*, numbered from 1. Extension selects the format. |
| `post<N>.cnf` | The formula *after* step *N*, in DIMACS CNF. |

For a chain of *N* steps there must be exactly *N* − 1 `post` files: `post<i>` is
both the expected result of step *i* and the input formula of step *i* + 1. There
is no `post<N>` because the final step must derive the empty clause rather than a
successor formula. Any file that does not match either pattern is an error, so
the directory must contain nothing else.

A two-step chain therefore looks like this:

```
unsat300cubes-0.cnf              ← the original formula, passed separately
unsat300cubes-0_proof/
├── step1.sr                     formula.cnf  ──step1──▶  post1.cnf
├── post1.cnf
└── step2.palrup                 post1.cnf    ──step2──▶  empty clause
```

Continuity is checked as *containment*, not equality: step *i* may derive
additional clauses beyond those in `post<i>.cnf`, which the next step simply
never uses. What must not happen is a clause in `post<i>.cnf` that the proof does
not account for.

## Supported formats

| Extension | Format | Verified by |
| --- | --- | --- |
| `.sr` | SR — backwards compatible with RAT, RUP and PR (this is without hints so could also be called .dsr, however we use .sr) | `dsr-trim` + `lsr-check` |
| `.lrat` | LRAT | `lsr-check` |
| `.palrup` | PalRUP (parallel/distributed proof fragments) | `palrup_local_check`, `palrup_redistribute`, `palrup_confirm` |

## External tools

Both are fetched at a pinned commit by `CMakeLists.txt` and keep their own
licenses:

| Tool | Provides | Pinned commit | License |
| --- | --- | --- | --- |
| [dsr-trim](https://github.com/ccodel/dsr-trim) | `dsr-trim`, `lsr-check` | `b812f5445a8297aed1fc25777de87237faa8750a` | Apache-2.0 |
| [PalRUP-Check](https://github.com/rubenGoetz/PalRUP-Check) | `palrup_local_check`, `palrup_redistribute`, `palrup_confirm` | `d9382fb4b0acf094034ee91e2ed0a22b1b479c1d` | MIT |

Neither is redistributed with this repository. To build without network access,
clone both repositories yourself, check out the commits above, and place them at
`extern/dsr_trim` and `extern/palrup_check` before running CMake — the build
uses `UPDATE_DISCONNECTED`, so existing sources at those paths are left alone.

## License

This project is licensed under the MIT License — see [LICENSE](LICENSE).
The external tools listed above are covered by their own licenses.
