/*
 * This source file is part of the bstring string library.  This code was
 * written by Paul Hsieh in 2002 - 2006, and is covered by the BSD open source
 * license. Refer to the accompanying documentation for details on usage and
 * license.
 */

/*
 * bstrwrap.h
 *
 * This file is the C++ wrapper for the bstring functions.
 */

#ifndef BSTRWRAP_INCLUDE
#define BSTRWRAP_INCLUDE


/////////////////// Configuration defines //////////////////////////////

// By default it is assumed that your compiler can deal with and has enabled
// exception handlling.  If this is not the case then you will need to
// #define BSTRLIB_DOESNT_THROW_EXCEPTIONS
#if !defined(BSTRLIB_THROWS_EXCEPTIONS) && !defined(BSTRLIB_DOESNT_THROW_EXCEPTIONS)
#define BSTRLIB_THROWS_EXCEPTIONS
#endif

////////////////////////////////////////////////////////////////////////
/** @cond */
#include "BString/bstrlib.h"
#include "Utils/StaticAssert.hpp"
#include "Platform/Platform.hpp"
/** @endcond */

#ifdef __cplusplus


// This is required for disabling exception handling
#if (DEBUG==1)
#define bstringThrow(er) do{ fprintf(stderr, "%s", er); Platform::breakUnderDebugger(); } while(0)
#else
#define bstringThrow(er) do{ fprintf(stderr, "[FATAL ERROR] %s:%d => %s", __FILE__, __LINE__, er); } while(0)
#endif

// Assignment operator not obvious, conditional expression constant
#ifdef _MSC_VER
#pragma warning(disable:4512 4127)
#endif

#if WantFloatParsing != 1
extern "C" int ftoa(float v, char buf[15], int);
extern "C" int lftoa(double v, char buf[20], int);
extern "C" char* utoa(unsigned long value, char* result, int base);
#endif

// Forward declare this class so it's easy to convert
namespace Strings { class VerySimpleReadOnlyString; }

/** The better string lib namespace. */
namespace Bstrlib
{
    // Forward declaration
    struct  String;

    /** Provide write protected char functionality */
    class  CharWriteProtected
    {
    	// Members
    private:
    	/** Required to access the String class */
    	friend struct String;
    	/** The reference to the working string */
    	const struct tagbstring& s;
    	/** The index in itself */
    	const unsigned int idx;

    	/** Constructor is private and only used in the String class */
    	CharWriteProtected(const struct tagbstring& c, int i) : s(c), idx((unsigned int)i)
    		{	if (idx >=(unsigned) s.slen) bstringThrow("character index out of bounds");	}

    	// Operators
    public:
    	/** Assignment operator */
    	inline char operator=(char c)
    	{
    		if (s.mlen <= 0)
    			bstringThrow("Write protection error");
    		else
    		{
#ifndef BSTRLIB_THROWS_EXCEPTIONS
    			if (idx >=(unsigned) s.slen)
    				return '\0';
#endif
    			s.data[idx] = (unsigned char) c;
    		}
    		return (char) s.data[idx];
    	}

    	/** Assignment operator */
    	inline unsigned char operator=(unsigned char c)
    	{
    		if (s.mlen <= 0)
    			bstringThrow("Write protection error");
    		else
    		{
#ifndef BSTRLIB_THROWS_EXCEPTIONS
    			if (idx >=(unsigned) s.slen)
    				return '\0';
#endif
    			s.data[idx] = c;
    		}
    		return s.data[idx];
    	}
    	/** Access operator */
    	inline operator unsigned char() const
    	{
#ifndef BSTRLIB_THROWS_EXCEPTIONS
    		if (idx >=(unsigned) s.slen)
    			return (unsigned char) '\0';
#endif
    		return s.data[idx];
    	}
    };

    /** The string class.

        The functions in this string class are very straightforward to use. */
    struct String : public tagbstring
    {
    // Construction and destruction
    //public:
    	/** Default constructor - Allocate 8 bytes of memory */
    	String();
    	/** Char and unsigned char constructor */
    	String(char c);
    	String(unsigned char c);
    	String(char c, int len);
    	/** C-Style string constructor */
    	String(const char *s);
    	/** C-Style string with len limitation.
    	    Use this code to force an allocation that's len bytes:
    		@code
    		    String ret(345, ""); // Allocated length for ret is 345 bytes
    		@endcode
            @warning len is the minimum size, s might be larger */
    	String(int len, const char *s);

    	/** Copy constructors */
    	String(const String& b);
    	String(const tagbstring& x);
    	/** Bulk construction from a given block.
            The block is considered opaque and is not used.
            The block is copyied so it's safe to reuse it in your code later on.
            If you don't need to reuse the block, use the transitiveConstruct instead, as it avoid a memory allocation and copy */
    	String(const void * blk, int len);
        /** Transitive constructor.
            This is taking ownership of the opaque block passed in.
            This avoids a memory allocation and copying, but the block must be allocated by malloc or realloc. */
        String(const void * ownedBlock, const int len, const int allocLen);
        /** Create from a very simple read-only string.
            A copy of the string content is made */
        String(const Strings::VerySimpleReadOnlyString & b);

