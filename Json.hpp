/**
 * Copyright ©2021. Brent Weichel. All Rights Reserved.
 * Permission to use, copy, modify, and/or distribute this software, in whole
 * or part by any means, without express prior written agreement is prohibited.
 */
#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <map>
#include <string>
#include <type_traits>
#include <vector>

#ifdef USE_GMP
#include <gmp.h>
#endif

/**
 * Class for representing a JSON value, as defined in the ECMA-404 specification, in C++.
 * Reference: https://www.json.org/json-en.html
 * Note(s):
 *    - Requires C++14.
 *    - The design of this implementation is focused on
 *      have a similar syntactic feel to ECMAScript when manipulating
 *      JSON. The dump(s) and load(s) functions are present for those
 *      that may be coming out of Python.
 *    - This implementation is only safe for single threaded use.
 *      And there is no effort put into protecting the tree.
 */
class JsonValue
{
private:
	/*
	 * Enumeration of number types, which is
	 * used when casting from mValue (std::string) to
	 * an arithmetic type: u?int{8,16,32,64}_t, float, double, long double.
	 * If GMP is present, then we also have mpz_t and mpf_t
	 */
	enum class eNumberType
	{
		FLOATING,
		SIGNED_INTEGRAL,
		UNSIGNED_INTEGRAL,
		NONE
	};

	/*
	 * Useful state information when parsing
	 * and for providing informative error messages.
	 */
	struct sParseFrame
	{
		const char* beginPointer = nullptr;
		const char* currentPointer = nullptr;
		const char* endPointer = nullptr;
	};

public:
	using ObjectType = std::map< std::string, JsonValue >;
	using ArrayType = std::vector< JsonValue >;
	
	enum class Type
	{
		object,
		array,
		string,
		number,
		boolean,
		null,
		undefined
	};

	enum class Indent
	{
		TAB,
		SPACE,
		NONE
	};

	class iterator
	{
		
	};

	JsonValue( Type type = Type::undefined ) noexcept;
	JsonValue( const JsonValue& other );
	JsonValue( JsonValue&& other );
	JsonValue( const ObjectType& object );
	JsonValue( ObjectType&& object );
	JsonValue( const ArrayType& array );
	JsonValue( ArrayType&& array );
	JsonValue( const std::string& string );
	JsonValue( std::string&& string );
	JsonValue( bool boolean );
	template < typename ArithmeticType,
		typename = typename std::enable_if< std::is_arthmetic< ArithmeticType >::value >::type >
	JsonValue( ArithmeticType arithmeticValue );
#if USE_GMP
	JsonValue( mpz_t multiplePrecisionIntegral );
	JsonValue( mpf_t multiplePrecisionFloat );
#endif
	JsonValue( std::nullptr_t );
	~JsonValue();

	iterator begin();

	/**
	 * Clear the contents of this JsonValue instance.
	 * The type of this JsonValue instance shall be undefined after calling this method.
	 */
	void clear() noexcept
	{
		mNumericType = eNumberType::NONE;
		mElements.clear();
		mMembers.clear();
		mValue.clear();
		mType = Type::undefined;
	}

	void dump( FILE* jsonFile, Indent indent = Indent::NONE, size_t indentLevel = 0 ) const;
	void dump( std::ofstream& jsonOFStream, Indent indent = Indent::NONE, size_t indentLevel = 0 ) const;
	void dumps( std::string& jsonString, Indent indent = Indent::NONE, size_t indentLevel = 0 ) const;

	iterator end();

	/**
	 * Check if the given key is present under the constraint that the JsonValue is an object.
	 * @param key Member key to check existance for.
	 * @return True is returned if the key is present. False is returned if either the member
	 *         key is not present or if the JsonValue is not an object.
	 */
	bool hasMember( const std::string& key ) const noexcept
	{
		if ( Type::object == mType )
		{
			return mMembers.end() != mMembers.find( key );
		}

		return false;
	}

	/**
	 * Check if this JsonValue is the same type as the requested.
	 * @param type JsonValue::Type to check the present type against.
	 * @return True is retured if the given and the present JsonValue::Type are the same.
	 */
	bool is( Type type ) const noexcept
	{
		return type == mType;
	}

	/**
	 * Return a vector of keys under the constraint that the JsonValue is an object.
	 * @return Return a vector of keys.
	 * @throw An exception is thrown if the JsonValue is not an object.
	 */
	std::vector< std::string > keys() const
	{
		if ( Type::object == mType )
		{
			std::vector< std::string > memberKeys( mMembers.size() );
			std::transform(
				mMembers.begin(), mMembers.end(),
				std::back_inserter( memberKeys ),
				[]( const auto& keyValuePair )
				{
					return keyValuePair.first;
				}
			);
			return objectKeys;
		}

		throw std::runtime_error( "Operation 'keys()' is not defined for non-object type" );
	}

