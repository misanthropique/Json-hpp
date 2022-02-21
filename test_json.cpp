/**
 * Copyright ©2022. Brent Weichel. All Rights Reserved.
 * Permission to use, copy, modify, and/or distribute this software, in whole
 * or part by any means, without express prior written agreement is prohibited.
 */

/**
 * Install Google Test (Ubuntu):
 *   $ sudo apt-get install libgtest-dev
 *   $ cd /usr/src/gtest
 *   $ sudo cmake -DBUILD_SHARED_LIBS=ON
 *   $ sudo make
 *   $ sudo cp *.so /usr/lib
 *
 * Compile the test:
 *   $ g++ -std=c++14 test_json.cpp -lgtest -lgtest_main -o gtest_json
 */
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <map>
#include <gtest/gtest.h>
#include <vector>

#include "Json.hpp"

using Type = JsonValue::Type;

static const std::string STD_STRING_KEY( "Key - std::string" );
static const char* const CSTRING_KEY = "Key - cstring";

TEST( JsonValueConstructor, DefaultConstructorShouldSetTypeToUndefined )
{
	JsonValue defaultUndefined;

	EXPECT_EQ( Type::undefined, defaultUndefined.type() );
	EXPECT_TRUE( defaultUndefined.is( Type::undefine ) );
}