    	/** Destructor */
    	~String();

    // Operators
    //public:

    	/** = operator */
    	const String& operator=(char c);
    	const String& operator=(unsigned char c);
    	const String& operator=(const char *s);
    	const String& operator=(const String& b);
    	const String& operator=(const tagbstring& x);

    	/** += operator */
    	const String& operator +=(char c);
    	const String& operator +=(unsigned char c);
    	const String& operator +=(const char *s);
    	const String& operator +=(const String& b);
    	const String& operator +=(const tagbstring& x);
        const String& operator +=(const int val);
        const String& operator +=(const unsigned int val);
        const String& operator +=(const int64 val);
        const String& operator +=(const uint64 val);
        const String& operator +=(const float val);
        const String& operator +=(const double val);

    	/** *= operator, this one simply repeats the string count times (0 if negative) */
    	inline const String& operator *=(int count)
    	{
    		this->Repeat(count);
    		return *this;
    	}

    	/** + operator */
    	const String operator+(char c) const;
    	const String operator+(unsigned char c) const;
    	const String operator+(const unsigned char *s) const;
    	const String operator+(const char *s) const;
    	const String operator+(const String& b) const;
    	const String operator+(const tagbstring& x) const;
    	const String operator+(const int c) const;
    	const String operator+(const unsigned int c) const;
    	const String operator+(const int64 c) const;
    	const String operator+(const uint64 c) const;
    	const String operator+(const float c) const;
    	const String operator+(const double c) const;


    	/** * operator, repeat and copy
            @sa Repeat
            @param count    Repeat this string this given amount of times, 0 if negative */
    	inline const String operator * (int count) const
    	{
    		String retval(*this);
    		retval.Repeat(count);
    		return retval;
    	}

    	/** Comparison operators */
    	bool operator ==(const String& b) const;
    	bool operator ==(const char * s) const;
    	bool operator ==(const unsigned char * s) const;
    	bool operator ==(char s[]) const { return this->operator == ((const char*)s); }
    	bool operator !=(const String& b) const;
    	bool operator !=(const char * s) const;
    	bool operator !=(const unsigned char * s) const;
    	bool operator !=(char s[]) const { return this->operator != ((const char*)s); }
    	bool operator <(const String& b) const;
    	bool operator <(const char * s) const;
    	bool operator <(const unsigned char * s) const;
    	bool operator <=(const String& b) const;
    	bool operator <=(const char * s) const;
    	bool operator <=(const unsigned char * s) const;
    	bool operator >(const String& b) const;
    	bool operator >(const char * s) const;
    	bool operator >(const unsigned char * s) const;
    	bool operator >=(const String& b) const;
    	bool operator >=(const char * s) const;
    	bool operator >=(const unsigned char * s) const;

    	inline bool operator !() const { return slen == 0; }
    	inline operator bool() const { return slen > 0; }

    // Casting
    //public:
    	/** To char * and unsigned char * */
    	inline operator const char* () const            { return (const char *)data; }
    	inline operator const unsigned char* () const   { return (const unsigned char *)data; }
        inline const char * getData() const             { return (const char *)data; }

#if WantFloatParsing == 1
    	/** To double, as long as the string contains a double value (0 on error)
    		@warning if you have disabled float parsing, you must provide a double atof (const char*) function */
    	operator double() const;
    	/** To float, as long as the string contains a float value (0 on error)
    		@warning if you have disabled float parsing, you must provide a double atof (const char*) function */
    	operator float() const;

        /** Serialize a float to this string with the given precision after the decimal point
    		@warning if you have disabled float parsing, you must provide a int ftoa(float, char out[15], int precision) function */
    	void storeFloat(float v, int precision) { Format(String::Print("%%.%df", precision), v); }
        /** Serialize a double to this string with the given precision after the decimal point
    		@warning if you have disabled float parsing, you must provide a int ftoa(float, char out[15], int precision) function */
    	void storeDouble(double v, int precision) { Format(String::Print("%%.%dlg", precision), v); }

        /** Get the double stored in this string
            @param consumed If provided, will be filled with the number of consumed characters.
            @return The largest possible double number that's parseable. */
        double parseDouble(int * consumed = 0) const;
#define HasFloatParsing 1
#else
    	/** To double, as long as the string contains a double value (0 on error)
    		@warning if you have disabled float parsing, you must provide a double atof (const char*) function */
    	operator double() const { return atof((const char*)data); }
    	/** To float, as long as the string contains a float value (0 on error)
    		@warning if you have disabled float parsing, you must provide a double atof (const char*) function */
    	operator float() const { return (float)atof((const char*)data); }

