# Ran Liu - Portfolio Site

All commands below are run from the repository root.

## Push-fold solver

Build the WASM solver and emit JS bindings into the SvelteKit app:

```
./scripts/build-pushfold.sh
```

Or via pnpm:

```
pnpm build:pushfold
```

This requires the `wasm32-unknown-unknown` target and `wasm-bindgen-cli`:

```
rustup target add wasm32-unknown-unknown
cargo install wasm-bindgen-cli
```

The script runs the equivalent of:

```
cargo build --release --target=wasm32-unknown-unknown --manifest-path pushfold/Cargo.toml
wasm-bindgen pushfold/target/wasm32-unknown-unknown/release/pushfold.wasm --out-dir liuran/src/lib/pkg
```

## Web app (liuran)

The SvelteKit app lives in `liuran/`. Its pnpm scripts can be run from the root:

```
pnpm install:liuran   # install the app's dependencies
pnpm dev              # start the dev server
pnpm build            # production build
pnpm preview          # preview the production build
```
