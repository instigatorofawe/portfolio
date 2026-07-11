/// Input validation errors shared by the solver crates.
///
/// The superset of every solver's rejection reasons (heads-up never emits
/// `BigBlindExceedsStack`); each crate's `validate_inputs` uses the variants
/// its game has. Deliberately *not* a `#[wasm_bindgen]` enum: that conversion
/// crosses the boundary as a bare variant number, so a JS `catch` would
/// surface e.g. "3" to the user. The solvers' wasm `solve` wrappers instead
/// convert through `JsError` (enabled by the `std::error::Error` impl below),
/// so JS catches an `Error` whose message is the `Display` string.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum SolverError {
    NonFiniteInput,
    StackNotPositive,
    NegativeBlindOrAnte,
    SmallBlindExceedsStack,
    BigBlindExceedsStack,
    ZeroIterations,
}

impl std::fmt::Display for SolverError {
    /// User-facing copy, not debug output: the solver UIs render
    /// `Error.message` directly in their error alerts, alongside client-side
    /// validation text of the same register. Phrasing stays neutral enough to
    /// read correctly in both the heads-up UI (one shared stack) and the
    /// three-way UI (per-seat stacks).
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        f.write_str(match self {
            SolverError::NonFiniteInput => "Inputs must be finite numbers.",
            SolverError::StackNotPositive => "Stacks must be positive.",
            SolverError::NegativeBlindOrAnte => "Small blind and ante must be non-negative.",
            SolverError::SmallBlindExceedsStack => "Small blind cannot exceed the stack.",
            SolverError::BigBlindExceedsStack => "BB stack must be at least 1 big blind.",
            SolverError::ZeroIterations => "Iterations must be at least 1.",
        })
    }
}

/// Enables the blanket `From<SolverError> for JsError`, which is what lets
/// the wasm `solve` wrappers convert with `?`.
impl std::error::Error for SolverError {}