        /** Serialize a float to this string with the given precision after the decimal point
    		@warning if you have disabled float parsing, you must provide a int ftoa(float, char out[15], int precision) function */
    	void storeFloat(float v, int precision) { char Buf[15]; ftoa(v, Buf, precision); *this = Buf; }
        /** Serialize a double to this string with the given precision after the decimal point
    		@warning if you have disabled float parsing, you must provide a int dtoa(double, char out[20], int precision) function */
    	void storeDouble(double v, int precision) { char Buf[20]; lftoa(v, Buf, precision); *this = Buf; }

        /** Get the double stored in this string
            @param consumed If provided, will be filled with the number of consumed characters.
            @return The largest possible double number that's parseable. */
        double parseDouble(int * consumed = 0) const
        {
            // This uglyness is required if your platform does not provide a strtod
            if (consumed) *consumed = getLength() - Trimmed("-").splitWhenNoMore("0123456789").Trimmed(".").splitWhenNoMore("0123456789").Trimmed("eE").Trimmed("-").splitWhenNoMore("0123456789").getLength();
            return atof((const char*)data);
        }
#endif
    	/** To int, as long as the string contains a int value in base 10 (0 on error) */
    	operator signed int() const;
    	/** To unsigned int, as long as the string contains a int value in base 10 (0 on error) */
    	operator unsigned int() const;
        /** To int64, as long as the string contains a int64 value in base 10 (0 on error) */
        operator int64() const;

        /** Handy function to get the hexadecimal representation of a number
            @return a lowercase hexadecimal version, with "0x" prefix for the given number */
        static String getHexOf(const uint64 c);

        /** Get the hexadecimal encoded integer in string.
            Both leading 0x and no leading 0x are supported, and both uppercase and lowercase are supported.
            @warning This method is a convenience wrapper around Scan function. It's not optimized for speed.
                     If you intend to parse a integer in unknown format (decimal, hexadecimal, octal, binary) use parseInt() instead
            @sa parseInt */
        inline uint32 fromHex() const { uint32 ret = 0; dropUpTo("0x").asUppercase().Scan("%08X", &ret); return ret; }

        /** Get the integer out of this string.
            This method support any usual encoding of the integer, and detect the integer format automatically.
            This method is optimized for speed, and does no memory allocation on heap
            Supported formats examples: "0x1234, 0700, -1234, 0b00010101"
            @param base     If provided, only the given base is supported (default to 0 for auto-detection).
            @param consumed If provided, will be filled with the number of consumed characters.
            @return The largest possible integer that's parseable. */
        int64 parseInt(const int base = 0, int * consumed = 0) const;

    // Accessors
    //public:
    	/** Return the length of the string */
    	inline int getLength() const {return slen;}
    	/** Bound checking character retrieval */
    	inline unsigned char Character(int i) const
    	{
    		if (((unsigned) i) >=(unsigned) slen)
    		{
#ifdef BSTRLIB_THROWS_EXCEPTIONS
    			bstringThrow("character idx out of bounds");
#else
    			return '\0';
#endif
    		}
    		return data[i];
    	}
    	/** Bound checking character retrieval */
    	inline unsigned char operator[](int i) const { return Character(i); }
    	/** Character retrieval when write protected */
    	inline CharWriteProtected Character(int i) { return CharWriteProtected(*this, i); }
    	inline CharWriteProtected operator[](int i) { return Character(i); }

    	/** Space allocation.
                    This allocate the given length, and return the allocated buffer.
                    You must call releaseLock() once you know the final string's size in byte.
                    @param length   The buffer size in bytes
                    @return A pointer on a new allocated buffer of length bytes, or 0 on error. Use releaseLock() once you're done with the buffer. */
    	char * Alloc(int length);
    	// Unlocking mechanism for char * like access
    	/** Release a lock acquired with Alloc method */
    	inline void releaseLock(const int & len ) { slen = len; mlen = slen + 1; }

