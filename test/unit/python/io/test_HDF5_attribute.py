#!/usr/bin/env py.test

"""Unit tests for the Attribute interface of the HDF5 io library"""

# Copyright (C) 2013 Chris Richardson
#
# This file is part of DOLFIN.
#
# DOLFIN is free software: you can redistribute it and/or modify
# it under the terms of the GNU Lesser General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# DOLFIN is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
# GNU Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public License
# along with DOLFIN. If not, see <http://www.gnu.org/licenses/>.

import pytest
import os
from dolfin import *
import numpy
from dolfin_utils.test import skip_if_not_HDF5, fixture, tempdir


@pytest.yield_fixture
def attr(tempdir):
    hdf_file = HDF5File(mpi_comm_world(), os.path.join(tempdir, "hdf_file.h5"), "w")
    x = Vector(mpi_comm_world(), 123)
    hdf_file.write(x, "/a_vector")
    attr = hdf_file.attributes("/a_vector")

    yield attr

    del hdf_file

@skip_if_not_HDF5
def test_read_write_str_attribute(attr):
    attr['name'] = 'Vector'
    assert attr.type_str("name") == "string"
    assert attr['name'] == 'Vector'

@skip_if_not_HDF5
def test_read_write_float_attribute(attr):
    attr['val'] = -9.2554
    assert attr.type_str("val") == "float"
    assert attr['val'] == -9.2554

@skip_if_not_HDF5
def test_read_write_int_attribute(attr):
    attr['val'] = 1
    assert attr.type_str("val") == "int"
    assert attr['val'] == 1

@skip_if_not_HDF5
def test_read_write_vec_float_attribute(attr):
    vec = numpy.array([1,2,3,4.5], dtype='float')
    attr['val'] = vec
    ans = attr['val']
    assert attr.type_str("val") == "vectorfloat"
    assert len(vec) == len(ans)
    for val1, val2 in zip(vec, ans):
        assert val1 == val2

@skip_if_not_HDF5
def test_read_write_vec_int_attribute(attr):
    vec = numpy.array([1,2,3,4,5], dtype=numpy.uintp)
    attr['val'] = vec
    ans = attr['val']
    assert attr.type_str("val") == "vectorint"
    assert len(vec) == len(ans)
    for val1, val2 in zip(vec, ans):
        assert val1 == val2

@skip_if_not_HDF5
def test_attribute_container_interface(attr):
    names = ["data_0", "data_1", "data_2", "data_3"]
    values = range(4)
    
    for name, value in zip(names, values):
        attr[name] = value
        
    # Added from the vector storage call above
    if "partition" in attr:
        names.append("partition")
        values.append(attr["partition"])
    
    assert(attr.list_attributes()==names)
    assert(attr.to_dict() == dict(zip(names, values)))

    for name, value in attr.items():
        assert(name in names)
        assert(value in values)
        assert(names.index(name)==values.index(value))

    for name in attr:
        assert(name in names)

    for value in attr.values():
        assert(value in values)

    assert(attr.keys()==attr.list_attributes())
