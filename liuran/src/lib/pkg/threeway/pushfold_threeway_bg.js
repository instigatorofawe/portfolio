/**
 * Input validation errors
 * @enum {0 | 1 | 2 | 3 | 4 | 5}
 */
export const SolverError = Object.freeze({
    NonFiniteInput: 0, "0": "NonFiniteInput",
    StackNotPositive: 1, "1": "StackNotPositive",
    NegativeBlindOrAnte: 2, "2": "NegativeBlindOrAnte",
    SmallBlindExceedsStack: 3, "3": "SmallBlindExceedsStack",
    BigBlindExceedsStack: 4, "4": "BigBlindExceedsStack",
    ZeroIterations: 5, "5": "ZeroIterations",
});

/**
 * Solver result: the six averaged strategies, one per decision point.
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
     * @returns {number}
     */
    get exploitability() {
        const ret = wasm.__wbg_get_strategies_exploitability(this.__wbg_ptr);
        return ret;
    }
    /**
     * @returns {Float32Array}
     */
    get bb_call_both() {
        try {
            const retptr = wasm.__wbindgen_add_to_stack_pointer(-16);
            wasm.strategies_bb_call_both(retptr, this.__wbg_ptr);
            var r0 = getDataViewMemory0().getInt32(retptr + 4 * 0, true);
            var r1 = getDataViewMemory0().getInt32(retptr + 4 * 1, true);
            var v1 = getArrayF32FromWasm0(r0, r1).slice();
            wasm.__wbindgen_export2(r0, r1 * 4, 4);
            return v1;
        } finally {
            wasm.__wbindgen_add_to_stack_pointer(16);
        }
    }
    /**
     * @returns {Float32Array}
     */
    get bb_call_bu() {
        try {
            const retptr = wasm.__wbindgen_add_to_stack_pointer(-16);
            wasm.strategies_bb_call_bu(retptr, this.__wbg_ptr);
            var r0 = getDataViewMemory0().getInt32(retptr + 4 * 0, true);
            var r1 = getDataViewMemory0().getInt32(retptr + 4 * 1, true);
            var v1 = getArrayF32FromWasm0(r0, r1).slice();
            wasm.__wbindgen_export2(r0, r1 * 4, 4);
            return v1;
        } finally {
            wasm.__wbindgen_add_to_stack_pointer(16);
        }
    }
    /**
     * @returns {Float32Array}
     */
    get bb_call_sb() {
        try {
            const retptr = wasm.__wbindgen_add_to_stack_pointer(-16);
            wasm.strategies_bb_call_sb(retptr, this.__wbg_ptr);
            var r0 = getDataViewMemory0().getInt32(retptr + 4 * 0, true);
            var r1 = getDataViewMemory0().getInt32(retptr + 4 * 1, true);
            var v1 = getArrayF32FromWasm0(r0, r1).slice();
            wasm.__wbindgen_export2(r0, r1 * 4, 4);
            return v1;
        } finally {
            wasm.__wbindgen_add_to_stack_pointer(16);
        }
    }
    /**
     * @returns {Float32Array}
     */
    get bu_push() {
        try {
            const retptr = wasm.__wbindgen_add_to_stack_pointer(-16);
            wasm.strategies_bu_push(retptr, this.__wbg_ptr);
            var r0 = getDataViewMemory0().getInt32(retptr + 4 * 0, true);
            var r1 = getDataViewMemory0().getInt32(retptr + 4 * 1, true);
            var v1 = getArrayF32FromWasm0(r0, r1).slice();
            wasm.__wbindgen_export2(r0, r1 * 4, 4);
            return v1;
        } finally {
            wasm.__wbindgen_add_to_stack_pointer(16);
        }
    }
    /**
     * @returns {Float32Array}
     */
    get sb_call() {
        try {
            const retptr = wasm.__wbindgen_add_to_stack_pointer(-16);
            wasm.strategies_sb_call(retptr, this.__wbg_ptr);
            var r0 = getDataViewMemory0().getInt32(retptr + 4 * 0, true);
            var r1 = getDataViewMemory0().getInt32(retptr + 4 * 1, true);
            var v1 = getArrayF32FromWasm0(r0, r1).slice();
            wasm.__wbindgen_export2(r0, r1 * 4, 4);
            return v1;
        } finally {
            wasm.__wbindgen_add_to_stack_pointer(16);
        }
    }
    /**
     * @returns {Float32Array}
     */
    get sb_push() {
        try {
            const retptr = wasm.__wbindgen_add_to_stack_pointer(-16);
            wasm.strategies_sb_push(retptr, this.__wbg_ptr);
            var r0 = getDataViewMemory0().getInt32(retptr + 4 * 0, true);
            var r1 = getDataViewMemory0().getInt32(retptr + 4 * 1, true);
            var v1 = getArrayF32FromWasm0(r0, r1).slice();
            wasm.__wbindgen_export2(r0, r1 * 4, 4);
            return v1;
        } finally {
            wasm.__wbindgen_add_to_stack_pointer(16);
        }
    }
}
if (Symbol.dispose) Strategies.prototype[Symbol.dispose] = Strategies.prototype.free;