    	// Search methods.
    	/** Check if the argument is equal in a case insensitive compare */
    	bool caselessEqual(const String& b) const;
    	/** Case insensitive compare */
    	int caselessCmp(const String& b) const;
    	/** Find a given substring starting at position pos */
    	int Find(const String& b, int pos = 0) const;
    	/** Find a given substring starting at position pos */
    	int Find(const char * b, int pos = 0) const;
    	/** Find a given substring starting at position pos in a case insensitive search */
    	int caselessFind(const String& b, int pos = 0) const;
    	/** Find a given substring starting at position pos in a case insensitive search */
    	int caselessFind(const char * b, int pos = 0) const;
    	/** Find a given char starting at position pos */
    	int Find(char c, int pos = 0) const;
    	/** Reverse find a given substring starting at position pos */
    	int reverseFind(const String& b, int pos) const;
    	/** Reverse find a given substring starting at position pos */
    	int reverseFind(const char * b, int pos) const;
    	/** Reverse find a given substring starting at position pos in a case insensitive search */
    	int caselessReverseFind(const String& b, int pos) const;
    	/** Reverse find a given substring starting at position pos in a case insensitive search */
    	int caselessReverseFind(const char * b, int pos) const;
    	/** Reverse find a given char starting at position pos */
    	int reverseFind(char c, int pos = -1) const;
    	/** Find the first occurrence matching any char of the given substring starting at position pos */
    	int findAnyChar(const String& b, int pos = 0) const;
    	/** Find the first occurrence matching any char of the given substring starting at position pos */
    	int findAnyChar(const char * s, int pos = 0) const;
    	/** Reverse find the first occurrence matching any char of the given substring starting at position pos */
    	int reverseFindAnyChar(const String& b, int pos) const;
    	/** Reverse find the first occurrence matching any char of the given substring starting at position pos */
    	int reverseFindAnyChar(const char * s, int pos) const;
    	/** Find the first occurrence not matching any char of the given substring starting at position pos */
    	int invFindAnyChar(const String& b, int pos = 0) const;
    	/** Find the first occurrence not matching any char of the given substring starting at position pos */
    	int invFindAnyChar(const char * b, int pos = 0) const;
    	/** Reverse find the first occurrence not matching any char of the given substring starting at position pos */
    	int invReverseFindAnyChar(const String& b, int pos) const;
    	/** Reverse find the first occurrence not matching any char of the given substring starting at position pos */
    	int invReverseFindAnyChar(const char * b, int pos) const;
    	/** Count the number of times the given substring appears in the string */
    	int Count(const String & needle) const;
        /** Extract tokens from by splitting the string with the given char, starting at position pos (first call should set pos == 0)
            While called in loop, it extracts the token one by one, until either the returned value is empty, or pos > getLength() */
        String extractToken(char c, int & pos) const;


#if (WantRegularExpressions == 1)
    	/** @name Regular expressions.
                  Everything concerning regular expressions
            @{*/
        // Regular expressions
        enum RegExError
        {
            NotFound               = -1,
            SyntaxError            = -2,
            UnexpectedQuantifier   = -3,
            UnbalancedBrackets     = -4,
            BadSetOfCharacter      = -5,
            BadMetaCharacter       = -6,
            CapturesArrayTooSmall  = -7,
            NotEnoughMemory        = -8,
        };


        /** Match this string against a regular expression and extract any captured element.
            The common regular expression syntax's term are supported but there is no extension.
            The supported syntax is:
            @verbatim
               .        Match any character
               ^        Match beginning of a buffer
               $        Match end of a buffer
               ()       Grouping and substring capturing
               [...]    Match any character from set, ranges (ex [a-z]) are supported
               [^...]   Match any character but ones from set, ranges (ex [a-z]) are supported
               \s       Match whitespace
               \S       Match non-whitespace
               \d       Match decimal digit
               \r       Match carriage return
               \n       Match newline
               +        Match one or more times (greedy)
               +?       Match one or more times (non-greedy)
               *        Match zero or more times (greedy)
               *?       Match zero or more times (non-greedy)
               ?        Match zero or once
               \xDD     Match byte with hex value 0xDD
               \meta    Match one of the meta character: ^$().[*+\?
               (?i)     If at the beginning of the regex, it makes match case-insensitive (override caseSensitive argument)
            @endverbatim

            @param regEx         The regular expression to match against. @sa RE macro to avoid double escape
                                 of the backlash in some regular expression.
            @param captures      If provided will be filled with the captures.
                                 The size of the array must match the regular expression capture's amount.
                                 No allocation is done in this method.
            @param capturesCount On input, contains the size of the captures array, on output will be filled
                                 with the number of captures required. You can call with captures set to 0 in
                                 order to query the number of captures required.
            @param caseSensitive If set, the search is case sensitive
            @param matchPos      If set, will be filled by the first position found where the regular expression match

            @warning First capture does not match the whole string unlike some other engine to avoid useless
                     memory allocation and copying.
                     In the regExReplace method, \0 does represent the whole string.
            @return The last position the expression matched, or a negative value on error (check RegExError for details) */
        int regExMatch(const String & regEx, String * captures, int & capturesCount, const bool caseSensitive = true, int * matchPos = 0) const;

        /** Simpler version of regExMatch that performs no capture.
            @param regEx         The regular expression to match against. @sa RE macro to avoid double escape
                                 of the backlash in some regular expression.
            @param caseSensitive If set, the search is case sensitive.
            @param capturesOpt   If provided and you need to search the same regular expression numerous time, then it's allocated to store the captures internally.
                                 In that case, you need to free the returned pointer when not used anymore.
            @param capturesCount If provided and you need to search the same regular expression numerous time, then it's set to the number of captures to use (no allocation is made)

            @return True if the string matches the given regular expression (partial match will return true too). */
        bool regExFit(const String & regEx, const bool caseSensitive = true, void ** capturesOpt = 0, int * capturesCount = 0) const;