	void load( FILE* jsonFile );
	void load( std::ifstream& jsonIFStream );

	/**
	 * Parse a JsonValue from the given string an assign to this instance.
	 * @param jsonString A string object containing the JSON to be parsed.
	 * @throw ParseError is thrown if there is a parsing error.
	 */
	void loads( const std::string& jsonString )
	{
		this->parse( jsonString );
	}

	JsonValue& operator=( const JsonValue& other );
	JsonValue& operator=( JsonValue&& other );
	JsonValue& operator=( const ObjectType& object );
	JsonValue& operator=( ObjectType&& object );
	JsonValue& operator=( const ArrayType& array );
	JsonValue& operator=( ArrayType&& array );
	JsonValue& operator=( const std::string& string );
	JsonValue& operator=( std::string&& string );
	template < typename ArithmeticType,
		typename = typename std::enable_if< std::is_arithmetic< ArithmeticType >::value >::type >
	JsonValue& operator=( ArithmeticType arithmeticValue );
#ifdef USE_GMP
	JsonValue& operator=( mpz_t multiplePrecisionIntegral );
	JsonValue& operator=( mpf_t multiplePrecitionFloat );
#endif
	JsonValue& operator=( bool boolean );
	JsonValue& operator=( std::nullptr_t );

	bool operator==( const JsonValue& other ) const noexcept;
	bool operator!=( const JsonValue& other ) const noexcept;

	JsonValue& operator[]( const char* const key );
	JsonValue& operator[]( const std::string& key );
	template < typename IntegralType,
		typename = typename std::enable_if< std::is_integral< IntegralType >::value >::type >
	JsonValue& operator[]( IntegralType index );
	JsonValue& operator[]( const char* const key ) const;
	JsonValue& operator[]( const std::string& key ) const;
	template < typename IntegralType,
		typename = typename std::enable_if< std::is_integral< IntegralType >::value >::type >
	JsonValue& operator[]( IntegralType index ) const;

	operator bool() const;
	operator std::string() const;
	template < typename ArithmeticType,
		typename = typename std::enable_if< std::is_arithmetic< ArithmeticType >::value >::type >
	operator ArithmeticType() const;
	operator ObjectType() const;
	operator ArrayType() const;

	void parse( FILE* jsonFile );
	void parse( std::ifstream& jsonIFStream );

	/**
	 * Parse a JsonValue from the given string an assign to this instance.
	 * @param jsonString A string object containing the JSON to be parsed.
	 * @throw ParseError is thrown if there is a parsing error.
	 */
	void parse( const std::string& jsonString );

	/**
	 * Length of the JsonValue, assuming the type is: object, array, or string.
	 * @return Length of the JsonValue.
	 * @throw An exception is thrown if the JsonValue is not an object, array, or string.
	 */
	size_t size() const
	{
		switch ( mType )
		{
		case Type::object:
			return mMembers.size();

		case Type::array:
			return mElements.size();

		case Type::string:
			return mValue.size();
		}

		throw std::runtime_error( "Operation 'size()' is not defined for type: " + _getTypeString() );
	}

	/**
	 * Generate the string representation of this JsonValue.
	 * By default, the dense representation is generated. If a beautified,
	 * or human readable, version is desired, then the indent can be set to
	 * use tabs or spaces. The indentLevel is by default set to 4 for spaces.
	 * @param indent The character to use for indentation. [default: Indent:NONE]
	 * @param indentationLevel This parameter is only used if {@param indent} is set
	 *                         to Indent::SPACE, in which case it is the number of space
	 *                         characters used for each level of indentation. [default: 4]
	 * @return The string representation of this JsonValue is returned.
	 */
	std::string stringify( Indent indent = Indent::NONE, size_t indentLevel = 4 ) const
	{
		
	}

	/**
	 * Retrieve the type of the JsonValue.
	 * @return Return the type of the JsonValue.
	 */
	Type type() const noexcept
	{
		return mType;
	}

	/**
	 * Get a string representation of the type.
	 * @return Return a string representation of the JsonValue type.
	 */
	const std::string& typeString() const
	{
		return _getTypeString();
	}

private:
	const std::string& _getTypeString() const
	{
		static const std::map< JsonValue::Type, std::string > TYPE_STRING_MAP
		{
			{ Type::object,    "object"    },
			{ Type::array,     "array"     },
			{ Type::string,    "string"    },
			{ Type::number,    "number"    },
			{ Type::boolean,   "boolean"   },
			{ Type::null,      "null"      },
			{ Type::undefined, "undefined" }
		};

		return TYPE_STRING_MAP.at( mType );
	}

	Type mType;
	eNumberType mNumericType;
	std::string mValue;
	ArrayType mElements;
	ObjectType mMembers;
};
