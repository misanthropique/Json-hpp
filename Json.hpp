/**
 * Copyright ©2021. Brent Weichel. All Rights Reserved.
 * Permission to use, copy, modify, and/or distribute this software, in whole
 * or part by any means, without express prior written agreement is prohibited.
 */
#pragma once

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iterator>
#include <map>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>
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
		FLOATING,                     // Cast to a long double before anything else
		SIGNED_INTEGRAL,              // Cast to a int64_t before anything else
		UNSIGNED_INTEGRAL,            // Cast to a uint64_t before anything else
		MULTIPLE_PRECISION_FLOAT,     // Use gmz_t for the number
		MULTIPLE_PRECISION_INTEGRAL,  // Use gmf_t for the number
		NONE                          // We've not resolved the numeric type
	};

	/*
	 * Useful state information when parsing
	 * and for providing informative error messages.
	 */
	// We will need to upgrade this struct to a full subclass
	// to accomidate the changes needed for std::ifstream and FILE
	struct sParseFrame
	{
		const char* beginPointer = nullptr;
		const char* currentPointer = nullptr;
		const char* endPointer = nullptr;

		// Copy from the current position into {destination} for {length} bytes
		void copy( std::string& destination, size_t length )
		{
			destination.clear();

			if ( 0 < length )
			{
				destination.assign( currentPointer, length );
			}
		}

		// Check if we're at the end of the source
		bool endOfStream() const
		{
			return currentPointer != endPointer;
		}

		// Peek at the character {offset} bytes from the current position. 
		char peek( uint32_t offset = 0 ) const
		{
			return currentPointer[ offset ];
		}

		// Compare the given {string} of length {length} with the data starting at {currentPointer}
		// Return true if they are equal, false otherwise.
		bool strncmp( const char* const string, size_t length )
		{
			return 0 == strncmp( currentPointer, string, length );
		}

		// Update the current position by {offset} bytes
		void update( uint32_t offset = 1 )
		{
			if ( 0 < offset )
			{
				currentPointer =
					( ( currentPointer + offset ) <= endPointer )
						? currentPointer + offset : endPointer;
			}
		}
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

	/**
	 * Default constructor.
	 * @param type Type to initialize the JsonValue to. [default: undefined]
	 */
	JsonValue( Type type = Type::undefined ) noexcept
	{
		_initPrimitiveVariables( type );
	}

	/**
	 * Copy constructor.
	 * @param other Const reference to the JsonValue to copy.
	 */
	JsonValue( const JsonValue& other )
	{
		mType = other.mType;
		mStringValue = other.mStringValue;
		mElements = other.mElements;
		mMembers = other.mMembers;
		mBoolean = other.mBoolean;
		mNumericType = other.mNumericType;

		switch ( mNumericType )
		{
		case eNumberType::FLOATING:
		case eNumberType::SIGNED_INTEGRAL:
		case eNumberType::UNSIGNED_INTEGRAL:
			std::memcpy( &mNumericValue, &other.mNumericValue, sizeof( mNumericValue ) );
			break;
#ifdef USE_GMP
		case eNumberType::MULTIPLE_PRECISION_FLOAT:
			mpf_init_set( mMPFloatValue, other.mMPFloatValue );
			break;
		case eNumberType::MULTIPLE_PRECISION_INTEGRAL:
			mpz_init_set( mMPIntegralValue, other.mMPIntegralValue );
			break;
#endif
		default:
			std::memset( &mNumericValue, 0, sizeof( mNumericValue ) );
			break;
		}
	}

	/**
	 * Move constructor.
	 * @param other R-Value of the JsonValue to move to this instance.
	 */
	JsonValue( JsonValue&& other )
	{
		mType = std::exchange( other.mType, Type::undefined );
		mStringValue = std::move( other.mStringValue );
		mElements = std::move( other.mElements );
		mMembers = std::move( other.mMembers );
		mBoolean = std::exchange( other.mBoolean, false );
		mNumericType = std::exchange( other.mNumericType, eNumberType::NONE );
		std::memcpy( &mNumericValue, &other.mNumericValue, sizeof( mNumericValue ) );
		std::memset( &other.mNumericValue, 0, sizeof( mNumericValue ) );
	}

	/**
	 * Object copy initialization constructor.
	 * @param object Const reference to an ObjectType to initialize the JsonValue to.
	 */
	JsonValue( const ObjectType& object )
	{
		_initPrimitiveVariables( Type::object );
		mMembers = object;
	}

	/**
	 * Object move initialization constructor.
	 * @param object R-Value to an ObjectType to initialize the JsonValue to.
	 */
	JsonValue( ObjectType&& object )
	{
		_initPrimitiveVariables( Type::object );
		mMembers = std::move( object );
	}

	/**
	 * Array copy initialization constructor.
	 * @param array Const reference to an ArrayType to initialize the JsonValue to.
	 */
	JsonValue( const ArrayType& array )
	{
		_initPrimitiveVariables( Type::array );
		mElements = array;
	}

	/**
	 * Array move initialization constructor.
	 * @param array R-Value to an ArrayType to initialize the JsonValue to.
	 */
	JsonValue( ArrayType&& array )
	{
		_initPrimitiveVariables( Type::array );
		mElements = std::move( array );
	}

	/**
	 * String copy initialization constructor.
	 * @param string Const reference to a string to initialize the JsonValue to.
	 */
	JsonValue( const std::string& string )
	{
		_initPrimitiveVariables( Type::string );
		mStringValue = string;
	}

	/**
	 * String move initialization constructor.
	 * @param string R-Value to a string to initialize the JsonValue to.
	 */
	JsonValue( std::string&& string )
	{
		_initPrimitiveVariables( Type::string );
		mStringValue = std::move( string );
	}

	/**
	 * Boolean initialization constructor.
	 * @param boolean Boolean value to initialize the JsonValue to.
	 */
	JsonValue( bool boolean )
	{
		_initPrimitiveVariables( Type::boolean );
		mBoolean = boolean;
	}

	/**
	 * Arithmetic number initialization constructor.
	 * @param arithmeticValue An arithmetic number to initialize the JsonValue with.
	 */
	template < typename ArithmeticType,
		typename = typename std::enable_if< std::is_arthmetic< ArithmeticType >::value >::type >
	JsonValue( ArithmeticType arithmeticValue )
	{
		_initPrimitiveVariables( Type::number );

		if ( std::is_floating_point< ArithmeticType >::value )
		{
			mNumericType = eNumberType::FLOATING;
			mNumericValue.floatValue = arithmeticValue;
		}
		else if ( std::is_unsigned< ArithmeticType >::value )
		{
			mNumericType = eNumberType::UNSIGNED_INTEGRAL;
			mNumericValue.unsignedIntegral = arithmeticValue;
		}
		else
		{
			mNumericType = eNumberType::SIGNED_INTEGRAL;
			mNumericValue.signedIntegral = arithmeticValue;
		}
	}

#ifdef USE_GMP
	/**
	 * Multiple precision integral copy initialization constructor.
	 * @param array Const reference to an ArrayType to initialize the JsonValue to.
	 */
	JsonValue( mpz_t multiplePrecisionIntegral )
	{
		_initPrimitiveVariables( Type::number );
		mNumericType = eNumberType::MULTIPLE_PRECISION_INTEGRAL;
		mpz_init_set( mNumericValue.MPIntegralValue, multiplePrecisionIntegral );
	}

	/**
	 * Multiple precision floating copy initialization constructor.
	 * @param array Const reference to an ArrayType to initialize the JsonValue to.
	 */
	JsonValue( mpf_t multiplePrecisionFloat )
	{
		_initPrimitiveVariables( Type::number );
		mNumericType = eNumberType::MULTIPLE_PRECISION_FLOAT;
		mpf_init_set( mNumericValue.MPFloatValue, multiplePrecisionFloat );
	}
#endif

	/**
	 * Null initialization constructor.
	 */
	JsonValue( std::nullptr_t )
	{
		_initPrimitiveVariables( Type::null );
	}

	/**
	 * Destructor.
	 */
	~JsonValue()
	{
		this->clear();
	}

	iterator begin();

	/**
	 * Clear the contents of this JsonValue instance.
	 * The type of this JsonValue instance shall be undefined after calling this method.
	 */
	void clear() noexcept
	{
#ifdef USE_GMP
		switch ( mNumericType )
		{
		case eNumberType::MULTIPLE_PRECISION_FLOAT:
			mpf_clear( mMPFloatValue );
			break;
		case eNumberType::MULTIPLE_PRECISION_INTEGRAL:
			mpz_clear( mMPIntegralValue );
			break;
		}
#endif

		mType = Type::undefined;
		mStringValue.clear();
		mElements.clear();
		mMembers.clear();
		mBoolean = false;
		mNumericType = eNumberType::NONE;
		std::memset( &mNumericValue, 0, sizeof( mNumericValue ) );
	}

	/**
	 * Write the string representation of this JsonValue out to file.
	 * By default, the dense representation is generated. If a beautified,
	 * or human readable, version is desired, then the indent can be set to
	 * use tabs or spaces. The indentLevel is by default set to 4 for spaces.
	 * @param jsonFile Pointer to the FILE handle to write to.
	 * @param indent The character to use for indentation. [default: Indent:NONE]
	 * @param indentationLevel This parameter is only used if {@param indent} is set
	 *                         to Indent::SPACE, in which case it is the number of space
	 *                         characters used for each level of indentation. [default: 4]
	 */
	void dump( FILE* jsonFile, Indent indent = Indent::NONE, size_t indentLevel = 4 ) const
	{
	}

	/**
	 * Write the string representation of this JsonValue out to file.
	 * By default, the dense representation is generated. If a beautified,
	 * or human readable, version is desired, then the indent can be set to
	 * use tabs or spaces. The indentLevel is by default set to 4 for spaces.
	 * @param jsonOFStream Reference to the std::ofstream to write to.
	 * @param indent The character to use for indentation. [default: Indent:NONE]
	 * @param indentationLevel This parameter is only used if {@param indent} is set
	 *                         to Indent::SPACE, in which case it is the number of space
	 *                         characters used for each level of indentation. [default: 4]
	 */
	void dump( std::ofstream& jsonOFStream, Indent indent = Indent::NONE, size_t indentLevel = 4 ) const
	{
	}

	/**
	 * Write the string representation of this JsonValue out to file.
	 * By default, the dense representation is generated. If a beautified,
	 * or human readable, version is desired, then the indent can be set to
	 * use tabs or spaces. The indentLevel is by default set to 4 for spaces.
	 * @param jsonString Reference to the std::string to write to.
	 * @param indent The character to use for indentation. [default: Indent:NONE]
	 * @param indentationLevel This parameter is only used if {@param indent} is set
	 *                         to Indent::SPACE, in which case it is the number of space
	 *                         characters used for each level of indentation. [default: 4]
	 */
	void dumps( std::string& jsonString, Indent indent = Indent::NONE, size_t indentLevel = 4 ) const
	{
		jsonString = this->stringify( indent, indentLevel );
	}

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

	/**
	 * Parse a JsonValue from the given FILE and assign to this instance.
	 * @param jsonFile Pointer to a FILE handle from whence to read the JSON from.
	 * @throw ParseError is thrown if there is a parsing error.
	 */
	void load( FILE* jsonFile )
	{
		this->parse( jsonIFStream );
	}

	/**
	 * Parse a JsonValue from the the given std::ifstream and assign to this instance.
	 * @param jsonIFStream Reference to a std::ifstream from whence to read the JSON from.
	 * @throw ParseError is thrown if there is a parsing error.
	 */
	void load( std::ifstream& jsonIFStream )
	{
		this->parse( jsonIFStream );
	}

	/**
	 * Parse a JsonValue from the given string and assign to this instance.
	 * @param jsonString A string object containing the JSON to be parsed.
	 * @throw ParseError is thrown if there is a parsing error.
	 */
	void loads( const std::string& jsonString )
	{
		this->parse( jsonString );
	}

	/**
	 * Copy assignment operator.
	 * @param other Const reference to the JsonValue to copy.
	 * @return Reference to this JsonValue instance.
	 */
	JsonValue& operator=( const JsonValue& other )
	{
		if ( *this != other )
		{
			this->clear();

			mType = other.mType;
			mStringValue = other.mStringValue;
			mElements = other.mElements;
			mMembers = other.mMembers;
			mBoolean = other.mBoolean;
			mNumericType = other.mNumericType;

			switch ( mNumericType )
			{
			case eNumberType::FLOATING:
			case eNumberType::SIGNED_INTEGRAL:
			case eNumberType::UNSIGNED_INTEGRAL:
				std::memcpy( &mNumericValue, &other.mNumericValue, sizeof( mNumericValue ) );
				break;
#ifdef USE_GMP
			case eNumberType::MULTIPLE_PRECISION_FLOAT:
				mpf_init_set( mMPFloatValue, other.mMPFloatValue );
				break;
			case eNumberType::MULTIPLE_PRECISION_INTEGRAL:
				mpz_init_set( mMPIntegralValue, other.mMPIntegralValue );
				break;
#endif
			default:
				std::memset( &mNumericValue, 0, sizeof( mNumericValue ) );
				break;
			}
		}

		return *this;
	}

	/**
	 * Move assignment operator.
	 * @param other R-Value to the JsonValue to move.
	 * @return Reference to this JsonValue instance.
	 */
	JsonValue& operator=( JsonValue&& other )
	{
		if ( *this != other )
		{
			this->clear();

			mType = std::exchange( other.mType, Type::undefined );
			mStringValue = std::move( other.mStringValue );
			mElements = std::move( other.mElements );
			mMembers = std::move( other.mMembers );
			mBoolean = std::exchange( other.mBoolean, false );
			mNumericType = std::exchange( other.mNumericType, eNumberType::NONE );

			std::memcpy( &mNumericValue, &other.mNumericValue, sizeof( mNumericValue ) );
			std::memset( &other.mNumericValue, 0, sizeof( mNumericValue ) );
		}

		return *this;
	}

	/**
	 * Object copy assignment operator.
	 * @param object Const reference to an object to assign to this JsonValue.
	 * @return Reference to this JsonValue instance is returned.
	 */
	JsonValue& operator=( const ObjectType& object )
	{
		this->clear();
		mType = Type::object;
		mMembers = object;

		return *this;
	}

	/**
	 * Object move assignment operator.
	 * @param object R-Value to an object to assign to this JsonValue.
	 * @return Reference to this JsonValue instance is returned.
	 */
	JsonValue& operator=( ObjectType&& object )
	{
		this->clear();
		mType = Type::object;
		mMembers = std::move( object );

		return *this;
	}

	/**
	 * Array copy assignment operator.
	 * @param array R-Value to an array to assign to this JsonValue.
	 * @return Reference to this JsonValue instance is returned.
	 */
	JsonValue& operator=( const ArrayType& array )
	{
		this->clear();
		mType = Type::array;
		mElements = array;

		return *this;
	}

	/**
	 * Array copy assignment operator.
	 * @param array R-Value to an array to assign to this JsonValue.
	 * @return Reference to this JsonValue instance is returned.
	 */
	JsonValue& operator=( ArrayType&& array )
	{
		this->clear();
		mType = Type::array;
		mElements = std::move( array );

		return *this;
	}

	/**
	 * String copy assignment operator.
	 * @param string Const reference to a string to assign to this JsonValue.
	 * @return Reference to this JsonValue instance is returned.
	 */
	JsonValue& operator=( const std::string& string )
	{
		this->clear();
		mType = Type::string;
		mStringValue = string;

		return *this;
	}

	/**
	 * String move assignment operator.
	 * @param string R-Value to a string to assign to this JsonValue.
	 * @return Reference to this JsonValue instance is returned.
	 */
	JsonValue& operator=( std::string&& string )
	{
		this->clear();
		mType = Type::string;
		mStringValue = std::move( string );

		return *this;
	}

	/**
	 * Numeric assignment operator.
	 * @param arithmeticValue Arithmetic number to assign to this JsonValue.
	 * @return Reference to this JsonValue instance is returned.
	 */
	template < typename ArithmeticType,
		typename = typename std::enable_if< std::is_arithmetic< ArithmeticType >::value >::type >
	JsonValue& operator=( ArithmeticType arithmeticValue )
	{
		this->clear();
		mType = Type::number;
		if ( std::is_floating_point< ArithmeticType >::value )
		{
			mNumericType = eNumberType::FLOATING;
			mNumericValue.floatValue = arithmeticValue;
		}
		else if ( std::is_unsigned< ArithmeticType >::value )
		{
			mNumericType = eNumberType::UNSIGNED_INTEGRAL;
			mNumericValue.unsignedIntegral = arithmeticValue;
		}
		else
		{
			mNumericType = eNumberType::SIGNED_INTEGRAL;
			mNumericValue.signedIntegral = arithmeticValue;
		}

		return *this;
	}

#ifdef USE_GMP
	/**
	 * Multiple precision integral numeric assignment operator.
	 * @param multiplePrecisionIntegral mpz_t object.
	 * @return Return reference to this JsonValue instance.
	 */
	JsonValue& operator=( mpz_t multiplePrecisionIntegral )
	{
		this->clear();
		mType = Type::number;
		mNumericType = eNumberType::MULTIPLE_PRECISION_INTEGRAL;
		mpz_init_set( mNumericValue.MPIntegralValue, multiplePrecisionIntegral );

		return *this;
	}

	/**
	 * Multiple precision float numeric assignment operator.
	 * @param multiplePrecisionFloat mpf_t object.
	 * @return Return reference to this JsonValue instance.
	 */
	JsonValue& operator=( mpf_t multiplePrecitionFloat )
	{
		this->clear();
		mType = Type::number;
		mNumericType = eNumberType::MULTIPLE_PRECISION_FLOAT;
		mpf_init_set( mNumericValue.MPFloatValue, multiplePrecisionFloat );

		return *this;
	}
#endif

	/**
	 * Boolean assignment operator.
	 * @param boolean Boolean value to assign to this JsonValue.
	 * @return Return reference to this JsonValue instance.
	 */
	JsonValue& operator=( bool boolean )
	{
		this->clear();
		mType = Type::boolean;
		mBoolean = boolean;

		return *this;
	}

	/**
	 * Null assignment operator.
	 * @return Return reference to this JsonValue instance.
	 */
	JsonValue& operator=( std::nullptr_t )
	{
		this->clear();
		mType = Type::null;

		return *this;
	}

	/**
	 * Compare equals operator.
	 * @param other Const reference to the JsonValue object to compare against.
	 * @return True is returned if this and other are equal, else false.
	 */
	bool operator==( const JsonValue& other ) const noexcept
	{
		if ( mType != other.mType )
		{
			return false;
		}

		switch ( mType )
		{
		case Type::object:
			return mMembers == other.mMembers;

		case Type::array:
			return mElements == other.mElements;

		case Type::string:
			return mStringValue == other.mStringValue;

		case Type::number:
			switch ( mNumericType )
			{
			case eNumberType::FLOATING:
				return mNumericValue.floatValue == other.mNumericValue.floatValue;

			case eNumberType::SIGNED_INTEGRAL:
				return mNumericValue.signedIntegral == other.mNumericValue.signedIntegral;

			case eNumberType::UNSIGNED_INTEGRAL:
				return mNumericValue.unsignedIntegral == other.mNumericValue.unsignedIntegral;
#ifdef USE_GMP
			case eNumberType::MULTIPLE_PRECISION_FLOAT:
				return mpf_cmp( mNumericValue.MPFloatValue, other.mNumericValue.MPFloatValue );

			case eNumberType::MULTIPLE_PRECISION_INTEGRAL:
				return mpz_cmp( mNumericValue.MPIntegralValue, other.mNumericValue.MPIntegralValue );
#endif
			}

			return false;

		case Type::boolean:
			return mBoolean == other.mBoolean;

		case Type::null:
			return true;
		}

		return false;
	}

	/**
	 * Compare not equal operator.
	 * @param other Const reference to the JsonValue object to compare against.
	 * @return True is returned if this and other are not equal, else false.
	 */
	bool operator!=( const JsonValue& other ) const noexcept
	{
		return not this->operator==( other );
	}

	/**
	 * Member access for object type JsonValue instances.
	 * @param key Pointer to a const char
	 * @return Reference to the member JsonValue.
	 */
	JsonValue& operator[]( const char* const key )
	{
		if ( nullptr == key )
		{
			throw std::invalid_argument( "Key may not be null" );
		}

		return this->operator[]( std::string( key ) );
	}

	/**
	 * Mutable member access for object type JsonValue instances.
	 * If the key is not present, then the member is added.
	 * @param key Const reference to a std::string.
	 * @return Reference to the member JsonValue.
	 */
	JsonValue& operator[]( const std::string& key )
	{
		return mMembers[ key ];
	}

	/**
	 * Mutable element access for array JSON values.
	 * If the index is positive and exceeds the size, then the array is filled
	 * with undefined elements up to the new index. Negative indices may not
	 * exceed the size of the array.
	 * @param index Index into the array.
	 * @return Reference to the element JsonValue.
	 * @throw std::out_of_range is thrown if the index is negative and exceeds the range.
	 */
	template < typename IntegralType,
		typename = typename std::enable_if< std::is_integral< IntegralType >::value >::type >
	JsonValue& operator[]( IntegralType index )
	{
		size_t absoluteIndex;

		// Convert the given index into an absolute offset
		// from the beginning of the elements array.
		if ( std::is_signed< IntegralType >::value )
		{
			if ( index < IntegralType( 0 ) )
			{
				absoluteIndex = size_t( -index );
				if ( mElements.size() < absoluteIndex )
				{
					throw std::out_of_range( "Negative indices may not exceed the length of the array." );
				}

				absoluteIndex = mElments.size() - absoluteIndex;
			}
			else
			{
				absoluteIndex = size_t( index );
			}
		}
		else
		{
			absoluteIndex = size_t( index );
		}

		// Fill in the difference with undefined JSON values.
		if ( absoluteIndex <= mElements.size() )
		{
			mElements.resize( absoluteIndex + 1 );
		}

		return mElements[ absoluteIndex ];
	}

	JsonValue& operator[]( const char* const key ) const;
	JsonValue& operator[]( const std::string& key ) const;

	/**
	 * Immutable element access for array JSON values.
	 * If the index exceeds the bounds 
	 * If the index is positive and exceeds the size, then the array is filled
	 * with undefined elements up to the new index. Negative indices may not
	 * exceed the size of the array.
	 * @param index Index into the array.
	 * @return Reference to the element JsonValue.
	 */
	template < typename IntegralType,
		typename = typename std::enable_if< std::is_integral< IntegralType >::value >::type >
	JsonValue& operator[]( IntegralType index ) const
	{
		size_t absoluteIndex;

		if ( std::is_signed< IntegralType >::value )
		{
			if ( index < IntegralType( 0 ) )
			{
				absoluteIndex = size_t( -index );
				if ( mElements.size() < absoluteIndex )
				{
					throw std::out_of_range( "Negative indices may not exceed the length of the array." );
				}

				absoluteIndex = mElments.size() - absoluteIndex;
			}
			else
			{
				absoluteIndex = size_t( index );
			}
		}
		else
		{
			absoluteIndex = size_t( index );
		}

		return mElements.at( absoluteIndex );
	}

	/**
	 * Cast the value held by the JsonValue object to either true or false.
	 * If the type is object or array, then true is returned. For string type,
	 * true shall be returned if the string is non-zero in length. For number
	 * type, if the value is non-zero, then true is returned else false.
	 * For all other types, false is returned.
	 */
	operator bool() const
	{
		switch ( mType )
		{
			case Type::array:
			case Type::object:
				return true;

			case Type::string:
				return not mStringValue.empty();

			case Type::number:
				switch ( mNumericType )
				{
					case eNumberType::FLOATING:
						return 0.0 != mNumericValue.floatValue;
					case eNumberType::SIGNED_INTEGRAL:
						return 0 != mNumericValue.signedIntegral;
					case eNumberType::UNSIGNED_INTEGRAL:
						return 0 != mNumericValue.unsignedIntegral;
#ifdef USE_GMP
					case eNumberType::MULTIPLE_PRECISION_FLOAT:
						return 0 != mpf_sgn( mNumericValue.MPFloatValue );
					case eNumberType::MULTIPLE_PRECISION_INTEGRAL:
						return 0 != mpz_sgn( mNumericValue.MPIntegralValue );
#endif
					default:
						return false;
				}

				// If we get to here, then the JsonValue
				// is clearly in an invalid state.
				return false;

			case Type::boolean:
				return mBoolean;

			default:
				return false;
		}

		return false;
	}

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
			return mStringValue.size();
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

	void _initPrimitiveVariables( Type type )
	{
		mType = type;
		mBoolean = false;
		mNumericType = eNumberType::NONE;
		std::memset( &mNumericValue, 0, sizeof( mNumericValue ) );
	}

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

	union
	{
		long double floatValue;      // The parsed number is floating.
		intmax_t signedIntegral;     // The parsed number is signed.
		uintmax_t unsignedIntegral;  // The parsed number is unsigned.
#ifdef USE_GMP
		mpz_t MPIntegralValue;      // Store the number as a multi-precision integral.
		mpf_t MPFloatValue;         // Store the number as a multi-precision floating.
#endif
	}
	mNumericValue;             // Anonymous union for holding numeric values.
	Type mType;                // Type of this JsonValue.
	eNumberType mNumericType;  // The best representation of mValue.
	std::string mStringValue;  // Variable for holding string, non-mp numbers.
	ArrayType mElements;       // Array of JsonValues.
	ObjectType mMembers;       // Mapping of JsonValues.
	bool mBoolean;             // Store the boolean value here.

	// Just skip over any whitespace
	void _parseWhitespace( struct sParseFrame& frame )
	{
		const char STRING_WHITESPACE[] = " \r\n\t";
		while ( ( not frame.endOfStream() )
			and ( nullptr != strchr( STRING_WHITESPACE, frame.peek() ) ) )
		{
			frame.update();
		}
	}

	// Parse a string
	void _parseString( struct sParseFrame& frame )
	{
		const char STRING_ESCAPE_CHARACTER[] = "\"\\/bfnrtu";

		if ( '"' != frame.peek() )
		{
			throw ParseError( "parseString", frame );
		}

		frame.update();
		uint32_t stringLength = 0;
		while ( ( not frame.endOfStream() ) and ( '"' != frame.peek( stringLength ) ) )
		{
			if ( '\\' == frame.peek( stringLength ) )
			{
				++stringLength;

				if ( nullptr == strchr( STRING_ESCAPE_CHARACTER, frame.peek( stringLength ) ) )
				{
					throw ParseError( "parseString", frame, stringLength );
				}

				if ( 'u' == frame.peek( stringLength ) )
				{
					if ( isxdigit( frame.peek( stringLength + 1 ) ) and isxdigit( frame.peek( stringLength + 2 ) )
						and isxdigit( frame.peek( stringLength + 3 ) ) and isxdigit( frame.peek( stringLength + 4 ) ) )
					{
						stringLength += 5;
					}
					else
					{
						throw ParseError( "parseString", frame, stringLength );
					}
				}
				else
				{
					++stringLength;
				}
			}
			else if ( ( '\0' <= frame.peek( stringLength ) ) and ( frame.peek( stringLength ) < ' ' ) )
			{
				throw ParseError( "parseString", frame, stringLength );
			}
			else
			{
				++stringLength;
			}
		}

		if ( '"' != frame.peek( stringLength ) )
		{
			throw ParseError( "parseString", frame, stringLength );
		}

		frame.copy( mStringValue, stringLength );
		frame.update( stringLength + 1 );
		mType = Type::string;
	}

	// Parse a number
	void _parseNumber( struct sParseFrame& frame )
	{
		// TODO: Implement
	}

	// Parse an array
	void _parseArray( struct sParseFrame& frame )
	{
		if ( '[' != frame.peek() )
		{
			throw ParseError( "parseArray", frame );
		}

		frame.update();
		_parseWhitespace( frame );
		while ( ( not frame.endOfStream() ) and ( ']' != frame.peek() ) )
		{
			mElements.push_back( JsonValue( Type::undefined, frame ) );

			if ( ',' == frame.peek() )
			{
				frame.update();
			}
			else if ( ']' != frame.peek() )
			{
				throw ParseError( "parseArray", frame );
			}
		}

		if ( ']' != frame.peek() )
		{
			frame.update();
			throw ParseError( "parseArray", frame );
		}
		else
		{
			frame.update();
		}

		mType = Type::array;
	}

	// Parse an object
	void _parseObject( struct sParseFrame& frame )
	{
		if ( '{' != frame.peek() )
		{
			throw ParseError( "parseObject", frame );
		}

		frame.update();
		_parseWhitespace( frame );
		while ( ( not frame.endOfStream() ) and ( '}' != frame.peek() ) )
		{
			JsonValue key( Type::string, frame );

			if ( ':' != frame.peek() )
			{
				throw ParseError( "parseObject", frame );
			}

			frame.update();
			mMembers[ std::string( key ) ] = JsonValue( Type::undefined, frame );

			if ( ',' == frame.peek() )
			{
				frame.update();
			}
			else if ( '}' != frame.peek() )
			{
				throw ParseError( "parseObject", frame );
			}
		}

		if ( '}' != frame.peek() )
		{
			frame.update();
			throw ParseError( "parseObject", frame );
		}
		else
		{
			frame.update();
		}

		mType = Type::object;
	}

	// Root of the parser
	void _parseValue( struct sParseFrame& frame )
	{
		const char STRING_TRUE[] = "true";
		const char STRING_FALSE[] = "false";
		const char STRING_NULL[] = "null";

		_parseWhitespace( frame );

		if ( '"' == frame.peek() )
		{
			_parseString( frame );
		}
		else if ( ( '-' == frame.peek() ) or isdigit( frame.peek() ) )
		{
			_parseNumber( frame );
		}
		else if ( '{' == frame.peek() )
		{
			_parseObject( frame );
		}
		else if ( '[' == frame.peek() )
		{
			_parseArray( frame );
		}
		else if ( frame.strncmp( STRING_TRUE, strlen( STRING_TRUE ) ) )
		{
			// Parse a boolean
			mType = Type::boolean;
			mBoolean = true;
			frame.update( strlen( STRING_TRUE ) );
		}
		else if ( frame.strncmp( STRING_FALSE, strlen( STRING_FALSE ) ) )
		{
			// Parse a boolean
			mType = Type::boolean;
			mBoolean = false;
			frame.update( strlen( STRING_FALSE ) );
		}
		else if ( frame.strncmp( STRING_NULL, strlen( STRING_NULL ) ) )
		{
			// Parse a null
			mType = Type::null;
			frame.update( strlen( STRING_NULL ) );
		}
		else
		{
			throw ParseError( "parseValue", frame );
		}

		_parseWhitespace( frame );
	}
};
