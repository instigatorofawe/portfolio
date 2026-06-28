/**
 * Why the solver rejected its inputs. Exported as a `#[wasm_bindgen]` enum, so
 * it crosses into JS as a discriminant the caller can compare against
 * `SolverError.*` rather than parsing a free-text string. The human-readable
 * message for each variant lives on the consumer (see PushFold.svelte).
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
     * iter was zero.
     */
    ZeroIterations: 4, "4": "ZeroIterations",
});

/**
 * @param {number} stack
 * @param {number} sb
 * @param {number} ante
 * @param {number} iter
 * @returns {Float32Array}
 */
export function solve_push_fold(stack, sb, ante, iter) {
    try {
        const retptr = wasm.__wbindgen_add_to_stack_pointer(-16);
        wasm.solve_push_fold(retptr, stack, sb, ante, iter);
        var r0 = getDataViewMemory0().getInt32(retptr + 4 * 0, true);
        var r1 = getDataViewMemory0().getInt32(retptr + 4 * 1, true);
        var r2 = getDataViewMemory0().getInt32(retptr + 4 * 2, true);
        var r3 = getDataViewMemory0().getInt32(retptr + 4 * 3, true);
        if (r3) {
            throw takeObject(r2);
        }
        var v1 = getArrayF32FromWasm0(r0, r1).slice();
        wasm.__wbindgen_export(r0, r1 * 4, 4);
        return v1;
    } finally {
        wasm.__wbindgen_add_to_stack_pointer(16);
    }
}
export function __wbindgen_cast_0000000000000001(arg0) {
    // Cast intrinsic for `F64 -> Externref`.
    const ret = arg0;
    return addHeapObject(ret);
}
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

function getObject(idx) { return heap[idx]; }

let heap = new Array(1024).fill(undefined);
heap.push(undefined, null, true, false);

let heap_next = heap.length;

function takeObject(idx) {
    const ret = getObject(idx);
    dropObject(idx);
    return ret;
}


let wasm;
export function __wbg_set_wasm(val) {
    wasm = val;
}
