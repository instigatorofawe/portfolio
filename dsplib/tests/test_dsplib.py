import math

import dsplib


def test_db_to_linear_unity():
    assert dsplib.db_to_linear(0.0) == 1.0


def test_db_to_linear_twenty_db():
    assert math.isclose(dsplib.db_to_linear(20.0), 10.0)


def test_round_trip():
    assert math.isclose(dsplib.linear_to_db(dsplib.db_to_linear(-6.0)), -6.0)
