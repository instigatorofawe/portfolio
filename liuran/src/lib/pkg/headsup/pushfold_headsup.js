/* @ts-self-types="./pushfold_headsup.d.ts" */
import * as wasm from "./pushfold_headsup_bg.wasm";
import { __wbg_set_wasm } from "./pushfold_headsup_bg.js";

__wbg_set_wasm(wasm);

export {
    HeadsUpSolver, Strategies
} from "./pushfold_headsup_bg.js";
