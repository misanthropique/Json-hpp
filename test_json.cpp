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

TEST( JsonValueConstructor, DefaultConstructorShouldSetTypeToUndefinedIfNoParameterProvided )
{
	JsonValue defaultUndefined;

	EXPECT_EQ( Type::undefined, defaultUndefined.type() );
	EXPECT_TRUE( defaultUndefined.is( Type::undefined ) );
}

TEST( JsonValueConstructor, DefaultConstructorShouldSetTypeToParameterType )
{
	std::vector< Type > defaultTypes {
		Type::object, Type::array, Type::string, Type::number,
		Type::boolean, Type::null, Type::undefined };

	for ( const auto& type : defaultTypes )
	{
		JsonValue defaultConstructed( type );

		EXPECT_EQ( type, defaultConstructed.type() );
		EXPECT_TRUE( defaultConstructed.is( type ) );
	}
}

TEST( JsonValueConstructor, CopyConstructorShouldCopyTypeInformationOfSource )
{
	JsonValue sourceJsonValue( Type::string );
	JsonValue copyJsonValue( sourceJsonValue );

	EXPECT_EQ( sourceJsonValue.type(), copyJsonValue.type() );
	EXPECT_EQ( sourceJsonValue, copyJsonValue );
}

TEST( JsonValueConstructor, MoveConstructorShouldHaveTheTypeOfSourceJsonValueAndSourceJsonValueShouldHaveTypeUndefined )
{
	JsonValue sourceJsonValue( Type::boolean );
	Type sourceType = sourceJsonValue.type();

	JsonValue moveJsonValue( std::move( sourceJsonValue ) );

	EXPECT_EQ( Type::undefined, sourceJsonValue.type() );
	EXPECT_EQ( sourceType, moveJsonValue.type() );
}

TEST( JsonValueConstructor, ObjectTypeMoveConstructorShouldSetJsonValueToTypeObject )
{
	JsonValue::ObjectType dictionary
	{
		{ "empty_string", Type::string },
		{ "empty_number", Type::number },
		{ "null", Type::null }
	};

	
}
