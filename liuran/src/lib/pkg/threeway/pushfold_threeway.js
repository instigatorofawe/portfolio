/* @ts-self-types="./pushfold_threeway.d.ts" */
import * as wasm from "./pushfold_threeway_bg.wasm";
import { __wbg_set_wasm } from "./pushfold_threeway_bg.js";

__wbg_set_wasm(wasm);

export {
    Strategies, ThreewaySolver
} from "./pushfold_threeway_bg.js";
