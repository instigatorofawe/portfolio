#!/usr/bin/env bash
# Build the three-way push-fold solver to WASM and emit JS bindings into the
# SvelteKit app. Run from anywhere; paths resolve relative to the repo root.
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$REPO_ROOT"

# Remap the builder's $CARGO_HOME to a fixed virtual prefix. rustc bakes the
# absolute path of dependency sources into panic-location strings (e.g.
# $CARGO_HOME/registry/src/.../wasm-bindgen-x.y.z/src/externref.rs), which would
# otherwise embed the builder's home directory and make the emitted wasm differ
# between machines (local vs CI). Both environments apply the same rule, so the
# path collapses to an identical string and the committed pkg/ stays in sync.
# (Cargo's stable `trim-paths` profile option would do this, but it isn't
# stabilized in the pinned toolchain yet.)
CARGO_HOME_PATH="${CARGO_HOME:-$HOME/.cargo}"
export RUSTFLAGS="${RUSTFLAGS:-} --remap-path-prefix=${CARGO_HOME_PATH}=/cargo"

cargo build --release --target=wasm32-unknown-unknown --manifest-path pushfold/Cargo.toml \
	-p pushfold-threeway

# --remove-producers-section drops the telemetry `producers` custom section,
# which records the toolchain provenance (rustc/clang/walrus versions). Its
# contents vary by build host even with an identical toolchain, so leaving it in
# makes the emitted wasm differ between machines (local vs CI) despite identical
# code. It has no runtime effect.
wasm-bindgen pushfold/target/wasm32-unknown-unknown/release/pushfold_threeway.wasm \
	--out-dir liuran/src/lib/pkg/threeway \
	--remove-producers-section