/**
 * Three-handed push/fold solver.
 *
 * The game: the button open-pushes or folds; the small blind reacts (call a
 * push, or open-push once the button folds); the big blind closes the action
 * at one of three histories. Two-player showdowns and the side pot of a
 * three-way all-in are priced with the heads-up equity table (the third
 * player's card removal is ignored); a three-way all-in's main pot uses the
 * generated three-way pot shares.
 *
 * Runs CFR+ exactly like the heads-up solver — clamped regrets, alternating
 * updates (BU, then SB, then BB, each against the freshest strategies),
 * linear averaging. With three players CFR carries no convergence guarantee
 * to a Nash equilibrium, so `solve` reports NashConv (the summed
 * best-response gaps) as `exploitability`: a small value still certifies the
 * profile is hard to exploit.
 */
export class ThreewaySolver {
    __destroy_into_raw() {
        const ptr = this.__wbg_ptr;
        this.__wbg_ptr = 0;
        ThreewaySolverFinalization.unregister(this);
        return ptr;
    }
    free() {
        const ptr = this.__destroy_into_raw();
        wasm.__wbg_threewaysolver_free(ptr, 0);
    }
    constructor() {
        const ret = wasm.threewaysolver_new();
        this.__wbg_ptr = ret;
        ThreewaySolverFinalization.register(this, this.__wbg_ptr, this);
        return this;
    }
    /**
     * Runs CFR+ (regret clamping, alternating updates, linear averaging)
     * and returns the averaged strategies with their NashConv gap.
     *
     * A solve is O(N^3) per iteration and can run for several seconds; when
     * `progress` is given, it's called with the fraction of iterations
     * complete (in (0, 1], reaching exactly 1 on the last iteration) so a
     * caller can render a progress indicator. Reported at most ~100 times
     * regardless of `iterations`, so the callback overhead stays negligible
     * next to the O(N^3) work it's interleaved with.
     * @param {number} stack_bu
     * @param {number} stack_sb
     * @param {number} stack_bb
     * @param {number} sb
     * @param {number} ante
     * @param {number} iterations
     * @param {Function | null} [progress]
     * @returns {Strategies}
     */
    solve(stack_bu, stack_sb, stack_bb, sb, ante, iterations, progress) {
        try {
            const retptr = wasm.__wbindgen_add_to_stack_pointer(-16);
            wasm.threewaysolver_solve(retptr, this.__wbg_ptr, stack_bu, stack_sb, stack_bb, sb, ante, iterations, isLikeNone(progress) ? 0 : addHeapObject(progress));
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
if (Symbol.dispose) ThreewaySolver.prototype[Symbol.dispose] = ThreewaySolver.prototype.free;
export function __wbg___wbindgen_throw_ea4887a5f8f9a9db(arg0, arg1) {
    throw new Error(getStringFromWasm0(arg0, arg1));
}
export function __wbg_call_5575218572ead796() { return handleError(function (arg0, arg1, arg2) {
    const ret = getObject(arg0).call(getObject(arg1), getObject(arg2));
    return addHeapObject(ret);
}, arguments); }
export function __wbindgen_cast_0000000000000001(arg0) {
    // Cast intrinsic for `F64 -> Externref`.
    const ret = arg0;
    return addHeapObject(ret);
}
export function __wbindgen_object_drop_ref(arg0) {
    takeObject(arg0);
}
const StrategiesFinalization = (typeof FinalizationRegistry === 'undefined')
    ? { register: () => {}, unregister: () => {} }
    : new FinalizationRegistry(ptr => wasm.__wbg_strategies_free(ptr, 1));
const ThreewaySolverFinalization = (typeof FinalizationRegistry === 'undefined')
    ? { register: () => {}, unregister: () => {} }
    : new FinalizationRegistry(ptr => wasm.__wbg_threewaysolver_free(ptr, 1));

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

function handleError(f, args) {
    try {
        return f.apply(this, args);
    } catch (e) {
        wasm.__wbindgen_export(addHeapObject(e));
    }
}

let heap = new Array(1024).fill(undefined);
heap.push(undefined, null, true, false);

let heap_next = heap.length;

function isLikeNone(x) {
    return x === undefined || x === null;
}

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
