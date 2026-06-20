#!/usr/bin/env bash
# Build the push-fold solver to WASM and emit JS bindings into the SvelteKit app.
# Run from anywhere; paths resolve relative to the repo root.
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$REPO_ROOT"

cargo build --release --target=wasm32-unknown-unknown --manifest-path pushfold/Cargo.toml

wasm-bindgen pushfold/target/wasm32-unknown-unknown/release/pushfold.wasm \
	--out-dir liuran/src/lib/pkg