        /** Return a string with value replaced as regular expression.
            @param regEx            The regular expression to match against. @sa RE macro to avoid double escape
                                    of the backlash in some regular expression.
            @param replaceExp       The replacing pattern. @sa RE macro to avoid double escape of backlashes.
            @param caseSensitive    If set, the search is case sensitive
            @param iterations       The number of iterations to run, -1 for until it fails matching, default to one.
            @return The replaced String or empty string on failure.
            The replace pattern follow this scheme:
            @verbatim
                \0      The complete input string
                \n      The n-th matched group with capturing (typically \1, \2, etc...)
            @endverbatim
            The replacement only proceed to the position where the regular expression matches, so typically:
            @code
                String("aa1234 xy (3)").regExReplace("(\\d+)\\s*([x-y ]*)", "678\2") == "aa678xy (3)"
            @endcode */
        String regExReplace(const String & regEx, const String & replaceExp, const bool caseSensitive = true, int iterations = 1) const;
        /** @}*/
#endif

    	/** @name Extraction methods.
                  Extract, split, find, process in a fluent interface.
                  This makes the code much easier to read, write and maintain, since the intend is clear, instead of
                  dealing with indexes, midString and so forth
            @{*/
    	/** Extract the string starting at left of len char long.
    	    You can use negative left value to start from right, or on the length too.
    	    For example, the following code returns:
    	    @code
                String ret = String("abcdefgh").midString(0, 3); // ret = "abc"
                String ret = String("abcdefgh").midString(0, 30); // ret = "abcdefgh"
                String ret = String("abcdefgh").midString(10, 1); // ret = ""
                String ret = String("abcdefgh").midString(-1, 0); // ret = ""
                String ret = String("abcdefgh").midString(-3, 3); // ret = "fgh"
                String ret = String("abcdefgh").midString(-3, 2); // ret = "fg"
                String ret = String("abcdefgh").midString(0, -3); // ret = "abcde"
                String ret = String("abcdefgh").midString(1, -3); // ret = "bcde"
                String ret = String("abcdefgh").midString([whatever], -30); // ret = "abcdefgh"   as negative length takes precedence
    	    @endcode
    	    @param left     The index to start with (0 based)
    	    @param len      The number of bytes to return */
    	const String midString(int left, int len) const;

