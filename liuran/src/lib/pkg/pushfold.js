/* @ts-self-types="./pushfold.d.ts" */
import * as wasm from './pushfold_bg.wasm';
import { __wbg_set_wasm } from './pushfold_bg.js';

__wbg_set_wasm(wasm);

export { solve_push_fold } from './pushfold_bg.js';
