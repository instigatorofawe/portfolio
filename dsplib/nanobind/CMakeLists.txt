# nanobind Python extension for dsplib.
#
# Locate Python and nanobind. scikit-build-core provides both during a wheel
# build; for a plain `cmake -B build` the interpreter is found on PATH and
# nanobind is located by asking the running interpreter for its CMake package
# dir (so `pip install nanobind` is all a developer needs).
find_package(Python 3.13 COMPONENTS Interpreter Development.Module REQUIRED)

execute_process(
    COMMAND "${Python_EXECUTABLE}" -m nanobind --cmake_dir
    OUTPUT_STRIP_TRAILING_WHITESPACE
    OUTPUT_VARIABLE nanobind_ROOT
)
find_package(nanobind CONFIG REQUIRED)

# The compiled module is imported as `dsplib._dsplib` (see dsplib/__init__.py).
# NB_STATIC links nanobind's runtime statically into this single extension,
# which is the right default for a project shipping one module.
nanobind_add_module(_dsplib NB_STATIC ${CMAKE_CURRENT_SOURCE_DIR}/dsplib_ext.cpp)
target_link_libraries(_dsplib PRIVATE dsplib::core)

# Install the extension next to the pure-Python package inside the wheel. The
# DESTINATION is relative to the wheel's platlib root, matching
# `tool.scikit-build.wheel.packages = ["dsplib"]` in pyproject.toml.
install(TARGETS _dsplib LIBRARY DESTINATION dsplib)

# --- Type stubs ---
# Generate a .pyi stub for the compiled module at build time by importing it and
# introspecting it, so editors and type checkers see the native API. We emit it
# into a source-tree `stubs/dsplib/` package that pyright is pointed at (see
# `tool.pyright.stubPath` in pyproject.toml), so a plain `cmake --build build`
# is enough to give the editor an up-to-date stub. It's regenerated whenever the
# extension changes (DEPENDS) and imported from its freshly built location
# (PYTHON_PATH).
set(DSPLIB_STUB_DIR ${CMAKE_SOURCE_DIR}/stubs/dsplib)
file(MAKE_DIRECTORY ${DSPLIB_STUB_DIR})

nanobind_add_stub(
    _dsplib_stub
    MODULE _dsplib
    OUTPUT ${DSPLIB_STUB_DIR}/_dsplib.pyi
    PYTHON_PATH $<TARGET_FILE_DIR:_dsplib>
    DEPENDS _dsplib
)

# Ship the stub, plus a py.typed marker, inside the wheel so consumers of the
# installed package get types too.
install(FILES ${DSPLIB_STUB_DIR}/_dsplib.pyi DESTINATION dsplib)
install(FILES ${CMAKE_SOURCE_DIR}/dsplib/py.typed DESTINATION dsplib)