    	/** Get the string up to the first occurrence of the given string
    	    If not found, it returns the whole string.
    	    For example, this code returns:
    	    @code
    	        String ret = String("abcdefdef").upToFirst("d"); // ret == "abc"
    	        String ret = String("abcdefdef").upToFirst("g"); // ret == "abcdefdef"
    	    @endcode
    	    @param find         The text to look for
    	    @param includeFind  If set, the needle is included in the result */
    	const String upToFirst(const String & find, const bool includeFind = false) const;
    	/** Get the string up to the last occurrence of the given string
    	    If not found, it returns the whole string.
    	    For example, this code returns:
    	    @code
    	        String ret = String("abcdefdef").upToLast("d"); // ret == "abcdef"
    	        String ret = String("abcdefdef").upToLast("g"); // ret == "abcdefdef"
    	    @endcode
    	    @param find         The text to look for
    	    @param includeFind  If set, the needle is included in the result */
    	const String upToLast(const String & find, const bool includeFind = false) const;
    	/** Get the string from the last occurrence of the given string.
    	    If not found, it returns an empty string if includeFind is false, or the whole string if true
    	    For example, this code returns:
    	    @code
    	        String ret = String("abcdefdef").fromLast("d"); // ret == "ef"
    	        String ret = String("abcdefdef").fromLast("d", true); // ret == "def"
    	        String ret = String("abcdefdef").fromLast("g"); // ret == ""
    	    @endcode
    	    @param find         The text to look for
    	    @param includeFind  If set, the needle is included in the result */
    	const String fromLast(const String & find, const bool includeFind = false) const;
    	/** Get the string from the first occurrence of the given string
    	    If not found, it returns an empty string if includeFind is false, or the whole string if true
    	    For example, this code returns:
    	    @code
    	        String ret = String("abcdefdef").fromFirst("d"); // ret == "efdef"
    	        String ret = String("abcdefdef").fromFirst("d", true); // ret == "defdef"
    	        String ret = String("abcdefdef").fromFirst("g"); // ret == ""
    	    @endcode
    	    @param find         The text to look for
    	    @param includeFind  If set, the needle is included in the result */
    	const String fromFirst(const String & find, const bool includeFind = false) const;
    	/** Split a string when the needle is found first, returning the part before the needle, and
    	    updating the string to start on or after the needle.
    	    If the needle is not found, it returns an empty string if includeFind is false, or the whole string if true.
    	    For example this code returns:
    	    @code
    	        String text = "abcdefdef";
    	        String ret = text.splitFrom("d"); // ret = "abc", text = "efdef"
    	        ret = text.splitFrom("f", true);  // ret = "e", text = "fdef"
    	    @endcode
    	    @param find         The string to look for
    	    @param includeFind  If true the string is updated to start on the find text. */
    	const String splitFrom(const String & find, const bool includeFind = false);
    	/** Get the substring from the given needle up to the given needle.
    	    For example, this code returns:
    	    @code
    	        String text = "abcdefdef";
    	        String ret = text.fromTo("d", "f"); // ret = "e"
    	        ret = text.fromTo("d", "f", true);  // ret = "def"
    	        ret = text.fromTo("d", "g"); // ret = ""
    	        ret = text.fromTo("d", "g", true); // ret = "defdef"
    	        ret = text.fromTo("g", "f", [true or false]); // ret = ""
    	    @endcode

    	    @param from         The first needle to look for
    	    @param to           The second needle to look for
            @param includeFind  If true the string includes the from and to needles (if found).
    	    @return If "from" needle is not found, it returns an empty string, else if "to" needle is not found,
    	            it returns an empty string upon includeFind being false, or the string starting from "from" if true. */
    	const String fromTo(const String & from, const String & to, const bool includeFind = false) const;
    	/** Get the substring from the given needle if found, or the whole string if not.
    	    For example, this code returns:
    	    @code
    	        String text = "abcdefdef";
    	        String ret = text.dropUpTo("d"); // ret = "efdef"
    	        ret = text.dropUpTo("d", true); // ret = "defdef"
    	        ret = text.dropUpTo("g", [true or false]); // ret = "abcdefdef"
    	    @endcode
    	    @param find         The string to look for
    	    @param includeFind  If true the string is updated to start on the find text. */
        const String dropUpTo(const String & find, const bool includeFind = false) const;
    	/** Get the substring up to the given needle if found, or the whole string if not, and split from here.
    	    For example, this code returns:
    	    @code
    	        String text = "abcdefdef";
    	        String ret = text.splitUpTo("d"); // ret = "abc", text = "efdef"
    	        ret = text.splitUpTo("g", [true or false]); // ret = "efdef", text = ""
                text = "abcdefdef";
    	        ret = text.splitUpTo("d", true); // ret = "abcd", text = "efdef"
    	    @endcode
    	    @param find         The string to look for
    	    @param includeFind  If true the string is updated to start on the find text. */
        const String splitUpTo(const String & find, const bool includeFind = false);

        /** Split the string at the given position.
            The string is modified to start at the split position.
            @return the string data up to split point. */
        const String splitAt(const int pos);
        /** Eat the characters until the text is no more in the given set.
            The string is split at this position.
            @return The string made only from characters from the given set.
            @sa letterSet, digitSet
            For example, this code returns:
            @code
                String text = "_abs123 defgh";
                String ret = text.splitWhenNoMore("abcdefghijklmnopqrstuvwxyz_0123456789"); // text = " defgh", ret = "_abs123"
            @endcode */
        const String splitWhenNoMore(const String & set);
        /** @}*/


    	// Standard manipulation methods.
    	/** Set the substring at pos to the string b. If pos > len, (pos-len) 'fill' char are inserted */
    	void setSubstring(int pos, const String& b, unsigned char fill = ' ');
    	/** Set the substring at pos to the string b. If pos > len, (pos-len) 'fill' char are inserted */
    	void setSubstring(int pos, const char * b, unsigned char fill = ' ');
    	/** Insert at pos the string b. If pos > len, (pos-len) 'fill' char are inserted */
    	void Insert(int pos, const String& b, unsigned char fill = ' ');
    	/** Insert at pos the string b. If pos > len, (pos-len) 'fill' char are inserted */
    	void Insert(int pos, const char * b, unsigned char fill = ' ');
    	/** Insert at pos len times the 'fill' char */
    	void insertChars(int pos, int len, unsigned char fill = ' ');
    	/** Replace, at pos, len char to the string b.
    		If pos > string length or pos+len > string length, the missing 'fill' char are inserted  */
    	void Replace(int pos, int len, const String& b, unsigned char fill = ' ');
    	/** Replace, at pos, len char to the string b.
    		If pos > string length or pos+len > string length, the missing 'fill' char are inserted  */
    	void Replace(int pos, int len, const char * s, unsigned char fill = ' ');
    	/** Remove, at pos, len char from the string */
    	void Remove(int pos, int len);
    	/** Truncate the string at the given len */
    	void Truncate(int len);

    	// Miscellaneous methods.
    	/** Scan the string and extract to the given data (fmt follows scanf's format spec)
    	    @warning Only one parameter can be extracted by this method.
    	    @return the number of parameter extracted (1 or 0 on error) */
    	int Scan(const char * fmt, void * data) const;

