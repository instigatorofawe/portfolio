/**
 * Push/fold solver handle. Constructing it builds the (constant) equity and
 * matchup lookup tables once; reuse the same handle to solve at different
 * stakes rather than paying that setup per solve.
 */
export class PushFoldSolver {
    __destroy_into_raw() {
        const ptr = this.__wbg_ptr;
        this.__wbg_ptr = 0;
        PushFoldSolverFinalization.unregister(this);
        return ptr;
    }
    free() {
        const ptr = this.__destroy_into_raw();
        wasm.__wbg_pushfoldsolver_free(ptr, 0);
    }
    constructor() {
        const ret = wasm.pushfoldsolver_new();
        this.__wbg_ptr = ret;
        PushFoldSolverFinalization.register(this, this.__wbg_ptr, this);
        return this;
    }
    /**
     * Runs CFR+ for `iterations` and returns the averaged strategies, or
     * throws a [`SolverError`] into JS if the stakes are invalid.
     * @param {number} stack
     * @param {number} sb
     * @param {number} ante
     * @param {number} iterations
     * @returns {Strategies}
     */
    solve(stack, sb, ante, iterations) {
        try {
            const retptr = wasm.__wbindgen_add_to_stack_pointer(-16);
            wasm.pushfoldsolver_solve(retptr, this.__wbg_ptr, stack, sb, ante, iterations);
            var r0 = getDataViewMemory0().getInt32(retptr + 4 * 0, true);
            var r1 = getDataViewMemory0().getInt32(retptr + 4 * 1, true);
            var r2 = getDataViewMemory0().getInt32(retptr + 4 * 2, true);
            if (r2) {
                throw takeObject(r1);
            }
            return Strategies.__wrap(r0);
        } finally {
            wasm.__wbindgen_add_to_stack_pointer(16);
        }
    }
}
if (Symbol.dispose) PushFoldSolver.prototype[Symbol.dispose] = PushFoldSolver.prototype.free;

/**
 * Why the solver rejected its inputs. Annotated for wasm-bindgen so it crosses
 * into JS as a numeric discriminant (see `bindings.rs`); the human-readable
 * message for each variant lives on the consumer.
 * @enum {0 | 1 | 2 | 3 | 4}
 */
export const SolverError = Object.freeze({
    /**
     * stack, SB, or ante was NaN or infinite.
     */
    NonFiniteInput: 0, "0": "NonFiniteInput",
    /**
     * stack was zero or negative.
     */
    StackNotPositive: 1, "1": "StackNotPositive",
    /**
     * SB or ante was negative.
     */
    NegativeBlindOrAnte: 2, "2": "NegativeBlindOrAnte",
    /**
     * SB was larger than the stack.
     */
    SmallBlindExceedsStack: 3, "3": "SmallBlindExceedsStack",
    /**
     * iterations was zero.
     */
    ZeroIterations: 4, "4": "ZeroIterations",
});

/**
 * Converged strategies from a solve. `bu_push` and `bb_call` are each 169
 * frequencies in `[0, 1]`, row-major over the 13x13 hand grid (the button's
 * push frequency and the big blind's call frequency respectively); wasm-bindgen
 * surfaces the getters to JS as `Float32Array`s.
 */
