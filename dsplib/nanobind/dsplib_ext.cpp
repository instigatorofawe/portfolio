#include <nanobind/nanobind.h>

#include <dsplib/dsplib.hpp>

namespace nb = nanobind;

NB_MODULE(_dsplib, m) {
    m.doc() = "dsplib native extension";

    m.def("db_to_linear", &dsplib::db_to_linear, nb::arg("db"),
          "Convert a gain in decibels to a linear amplitude ratio.");
    m.def("linear_to_db", &dsplib::linear_to_db, nb::arg("linear"),
          "Convert a linear amplitude ratio to a gain in decibels.");
}