    	/** Printf like format for ascii */
    	void Formata(const char * fmt, ...)
#ifdef __GNUC__
        __attribute__ ((format (printf, 2, 3)))
#endif
        ;
    	/** Fill the string with 'fill' char length times */
    	void Fill(int length, unsigned char fill = ' ');

    	/** Repeat the same string count times.
            @param count   Repeat the pattern this number of times. If negative, it's not repeated. */
    	void Repeat(int count);
    	/** Trim the left side of this string with the given chars */
    	void leftTrim(const String& b = String(" \t\v\f\r\n", sizeof(" \t\v\f\r\n")));
    	/** Trim the right side of this string with the given chars */
    	void rightTrim(const String& b = String(" \t\v\f\r\n", sizeof(" \t\v\f\r\n")));
    	/** Trim the both sides of this string with the given chars */
    	inline void Trim(const String& b = String(" \t\v\f\r\n", sizeof(" \t\v\f\r\n")))
    	{
            if (!b.slen) return;
            rightTrim(b);
            leftTrim(b);
    	}

    	/** Change to uppercase */
    	void toUppercase();
    	/** Change to lowercase */
    	void toLowercase();

    	// Write protection methods.
    	/** Write protect this string */
    	void writeProtect();
    	/** Allow writing to this string */
    	void writeAllow();
    	/** Is the current string write protected ? */
    	inline bool isWriteProtected() const { return mlen <= 0; }

        /** @name Fluent interface.
                  The methods in this section returns reference on a string object (either mutated or new).
                  This makes the code much easier to read and write, it's more consise yet it gives the developer intend.
            @{*/
    	// Search and substitute methods.
    	/** Find the given substring starting at position pos and replace it */
    	String & findAndReplace(const String& find, const String& repl, int pos = 0);
    	/** Find the given substring starting at position pos and replace it */
    	String & findAndReplace(const String& find, const char * repl, int pos = 0);
    	/** Find the given substring starting at position pos and replace it */
    	String & findAndReplace(const char * find, const String& repl, int pos = 0);
    	/** Find the given substring starting at position pos and replace it */
    	String & findAndReplace(const char * find, const char * repl, int pos = 0);
    	/** Find the given substring starting at position pos and replace it with caseless search */
    	String & findAndReplaceCaseless(const String& find, const String& repl, int pos = 0);
    	/** Find the given substring starting at position pos and replace it with caseless search */
    	String & findAndReplaceCaseless(const String& find, const char * repl, int pos = 0);
    	/** Find the given substring starting at position pos and replace it with caseless search */
    	String & findAndReplaceCaseless(const char * find, const String& repl, int pos = 0);
    	/** Find the given substring starting at position pos and replace it with caseless search */
    	String & findAndReplaceCaseless(const char * find, const char * repl, int pos = 0);

        /** Printf like format */
    	String & Format(const char * fmt, ...)
#ifdef __GNUC__
        __attribute__ ((format (printf, 2, 3)))
#endif
        ;
    	/** Printf like format */
    	static String Print(const char * fmt, ...)
#ifdef __GNUC__
        __attribute__ ((format (printf, 1, 2)))
#endif
        ;
        /** Fill the string with 'fill' char length times */
    	inline String Filled(int length, unsigned char fill = ' ') const
    	{
    	    String a = *this;
    	    a.Fill(length, fill);
    	    return a;
    	}
    	/** Trim the both sides of this string with the given chars */
        inline String Trimmed(const String& b = String(" \t\v\f\r\n", sizeof(" \t\v\f\r\n"))) const
        {
            if (!b.slen) return *this;
            String a = *this;
            a.Trim(b);
            return a;
        }
        /** Get a uppercased version
            @sa toUppercase for the non fluent version */
        inline String asUppercase() const { String ret(*this); ret.toUppercase(); return ret; }
        /** Get a lowercased version
            @sa toLowercase for the non fluent version */
        inline String asLowercase() const { String ret(*this); ret.toLowercase(); return ret; }
        /** Normalized path, ready for concatenation */
        String normalizedPath(char sep = '/', const bool includeLastSep = true) const;
    	/** Align the string so it fits in the given length.
    	    @code
    	        String text = "abc";
    	        String alignedLeft = text.alignedTo(6, -1); // "abc   "
    	        String alignedRight = text.alignedTo(6, 1); // "   abc"
    	        String centered = text.alignedTo(6, 0); // " abc  "
    	        String larger = text.alignedTo(2, -1 or 1 or 0); // "abc"
    	    @endcode
    	    @param length   The length of the final string.
    	                    If the current string is larger than this length, the current string is returned
    	    @param fill     The char to fill the empty space with
    	    @param side     If 1, it's right aligned, 0 is centered (+- 1 char) and -1 is left aligned */
    	String alignedTo(const int length, int side = -1, char fill = ' ') const;

