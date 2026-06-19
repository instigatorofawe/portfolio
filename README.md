# Ran Liu - Portfolio Site

## Push-fold solver
```
cargo build --release --target=wasm32-unknown-unknown
```

```
cargo install wasm-bindgen-cli
```

```
wasm-bindgen target/wasm32-unknown-unknown/release/pushfold.wasm --out-dir ../liuran/src/lib/pkg
```
