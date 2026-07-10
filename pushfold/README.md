# pushfold

A Rust workspace implementing the push/fold poker solvers, compiled to
WASM for the browser. See the [repository root README](../README.md) for
the full build pipeline, cross-language dependencies, and Nx commands; this
file covers only what's specific to this package.

## Crates

| Crate              | What it is                                                                |
| ------------------- | ---------------------------------------------------------------------------- |
| `shared`           | Game-agnostic constants (grid dimensions, infoset count) and every generated lookup table, under `generated::headsup` / `generated::threeway`. The `hands` label grid is gated behind a `hands` Cargo feature so it stays out of release/WASM builds unless a dependent opts in as a dev-dependency (tests/benches use it to pretty-print strategies) |
| `headsup`          | Heads-up (BU vs. BB) CFR+ solver, exposed to JS via `wasm-bindgen` as `HeadsUpSolver`. Builds its lookup-derived state once on construction and is reused per solve |
| `threeway`         | Three-handed (BU / SB / BB) CFR+ solver. Mirrors `headsup`'s design, but the extra player turns the matchup/equity tables into 169^3 tensors (`ndarray`) and the game tree grows to six binary decision points; two-player showdowns reuse the heads-up equity table, three-way all-ins use the generated three-way pot shares |

All three tables in `shared::generated` are produced by the `lookups` C++
generator and are not hand-written — see [`../lookups/README.md`](../lookups/README.md).

Versions for dependencies shared across crates (`wasm-bindgen`, `js-sys`,
`nalgebra`, `ndarray`, `criterion`, and the internal `pushfold-shared` path
dependency) are pinned once in the workspace root's `[workspace.dependencies]`
and inherited by each crate via `{ workspace = true }`, so bumping one only
means editing `Cargo.toml` at the workspace root.

## Building

The WASM builds depend on the generated lookup tables existing in
`shared/src/generated/`; generate those first (`pnpm lookups` at the repo
root) if you're building standalone rather than through the full Nx
pipeline.

```
# heads-up, via the Nx target (what `pnpm build:wasm` / `pnpm build` run)
npx nx run pushfold:build-headsup-wasm

# or equivalently, standalone:
./scripts/build-pushfold.sh headsup
# which is:
cargo build --release --target=wasm32-unknown-unknown --manifest-path pushfold/Cargo.toml -p pushfold-headsup
wasm-bindgen pushfold/target/wasm32-unknown-unknown/release/pushfold_headsup.wasm --out-dir liuran/src/lib/pkg/headsup
```

Three-way follows the same shape via `scripts/build-pushfold.sh threeway` /
`npx nx run pushfold:build-threeway-wasm`, emitting into
`liuran/src/lib/pkg/threeway`.

## Testing

```
cargo test --manifest-path pushfold/Cargo.toml
```

The `[profile.test]` in the workspace `Cargo.toml` forces `opt-level = 3`:
the three-way solver's tests contract 169^3 tensors every CFR iteration,
which is unusably slow at the default debug optimization level.

## Benchmarking

```
cargo bench --manifest-path pushfold/Cargo.toml -p pushfold-headsup
cargo bench --manifest-path pushfold/Cargo.toml -p pushfold-threeway
```

Both use Criterion (`headsup/benches/solver.rs`, `threeway/benches/solver.rs`):
a `new` benchmark for solver construction, and a `solve` group across a few
iteration counts. The three-way group runs far fewer iterations at a smaller
sample size (`group.sample_size(10)`) than heads-up's — each three-way CFR+
iteration is an O(N^3) tensor contraction versus heads-up's O(N^2), so
matching heads-up's iteration counts would make a full `cargo bench` run
impractically slow.