        /** Replace a token by another token
            @return A reference on this string, modified */
        String & replaceAllTokens(char from, char to);
        /** Return a string with all occurences of "find" replaced by "by" */
        String replacedAll(const String & find, const String & by) const;


        /** @}*/


        /** @name Unicode methods.
                  This string class stores strings as UTF-8. However, most methods expect one char to be one byte.
                  The methods below handle unicode encoding correctly, and deal with variable length encoding of UTF-8
            @{*/
        /** Get the i-th unicode char.
            This decodes the UTF-8 sequence for iterating the string.
            @param  pos     The unicode character index in the string, not it's position in memory
            @warning Do not use for converting the string to UTF-16 or UTF-32, you should use Strings::convert functions. */
        uint32 getUnicodeChar(const int pos = 0) const;
        /** This counts the number of Unicode characters in the string.
            This decodes the UTF-8 sequence for iterating the string.
            @warning Do not use for converting the string to UTF-16 or UTF-32, you should use Strings::convert functions. */
        size_t getUnicodeLength() const;
        /** @}*/

    	// Prevent unwanted conversion
    private:
        /** Prevent unwanted conversion */
        template <typename T> bool operator == (const T & t) const
        {   // If compiler stops here, it's because you're trying to compare a string with a type it doesn't understand.
            // Try to cast this type to (const char*) or (String), but don't rely on default.
            CompileTimeAssertFalse(T); return false;
        }
        /** Prevent unwanted conversion */
        template <typename T> bool operator < (const T & t) const
        {   // If compiler stops here, it's because you're trying to compare a string with a type it doesn't understand.
            // Try to cast this type to (const char*) or (String), but don't rely on default.
            CompileTimeAssertFalse(T); return false;
        }
        /** Prevent unwanted conversion */
        template <typename T> bool operator > (const T & t) const
        {   // If compiler stops here, it's because you're trying to compare a string with a type it doesn't understand.
            // Try to cast this type to (const char*) or (String), but don't rely on default.
            CompileTimeAssertFalse(T); return false;
        }
        /** Prevent unwanted conversion */
        template <typename T> bool operator <= (const T & t) const
        {   // If compiler stops here, it's because you're trying to compare a string with a type it doesn't understand.
            // Try to cast this type to (const char*) or (String), but don't rely on default.
            CompileTimeAssertFalse(T); return false;
        }
        /** Prevent unwanted conversion */
        template <typename T> bool operator >= (const T & t) const
        {   // If compiler stops here, it's because you're trying to compare a string with a type it doesn't understand.
            // Try to cast this type to (const char*) or (String), but don't rely on default.
            CompileTimeAssertFalse(T); return false;
        }
        /** Prevent unwanted conversion */
        template <typename T> bool operator != (const T & t) const
        {   // If compiler stops here, it's because you're trying to compare a string with a type it doesn't understand.
            // Try to cast this type to (const char*) or (String), but don't rely on default.
            CompileTimeAssertFalse(T); return false;
        }
    };

    extern const String operator+(const char *a, const String& b);
    extern const String operator+(const unsigned char *a, const String& b);
    extern const String operator+(char c, const String& b);
    extern const String operator+(unsigned char c, const String& b);
    extern const String operator+(const tagbstring& x, const String& b);
    inline const String operator * (int count, const String& b)
    {
    	String retval(b);
    	retval.Repeat(count);
    	return retval;
    }

  #if (WantRegularExpressions == 1)
    #define _STRINGIFY_(a) #a
    /** This is used to create regular expression strings without double escaping the strings.
        Like this:
        @code
            REx(var, "(\w+\d)"); // Equivalent to const char * var = "(\\w+\\d)"; ie. that puts (\w+\d) in var
        @endcode
        @sa String::regExMatch method */
    #define REx(var, re)  static char var##_[] = _STRINGIFY_(re); const char * var = ( var##_[ sizeof(var##_) - 2] = '\0',  (var##_ + 1) )

    template <size_t size>
    struct StackStr
    {
        char data[size - 2];
        operator const char *() const { return data; }
        StackStr(const char (&str)[size]) { memcpy(data, str+1, size - 3); data[size - 3] = 0; }
    };
    /** This is used to create a regular expression strings without double escaping the strings.
        Unlike RE, it does not create a named variable, but instead allocate space on the stack to
        store the regular expression in a temporary object
        Use like this:
        @code
            Logger::log(Logger::Dump, "Regular expression to match: %s", RE("(\w+\d)")); // Equivalent to (const char *)"(\\w+\\d)"
        @endcode
        @sa String::regExMatch and String::regExReplace method */
    #define RE(re) (const char*)::Bstrlib::StackStr<sizeof(_STRINGIFY_(re))>(_STRINGIFY_(re))
  #endif
} // namespace Bstrlib
#endif

#endif