export class Strategies {
    static __wrap(ptr) {
        const obj = Object.create(Strategies.prototype);
        obj.__wbg_ptr = ptr;
        StrategiesFinalization.register(obj, obj.__wbg_ptr, obj);
        return obj;
    }
    __destroy_into_raw() {
        const ptr = this.__wbg_ptr;
        this.__wbg_ptr = 0;
        StrategiesFinalization.unregister(this);
        return ptr;
    }
    free() {
        const ptr = this.__destroy_into_raw();
        wasm.__wbg_strategies_free(ptr, 0);
    }
    /**
     * Big blind call frequency per infoset (169 entries).
     * @returns {Float32Array}
     */
    get bb_call() {
        try {
            const retptr = wasm.__wbindgen_add_to_stack_pointer(-16);
            wasm.strategies_bb_call(retptr, this.__wbg_ptr);
            var r0 = getDataViewMemory0().getInt32(retptr + 4 * 0, true);
            var r1 = getDataViewMemory0().getInt32(retptr + 4 * 1, true);
            var v1 = getArrayF32FromWasm0(r0, r1).slice();
            wasm.__wbindgen_export(r0, r1 * 4, 4);
            return v1;
        } finally {
            wasm.__wbindgen_add_to_stack_pointer(16);
        }
    }
    /**
     * Button push frequency per infoset (169 entries).
     * @returns {Float32Array}
     */
    get bu_push() {
        try {
            const retptr = wasm.__wbindgen_add_to_stack_pointer(-16);
            wasm.strategies_bu_push(retptr, this.__wbg_ptr);
            var r0 = getDataViewMemory0().getInt32(retptr + 4 * 0, true);
            var r1 = getDataViewMemory0().getInt32(retptr + 4 * 1, true);
            var v1 = getArrayF32FromWasm0(r0, r1).slice();
            wasm.__wbindgen_export(r0, r1 * 4, 4);
            return v1;
        } finally {
            wasm.__wbindgen_add_to_stack_pointer(16);
        }
    }
    /**
     * Nash gap of the returned strategy pair, in big-blind units per deal.
     * @returns {number}
     */
    get exploitability() {
        const ret = wasm.strategies_exploitability(this.__wbg_ptr);
        return ret;
    }
}
if (Symbol.dispose) Strategies.prototype[Symbol.dispose] = Strategies.prototype.free;
export function __wbg___wbindgen_throw_ea4887a5f8f9a9db(arg0, arg1) {
    throw new Error(getStringFromWasm0(arg0, arg1));
}
export function __wbindgen_cast_0000000000000001(arg0) {
    // Cast intrinsic for `F64 -> Externref`.
    const ret = arg0;
    return addHeapObject(ret);
}
const PushFoldSolverFinalization = (typeof FinalizationRegistry === 'undefined')
    ? { register: () => {}, unregister: () => {} }
    : new FinalizationRegistry(ptr => wasm.__wbg_pushfoldsolver_free(ptr, 1));
const StrategiesFinalization = (typeof FinalizationRegistry === 'undefined')
    ? { register: () => {}, unregister: () => {} }
    : new FinalizationRegistry(ptr => wasm.__wbg_strategies_free(ptr, 1));

function addHeapObject(obj) {
    if (heap_next === heap.length) heap.push(heap.length + 1);
    const idx = heap_next;
    heap_next = heap[idx];

    heap[idx] = obj;
    return idx;
}

function dropObject(idx) {
    if (idx < 1028) return;
    heap[idx] = heap_next;
    heap_next = idx;
}

function getArrayF32FromWasm0(ptr, len) {
    ptr = ptr >>> 0;
    return getFloat32ArrayMemory0().subarray(ptr / 4, ptr / 4 + len);
}

let cachedDataViewMemory0 = null;
function getDataViewMemory0() {
    if (cachedDataViewMemory0 === null || cachedDataViewMemory0.buffer.detached === true || (cachedDataViewMemory0.buffer.detached === undefined && cachedDataViewMemory0.buffer !== wasm.memory.buffer)) {
        cachedDataViewMemory0 = new DataView(wasm.memory.buffer);
    }
    return cachedDataViewMemory0;
}

let cachedFloat32ArrayMemory0 = null;
function getFloat32ArrayMemory0() {
    if (cachedFloat32ArrayMemory0 === null || cachedFloat32ArrayMemory0.byteLength === 0) {
        cachedFloat32ArrayMemory0 = new Float32Array(wasm.memory.buffer);
    }
    return cachedFloat32ArrayMemory0;
}

function getStringFromWasm0(ptr, len) {
    return decodeText(ptr >>> 0, len);
}

let cachedUint8ArrayMemory0 = null;
function getUint8ArrayMemory0() {
    if (cachedUint8ArrayMemory0 === null || cachedUint8ArrayMemory0.byteLength === 0) {
        cachedUint8ArrayMemory0 = new Uint8Array(wasm.memory.buffer);
    }
    return cachedUint8ArrayMemory0;
}

function getObject(idx) { return heap[idx]; }

let heap = new Array(1024).fill(undefined);
heap.push(undefined, null, true, false);

let heap_next = heap.length;

function takeObject(idx) {
    const ret = getObject(idx);
    dropObject(idx);
    return ret;
}

let cachedTextDecoder = new TextDecoder('utf-8', { ignoreBOM: true, fatal: true });
cachedTextDecoder.decode();
const MAX_SAFARI_DECODE_BYTES = 2146435072;
let numBytesDecoded = 0;
function decodeText(ptr, len) {
    numBytesDecoded += len;
    if (numBytesDecoded >= MAX_SAFARI_DECODE_BYTES) {
        cachedTextDecoder = new TextDecoder('utf-8', { ignoreBOM: true, fatal: true });
        cachedTextDecoder.decode();
        numBytesDecoded = len;
    }
    return cachedTextDecoder.decode(getUint8ArrayMemory0().subarray(ptr, ptr + len));
}


let wasm;
export function __wbg_set_wasm(val) {
    wasm = val;
}
