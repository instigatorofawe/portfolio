#!/usr/bin/env bash
# Build a push-fold solver to WASM and emit JS bindings into the SvelteKit
# app. Run from anywhere; paths resolve relative to the repo root.
#
# Usage: build-pushfold.sh <headsup|threeway>
set -euo pipefail

SOLVER="${1:-}"
case "$SOLVER" in
	headsup | threeway) ;;
	*)
		echo "usage: $(basename "$0") <headsup|threeway>" >&2
		exit 1
		;;
esac

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

# Likewise remap the local stdlib source checkout back to the toolchain's
# virtual /rustc/<commit-hash> prefix. The distributed toolchain already embeds
# that virtual path in panic-location strings, but when the `rust-src`
# component is installed (rust-analyzer pulls it in), rustc resolves std code
# instantiated in this crate back to the builder's
# $SYSROOT/lib/rustlib/src/rust checkout, embedding the builder's home
# directory. Machines without rust-src (CI) are unaffected: the prefix never
# appears there, so the flag is a no-op and both builds emit /rustc/<hash>.
SYSROOT="$(rustc --print sysroot)"
RUSTC_COMMIT_HASH="$(rustc -vV | sed -n 's/^commit-hash: //p')"
export RUSTFLAGS="${RUSTFLAGS:-} --remap-path-prefix=${CARGO_HOME_PATH}=/cargo --remap-path-prefix=${SYSROOT}/lib/rustlib/src/rust=/rustc/${RUSTC_COMMIT_HASH}"

cargo build --release --target=wasm32-unknown-unknown --manifest-path pushfold/Cargo.toml \
	-p "pushfold-${SOLVER}"

# --remove-producers-section drops the telemetry `producers` custom section,
# which records the toolchain provenance (rustc/clang/walrus versions). Its
# contents vary by build host even with an identical toolchain, so leaving it in
# makes the emitted wasm differ between machines (local vs CI) despite identical
# code. It has no runtime effect.
wasm-bindgen "pushfold/target/wasm32-unknown-unknown/release/pushfold_${SOLVER}.wasm" \
	--out-dir "liuran/src/lib/pkg/${SOLVER}" \
	--remove-producers-section
