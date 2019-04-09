// This file is part of Asteria.
// Copyleft 2018, LH_Mouse. All wrongs reserved.

#include "_test_init.hpp"
#include "../asteria/src/compiler/simple_source_file.hpp"
#include "../asteria/src/runtime/global_context.hpp"
#include <sstream>

using namespace Asteria;

int main()
  {
    static constexpr char s_source[] =
      R"__(
        assert std.array.slice([0,1,2,3,4], 0) == [0,1,2,3,4];
        assert std.array.slice([0,1,2,3,4], 1) == [1,2,3,4];
        assert std.array.slice([0,1,2,3,4], 2) == [2,3,4];
        assert std.array.slice([0,1,2,3,4], 3) == [3,4];
        assert std.array.slice([0,1,2,3,4], 4) == [4];
        assert std.array.slice([0,1,2,3,4], 5) == [];
        assert std.array.slice([0,1,2,3,4], 6) == [];
        assert std.array.slice([0,1,2,3,4], 0, 3) == [0,1,2];
        assert std.array.slice([0,1,2,3,4], 1, 3) == [1,2,3];
        assert std.array.slice([0,1,2,3,4], 2, 3) == [2,3,4];
        assert std.array.slice([0,1,2,3,4], 3, 3) == [3,4];
        assert std.array.slice([0,1,2,3,4], 4, 3) == [4];
        assert std.array.slice([0,1,2,3,4], 5, 3) == [];
        assert std.array.slice([0,1,2,3,4], 6, 3) == [];
        assert std.array.slice([0,1,2,3,4], std.constants.integer_max) == [];
        assert std.array.slice([0,1,2,3,4], std.constants.integer_max, std.constants.integer_max) == [];

        assert std.array.slice([0,1,2,3,4], -1) == [4];
        assert std.array.slice([0,1,2,3,4], -2) == [3,4];
        assert std.array.slice([0,1,2,3,4], -3) == [2,3,4];
        assert std.array.slice([0,1,2,3,4], -4) == [1,2,3,4];
        assert std.array.slice([0,1,2,3,4], -5) == [0,1,2,3,4];
        assert std.array.slice([0,1,2,3,4], -6) == [0,1,2,3,4];
        assert std.array.slice([0,1,2,3,4], -1, 3) == [4];
        assert std.array.slice([0,1,2,3,4], -2, 3) == [3,4];
        assert std.array.slice([0,1,2,3,4], -3, 3) == [2,3,4];
        assert std.array.slice([0,1,2,3,4], -4, 3) == [1,2,3];
        assert std.array.slice([0,1,2,3,4], -5, 3) == [0,1,2];
        assert std.array.slice([0,1,2,3,4], -6, 3) == [0,1];
        assert std.array.slice([0,1,2,3,4], -7, 3) == [0];
        assert std.array.slice([0,1,2,3,4], -8, 3) == [];
        assert std.array.slice([0,1,2,3,4], -9, 3) == [];
        assert std.array.slice([0,1,2,3,4], std.constants.integer_min) == [0,1,2,3,4];
        assert std.array.slice([0,1,2,3,4], std.constants.integer_min, std.constants.integer_max) == [0,1,2,3];

        assert std.array.replace_slice([0,1,2,3,4], 1, ["a","b","c"]) == [0,"a","b","c"];
        assert std.array.replace_slice([0,1,2,3,4], 1, 2, ["a","b","c"]) == [0,"a","b","c",3,4];
        assert std.array.replace_slice([0,1,2,3,4], 9, ["a","b","c"]) == [0,1,2,3,4,"a","b","c"];

        assert std.array.replace_slice([0,1,2,3,4], -2, ["a","b","c"]) == [0,1,2,"a","b","c"];
        assert std.array.replace_slice([0,1,2,3,4], -2, 1, ["a","b","c"]) == [0,1,2,"a","b","c",4];
        assert std.array.replace_slice([0,1,2,3,4], -9, ["a","b","c"]) == ["a","b","c"];
        assert std.array.replace_slice([0,1,2,3,4], -9, 1, ["a","b","c"]) == ["a","b","c",0,1,2,3,4];

        assert std.array.max_of([ ]) == null;
        assert std.array.max_of([5,null,3,"meow",7,4]) == 7;

        assert std.array.min_of([ ]) == null;
        assert std.array.min_of([5,null,3,"meow",7,4]) == 3;

        assert std.array.find([0,1,2,3,4,3,2,1,0], 2) == 2;
        assert std.array.find([0,1,2,3,4,3,2,1,0], 2, 2) == 2;
        assert std.array.find([0,1,2,3,4,3,2,1,0], 3, 2) == 6;
        assert std.array.find([0,1,2,3,4,3,2,1,0], 7, 2) == null;
        assert std.array.find([0,1,2,3,4,3,2,1,0], 2, 3, 2) == 2;
        assert std.array.find([0,1,2,3,4,3,2,1,0], 3, 3, 2) == null;
        assert std.array.find([0,1,2,3,4,3,2,1,0], 4, 3, 2) == 6;
        assert std.array.find([0,1,2,3,4,3,2,1,0], 2, 5, 2) == 2;

        assert std.array.rfind([0,1,2,3,4,3,2,1,0], 2) == 6;
        assert std.array.rfind([0,1,2,3,4,3,2,1,0], 2, 2) == 6;
        assert std.array.rfind([0,1,2,3,4,3,2,1,0], 3, 2) == 6;
        assert std.array.rfind([0,1,2,3,4,3,2,1,0], 7, 2) == null;
        assert std.array.rfind([0,1,2,3,4,3,2,1,0], 2, 3, 2) == 2;
        assert std.array.rfind([0,1,2,3,4,3,2,1,0], 3, 3, 2) == null;
        assert std.array.rfind([0,1,2,3,4,3,2,1,0], 4, 3, 2) == 6;
        assert std.array.rfind([0,1,2,3,4,3,2,1,0], 2, 5, 2) == 6;

        assert std.array.find_if([0,1,2,3,4,5,6,7,8,9], func(x) = (x % 5 == 3)) == 3;
        assert std.array.find_if([0,1,2,3,4,5,6,7,8,9], 3, func(x) = (x % 5 == 3)) == 3;
        assert std.array.find_if([0,1,2,3,4,5,6,7,8,9], 4, func(x) = (x % 5 == 3)) == 8;
        assert std.array.find_if([0,1,2,3,4,5,6,7,8,9], 9, func(x) = (x % 5 == 3)) == null;
        assert std.array.find_if([0,1,2,3,4,5,6,7,8,9], 3, 4, func(x) = (x % 5 == 3)) == 3;
        assert std.array.find_if([0,1,2,3,4,5,6,7,8,9], 4, 4, func(x) = (x % 5 == 3)) == null;
        assert std.array.find_if([0,1,2,3,4,5,6,7,8,9], 5, 4, func(x) = (x % 5 == 3)) == 8;
        assert std.array.find_if([0,1,2,3,4,5,6,7,8,9], 3, 6, func(x) = (x % 5 == 3)) == 3;

        assert std.array.find_if_not([0,1,2,3,4,5,6,7,8,9], func(x) = (x % 5 != 3)) == 3;
        assert std.array.find_if_not([0,1,2,3,4,5,6,7,8,9], 3, func(x) = (x % 5 != 3)) == 3;
        assert std.array.find_if_not([0,1,2,3,4,5,6,7,8,9], 4, func(x) = (x % 5 != 3)) == 8;
        assert std.array.find_if_not([0,1,2,3,4,5,6,7,8,9], 9, func(x) = (x % 5 != 3)) == null;
        assert std.array.find_if_not([0,1,2,3,4,5,6,7,8,9], 3, 4, func(x) = (x % 5 != 3)) == 3;
        assert std.array.find_if_not([0,1,2,3,4,5,6,7,8,9], 4, 4, func(x) = (x % 5 != 3)) == null;
        assert std.array.find_if_not([0,1,2,3,4,5,6,7,8,9], 5, 4, func(x) = (x % 5 != 3)) == 8;
        assert std.array.find_if_not([0,1,2,3,4,5,6,7,8,9], 3, 6, func(x) = (x % 5 != 3)) == 3;

        assert std.array.rfind_if([0,1,2,3,4,5,6,7,8,9], func(x) = (x % 5 == 3)) == 8;
        assert std.array.rfind_if([0,1,2,3,4,5,6,7,8,9], 3, func(x) = (x % 5 == 3)) == 8;
        assert std.array.rfind_if([0,1,2,3,4,5,6,7,8,9], 4, func(x) = (x % 5 == 3)) == 8;
        assert std.array.rfind_if([0,1,2,3,4,5,6,7,8,9], 9, func(x) = (x % 5 == 3)) == null;
        assert std.array.rfind_if([0,1,2,3,4,5,6,7,8,9], 3, 4, func(x) = (x % 5 == 3)) == 3;
        assert std.array.rfind_if([0,1,2,3,4,5,6,7,8,9], 4, 4, func(x) = (x % 5 == 3)) == null;
        assert std.array.rfind_if([0,1,2,3,4,5,6,7,8,9], 5, 4, func(x) = (x % 5 == 3)) == 8;
        assert std.array.rfind_if([0,1,2,3,4,5,6,7,8,9], 3, 6, func(x) = (x % 5 == 3)) == 8;

        assert std.array.rfind_if_not([0,1,2,3,4,5,6,7,8,9], func(x) = (x % 5 != 3)) == 8;
        assert std.array.rfind_if_not([0,1,2,3,4,5,6,7,8,9], 3, func(x) = (x % 5 != 3)) == 8;
        assert std.array.rfind_if_not([0,1,2,3,4,5,6,7,8,9], 4, func(x) = (x % 5 != 3)) == 8;
        assert std.array.rfind_if_not([0,1,2,3,4,5,6,7,8,9], 9, func(x) = (x % 5 != 3)) == null;
        assert std.array.rfind_if_not([0,1,2,3,4,5,6,7,8,9], 3, 4, func(x) = (x % 5 != 3)) == 3;
        assert std.array.rfind_if_not([0,1,2,3,4,5,6,7,8,9], 4, 4, func(x) = (x % 5 != 3)) == null;
        assert std.array.rfind_if_not([0,1,2,3,4,5,6,7,8,9], 5, 4, func(x) = (x % 5 != 3)) == 8;
        assert std.array.rfind_if_not([0,1,2,3,4,5,6,7,8,9], 3, 6, func(x) = (x % 5 != 3)) == 8;

        assert std.array.is_sorted([]) == true;
        assert std.array.is_sorted([10]) == true;
        assert std.array.is_sorted([10,11,12]) == true;
        assert std.array.is_sorted([20,11,32]) == false;
        assert std.array.is_sorted([20,11,32], func(x, y) = (x % 10 <=> y % 10)) == true;

        assert std.array.binary_search([10,11,12,12,12,12,13,13,14,15,15,15,15,15,16], 14) == 8;
        assert std.array.binary_search([10,11,12,12,12,12,13,13,14,15,15,15,15,15,16], 24) == null;

        assert std.array.lower_bound([10,11,12,12,12,12,13,13,14,15,15,15,15,15,16], 12) ==  2;
        assert std.array.lower_bound([10,11,12,12,12,12,13,13,14,15,15,15,15,15,16], 15) ==  9;
        assert std.array.lower_bound([10,11,12,12,12,12,13,13,14,15,15,15,15,15,16],  0) ==  0;
        assert std.array.lower_bound([10,11,12,12,12,12,13,13,14,15,15,15,15,15,16], 24) == 15;

        assert std.array.upper_bound([10,11,12,12,12,12,13,13,14,15,15,15,15,15,16], 12) ==  6;
        assert std.array.upper_bound([10,11,12,12,12,12,13,13,14,15,15,15,15,15,16], 15) == 14;
        assert std.array.upper_bound([10,11,12,12,12,12,13,13,14,15,15,15,15,15,16],  0) ==  0;
        assert std.array.upper_bound([10,11,12,12,12,12,13,13,14,15,15,15,15,15,16], 24) == 15;

        assert std.array.equal_range([10,11,12,12,12,12,13,13,14,15,15,15,15,15,16], 12) == [ 2, 6];
        assert std.array.equal_range([10,11,12,12,12,12,13,13,14,15,15,15,15,15,16], 15) == [ 9,14];
        assert std.array.equal_range([10,11,12,12,12,12,13,13,14,15,15,15,15,15,16],  0) == [ 0, 0];
        assert std.array.equal_range([10,11,12,12,12,12,13,13,14,15,15,15,15,15,16], 24) == [15,15];

        assert std.array.sort([17,13,14,11,16,18,10,15,12,19]) == [10,11,12,13,14,15,16,17,18,19];
        assert std.array.sort([32,14,11,22,21,34,31,13,23,24,12,33], func(x,y) = (x % 10 <=> y % 10)) == [11,21,31,32,22,12,13,23,33,14,34,24];

        assert std.array.generate(func(x,v) = x + (v ?? 1), 10) == [1,2,4,7,11,16,22,29,37,46];

        assert std.array.shuffle([1,2,3,4,5], 42) == std.array.shuffle([1,2,3,4,5], 42);
      )__";

    std::istringstream iss(s_source);
    Simple_Source_File code(iss, rocket::sref("my_file"));
    Global_Context global;
    code.execute(global, { });
  }