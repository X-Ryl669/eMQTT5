/*
 * This source file is part of the bstring string library.  This code was
 * written by Paul Hsieh in 2002 - 2006, and is covered by the BSD open source
 * license. Refer to the accompanying documentation for details on usage and
 * license.
 */

/*
 * bstrwrap.c
 *
 * This file is the C++ wrapper for the bstring functions.
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <limits.h>
#include <ctype.h>
#include "../include/Strings/bstring.hpp"
#include "../include/Strings/VerySimpleReadOnlyString.hpp"

#if defined(BSTRLIB_MEMORY_DEBUG)
#include "memdbg.h"
#endif

// Forward declare the uint64 serializer
namespace Strings { char* ulltoa(uint64 value, char* result, int base); }


namespace Bstrlib
{
    // Constructors.
    String::String()
    {
        slen = 0; mlen = 8;
        data = (unsigned char*)malloc(mlen);
        if (!data)
        {
            mlen = 0;
            bstringThrow("Failure in default constructor");
        }
        else data[0] = '\0';
    }

    String::String(const void * blk, int len)
    {
        slen = mlen = 0; data = 0;
        if (len >= 0)
        {
            mlen = len + 1;
            slen = len;
            data = (uint8 * ) malloc(mlen);
        }
        if (!data)
        {
            mlen = slen = 0;
            bstringThrow("Failure in block constructor");
        }
        else
        {
            if (slen > 0 && blk)    memcpy(data, blk, slen);
            data[slen] = '\0';
        }
    }

    String::String(const void * ownedBlock, const int len, const int allocLen)
    {
        slen = len; mlen = allocLen; data = (unsigned char*)ownedBlock;
    }


    String::String(char c, int len)
    {
        slen = mlen = 0; data = 0;
        if (len >= 0)
        {
            mlen = len + 1;
            slen = len;
            data = (uint8 * ) malloc(mlen);
        }
        if (!data)
        {
            mlen = slen = 0;
            bstringThrow("Failure in repeat(char) constructor");
        }
        else
        {
            if (slen > 0)    memset(data, c, slen);
            data[slen] = '\0';
        }
    }

    String::String(char c)
    {
        slen = mlen = 0; data = 0;
        data = (unsigned char*)malloc(2);
        if (data)
        {
            data[0] = (unsigned char)c;
            data[1] = '\0';
            mlen = 2; slen = 1;
        } else
            bstringThrow("Failure in (char) constructor");
    }

    String::String(unsigned char c)
    {
        slen = mlen = 0; data = 0;
        data = (unsigned char*)malloc(2);
        if (data)
        {
            data[0] = c;
            data[1] = '\0';
            mlen = 2; slen = 1;
        } else
            bstringThrow("Failure in (char) constructor");
    }

    String::String(const char *s)
    {
        slen = mlen = 0; data = 0;
        if (s)
        {
            slen = (int)strlen(s);
            mlen = slen + 1;
            data = (unsigned char*)malloc(mlen);

            if (!data)
            {
                mlen = slen = 0;
                bstringThrow("Failure in (char *) constructor");
            }
            else memcpy(data, s, mlen);
        }
    }

    String::String(int len, const char *s)
    {
        slen = mlen = 0; data = 0;
        if (s)
        {
            slen = (int)strlen(s);
            mlen = slen + 1;
            if (mlen < len)
                mlen = len;

            data = (uint8 * ) malloc(mlen);
            if (!data)
            {
                mlen = slen = 0;
                bstringThrow("Failure in (int len, char *) constructor");
            }
            memcpy(data, s, slen + 1);
        }
    }

    String::String(const String& b)
    {
        slen = b.slen;
        mlen = slen + 1;
        data = 0;
        if (mlen > 0)
            data = (uint8 * ) malloc(mlen);
        if (!data)
        {
            bstringThrow("Failure in (String) constructor");
        }
        else
        {
            memcpy(data, b.data, slen);
            data[slen] = '\0';
        }
    }

    String::String(const tagbstring& x)
    {
        slen = x.slen;
        mlen = slen + 1;
        data = 0;
        if (slen >= 0 && x.data != NULL)
            data = (uint8 * ) malloc(mlen);
        if (!data)
        {
            bstringThrow("Failure in (tagbstring) constructor");
        }
        else
        {
            memcpy(data, x.data, slen);
            data[slen] = '\0';
        }
    }
    String::String(const Strings::VerySimpleReadOnlyString & b)
    {
        slen = mlen = 0; data = 0;
        if (b.getLength() >= 0)
        {
            mlen = b.getLength() + 1;
            slen = b.getLength();
            data = (uint8 * ) malloc(mlen);
        }
        if (!data)
        {
            mlen = slen = 0;
            bstringThrow("Failure in block constructor");
        }
        else
        {
            if (slen > 0 && b.getData())    memcpy(data, b.getData(), slen);
            data[slen] = '\0';
        }
    }

    // Destructor.

    String::~String()
    {
        if (data != NULL)
        {
            free(data);
            data = NULL;
        }
        mlen = 0;
        slen = -__LINE__;
    }

    // = operator.

    const String& String::operator=(char c)
    {
        if (mlen <= 0)    bstringThrow("Write protection error");
        if (2 >= mlen)    Alloc(2);
        if (!data)
        {
            mlen = slen = 0;
            bstringThrow("Failure in =(char) operator");
        }
        else
        {
            slen = 1;
            data[0] = (unsigned char) c;
            data[1] = '\0';
        }
        return *this;
    }

    const String& String::operator=(unsigned char c)
    {
        if (mlen <= 0)    bstringThrow("Write protection error");
        if (2 >= mlen)    Alloc(2);
        if (!data)
        {
            mlen = slen = 0;
            bstringThrow("Failure in =(char) operator");
        }
        else
        {
            slen = 1;
            data[0] = c;
            data[1] = '\0';
        }
        return *this;
    }

    const String& String::operator=(const char *s)
    {
        int tmpSlen;
        if (mlen <= 0)    bstringThrow("Write protection error");
        if (NULL == s)    s = "";
        if ((tmpSlen = (int)strlen(s)) >= mlen)    Alloc(tmpSlen);

        if (data)
        {
            slen = tmpSlen;
            memcpy(data, s, tmpSlen + 1);
        }
        else
        {
            mlen = slen = 0;
            bstringThrow("Failure in =(const char *) operator");
        }
        return *this;
    }

    const String& String::operator=(const String& b)
    {
        if (&b == this)     return *this;
        if (mlen <= 0)        bstringThrow("Write protection error");
        if (b.slen >= mlen)    Alloc(b.slen);

        slen = b.slen;
        if (!data)
        {
            mlen = slen = 0;
            bstringThrow("Failure in =(String) operator");
        }
        else
        {
            memcpy(data, b.data, slen);
            data[slen] = '\0';
        }
        return *this;
    }

    const String& String::operator=(const tagbstring& x)
    {
        if (mlen <= 0)    bstringThrow("Write protection error");
        if (x.slen < 0)    bstringThrow("Failure in =(tagbstring) operator, badly formed tagbstring");
        if (x.slen >= mlen)    Alloc(x.slen);

        slen = x.slen;
        if (!data)
        {
            mlen = slen = 0;
            bstringThrow("Failure in =(tagbstring) operator");
        }
        else
        {
            memcpy(data, x.data, slen);
            data[slen] = '\0';
        }
        return *this;
    }

    const String& String::operator +=(const String& b)
    {
        if (BSTR_ERR == bconcat(this, (bstring)&b))
            bstringThrow("Failure in concatenate");
        return *this;
    }

    const String& String::operator +=(const char *s)
    {
        struct tagbstring x;

        char * d;
        int i, l;

        if (mlen <= 0)    bstringThrow("Write protection error");

        /* Optimistically concatenate directly */
        l = mlen - slen;
        d = (char *) &data[slen];
        for (i = 0; i < l; i++)
        {
            if ((*d++ = *s++) == '\0')
            {
                slen += i;
                return *this;
            }
        }
        slen += i;

        cstr2tbstr(x, s);
        if (BSTR_ERR == bconcat(this, &x))    bstringThrow("Failure in concatenate");

        return *this;
    }

    const String& String::operator +=(char c)
    {
        if (BSTR_ERR == bconchar(this, c))    bstringThrow("Failure in concatenate");
        return *this;
    }

    const String& String::operator +=(unsigned char c)
    {
        if (BSTR_ERR == bconchar(this, (char) c)) bstringThrow("Failure in concatenate");
        return *this;
    }

    const String& String::operator +=(const tagbstring& x)
    {
        if (mlen <= 0)    bstringThrow("Write protection error");
        if (x.slen < 0)    bstringThrow("Failure in +=(tagbstring) operator, badly formed tagbstring");
        Alloc(x.slen + slen + 1);

        if (!data)
        {
            mlen = slen = 0;
            bstringThrow("Failure in +=(tagbstring) operator");
        }
        else
        {
            memcpy(data + slen, x.data, x.slen);
            slen += x.slen;
            data[slen] = '\0';
        }
        return *this;
    }

    const String String::operator+(char c) const
    {
        String retval(*this);
        retval += c;
        return retval;
    }

    const String String::operator+(unsigned char c) const
    {
        String retval(*this);
        retval += c;
        return retval;
    }

    const String String::operator+(const String& b) const
    {
        String retval(*this);
        retval += b;
        return retval;
    }

    const String String::operator+(const char *s) const
    {
        String retval(*this);
        if (s == NULL)    return retval;
        retval += s;
        return retval;
    }

    const String String::operator+(const uint8 * s) const
    {
        String retval(*this);
        if (s == NULL)    return retval;
        retval +=(char *) s;
        return retval;
    }

    const String String::operator+(const int c) const
    {
        String retval(*this);
        retval += c;
        return retval;
    }
    const String String::operator+(const unsigned int c) const
    {
        String retval(*this);
        retval += c;
        return retval;
    }
    const String String::operator+(const float c) const
    {
        String retval(*this);
        retval += c;
        return retval;
    }
    const String String::operator+(const double c) const
    {
        String retval(*this);
        retval += c;
        return retval;
    }
    const String String::operator+(const int64 c) const
    {
        String retval(*this);
        retval += c;
        return retval;
    }
    const String String::operator+(const uint64 c) const
    {
        String retval(*this);
        retval += c;
        return retval;
    }

    const String& String::operator +=(const int c)
    {
#ifndef HasFloatParsing
        char buffer[12] = { c < 0 ? '-': '\0' };
        utoa((unsigned long)(c < 0 ? -c : c), &buffer[c < 0 ? 1 : 0], 10);
        return *this += buffer;
#else
        return *this += String::Print("%d", c);
#endif
    }
    const String& String::operator +=(const unsigned int c)
    {
#ifndef HasFloatParsing
        char buffer[11] = { 0 };
        utoa((unsigned long)c, buffer, 10);
        return *this += buffer;
#else
        return *this += String::Print("%u", c);
#endif
    }
    const String& String::operator +=(const int64 c)
    {
#ifndef HasFloatParsing
        char buffer[22] = { c < 0 ? '-': '\0' };
        Strings::ulltoa((uint64)(c < 0 ? -c : c), &buffer[c < 0 ? 1 : 0], 10);
        return *this += buffer;
#else
        return *this += String::Print(PF_LLD, c);
#endif
    }
    const String& String::operator +=(const uint64 c)
    {
#ifndef HasFloatParsing
        char buffer[21] = { 0 };
        Strings::ulltoa(c, buffer, 10);
        return *this += buffer;
#else
        return *this += String::Print(PF_LLU, c);
#endif
    }
    String String::getHexOf(const uint64 c)
    {
        char buffer[23] = { '0', 'x' };
        Strings::ulltoa(c, buffer+2, 16);
        return String(buffer);
    }

    int64 String::parseInt(int base, int * endPos) const
    {
        const char * text = (const char*)data;
        if (!slen) { if (endPos) *endPos = 0; return 0; }
        bool negative = text[0] == '-';
        text += (int)negative;
        const char baseText[] = "0123456789abcdef", BASEText[] = "0123456789ABCDEF";

        // Check the current base, if auto detection is activated
        if (!base)
        {
            if (text[0] != '0') base = 10;
            else switch(text[1])
            {
            case 'X': case 'x': base = 16; text += 2; break;
            case 'B': case 'b': base = 2; text += 2; break;
            default: base = 8; text += 1; break;
            }
        }
        // Let's start conversion
        int64 ret = 0;
        while (text)
        {
            const char * charPos = strchr(baseText, *text);
            int digit = (int)(charPos - baseText);
            if (charPos == NULL && base > 10) { charPos = strchr(BASEText, *text); digit = (int)(charPos - BASEText); }
            if (charPos == NULL) break;
            if (digit >= base) break;
            ret = ret * base + digit;
            text++;
        }
        if (endPos) *endPos = (int)(text - (const char*)data);
        return negative ? -ret : ret;
    }

    const String& String::operator +=(const float c)
    {
#ifndef HasFloatParsing
        char buffer[15] = { 0 };
        ftoa(c, buffer, 6);
        return *this += buffer;
#else
        return *this += String::Print("%lg", c);
#endif
    }
    const String& String::operator +=(const double c)
    {
#ifndef HasFloatParsing
        char buffer[15] = { 0 };
        ftoa((float)c, buffer, 6);
        return *this += buffer;
#else
        return *this += String::Print("%lg", c);
#endif
    }

    const String String::operator+(const tagbstring& x) const
    {
        if (x.slen < 0)    bstringThrow("Failure in + (tagbstring) operator, badly formed tagbstring");
        String retval(*this);
        retval += x;
        return retval;
    }

    bool String::operator ==(const String& b) const
    {
        int retval;
        if (BSTR_ERR ==(retval = biseq((bstring)this, (bstring)&b))) bstringThrow("Failure in compare (==)");
        return retval != 0;
    }

    bool String::operator ==(const char * s) const
    {
        int retval;
        if (NULL == s)    return slen == 0;
        if (BSTR_ERR ==(retval = biseqcstr((bstring) this, s)))    bstringThrow("Failure in compare (==)");
        return retval != 0;
    }

    bool String::operator ==(const uint8 *  s) const
    {
        int retval;
        if (NULL == s) return slen == 0;
        if (BSTR_ERR ==(retval = biseqcstr((bstring) this, (char *) s))) bstringThrow("Failure in compare (==)");
        return retval != 0;
    }

    bool String::operator !=(const String& b) const
    {
        return !((*this) == b);
    }

    bool String::operator !=(const char * s) const
    {
        return !((*this) == s);
    }

    bool String::operator !=(const uint8 *  s) const
    {
        return !((*this) == s);
    }

    bool String::operator <(const String& b) const
    {
        int retval;
        if (SHRT_MIN ==(retval = bstrcmp((bstring) this, (bstring)&b)))
            bstringThrow("Failure in compare (<)");
        return retval < 0;
    }

    bool String::operator <(const char * s) const
    {
        if (NULL == s) return false;
        return strcmp((const char *)this->data, s) < 0;
    }

    bool String::operator <(const uint8 *  s) const
    {
        if (NULL == s) return false;
        return strcmp((const char *)this->data, (const char *)s) < 0;
    }

    bool String::operator <=(const String& b) const
    {
        int retval;
        if (SHRT_MIN ==(retval = bstrcmp((bstring) this, (bstring)&b)))    bstringThrow("Failure in compare (<=)");
        return retval <= 0;
    }

    bool String::operator <=(const char * s) const
    {
        if (NULL == s) return slen == 0;
        return strcmp((const char *)this->data, s) <= 0;
    }

    bool String::operator <=(const uint8 *  s) const
    {
        if (NULL == s) return slen == 0;
        return strcmp((const char *)this->data, (const char *)s) <= 0;
    }

    bool String::operator >(const String& b) const
    {
        return !((*this) <= b);
    }

    bool String::operator >(const char * s) const
    {
        return !((*this) <= s);
    }

    bool String::operator >(const uint8 *  s) const
    {
        return !((*this) <= s);
    }

    bool String::operator >=(const String& b) const
    {
        return !((*this) < b);
    }

    bool String::operator >=(const char * s) const
    {
        return !((*this) < s);
    }

    bool String::operator >=(const uint8 *  s) const
    {
        return !((*this) < s);
    }

#ifdef HasFloatParsing
    String::operator double() const
    {
        return parseDouble();
    }

    String::operator float() const
    {
        return (float)parseDouble();
    }
    double String::parseDouble(int * consumed) const
    {
        char * ep = NULL;
        double ret = data ? strtod((const char*)data, &ep) : 0;
        if (consumed) *consumed = (int)(ep - (const char*)data);
        return ret;
    }
#endif

    String::operator signed int() const
    {
        return (signed int)parseInt(10);
    }

    String::operator unsigned int() const
    {
        return (unsigned int)parseInt(10);
    }

    String::operator int64() const
    {
        return (int64)parseInt(10);
    }

    int String::Scan(const char * fmt, void * data) const
    {
        return sscanf((const char *)this->data, fmt, data);
    }


#ifdef __TURBOC__
# ifndef BSTRLIB_NOVSNP
#  define BSTRLIB_NOVSNP
# endif
#endif

    /* Give WATCOM C/C++, MSVC some latitude for their non - support of vsnprintf */
#if defined(__WATCOMC__) || defined(_MSC_VER)
#define exvsnprintf(r,b,n,f,a) {r = _vsnprintf (b,n,f,a);}
#else
#ifdef BSTRLIB_NOVSNP
    /* This is just a hack.  If you are using a system without a vsnprintf, it is
    not recommended that bformat be used at all. */
#define exvsnprintf(r,b,n,f,a) {vsprintf (b,f,a); r = -1;}
#define START_VSNBUFF (256)
#else

#if defined(__GNUC__) && !defined(__PPC__)
    /* Something is making gcc complain about this prototype not being here, so
    I've just gone ahead and put it in. */
    extern "C"
    {
        extern int vsnprintf(char *buf, size_t count, const char *format, va_list arg);
    }
#endif

#define exvsnprintf(r,b,n,f,a) {r = vsnprintf (b,n,f,a);}
#endif
#endif

#ifndef START_VSNBUFF
#define START_VSNBUFF (16)
#endif

    /*
    * Yeah I'd like to just call a vformat function or something, but because of
    * the ANSI specified brokeness of the va_* macros, it is actually not
    * possible to do this correctly.
    */
    String & String::Format(const char * fmt, ...)
    {
        bstring b;
        va_list arglist;
        int r, n;

        if (mlen <= 0)        bstringThrow("Write protection error");
        if (fmt == NULL)
            *this = "<NULL>";
        else
        {
            if ((b = bfromcstr("")) == NULL)
            {
                *this = "<NULL>";
            }
            else
            {
                if ((n = 2*(int)strlen(fmt)) < START_VSNBUFF)
                    n = START_VSNBUFF;
                for (;;)
                {
                    if (BSTR_OK != balloc(b, n + 2))
                    {
                        b = bformat("<OUTM>");
                        break;
                    }

                    va_start(arglist, fmt);
                    exvsnprintf(r, (char *) b->data, n + 1, fmt, arglist);
                    va_end(arglist);

                    b->data[n] = '\0';
                    b->slen = (int)strlen((char *) b->data);

                    if (b->slen < n)    break;
                    if (r > n)            n = r;
                    else                 n += n;
                }
                *this = *b;
                bdestroy(b);
            }
        }
        return *this;
    }

    /*
    * Yeah I'd like to just call a vformat function or something, but because of
    * the ANSI specified brokeness of the va_* macros, it is actually not
    * possible to do this correctly.
    */
    String String::Print(const char * fmt, ...)
    {
        bstring b;
        va_list arglist;
        int r, n;
        String ret;

        if (fmt == NULL) return ret;
        else
        {
            if ((b = bfromcstr("")) == NULL)
            {
                ret = "<NULL>";
            }
            else
            {
                if ((n = 2*(int)strlen(fmt)) < START_VSNBUFF)
                    n = START_VSNBUFF;
                for (;;)
                {
                    if (BSTR_OK != balloc(b, n + 2))
                    {
                        b = bformat("<OUTM>");
                        break;
                    }

                    va_start(arglist, fmt);
                    exvsnprintf(r, (char *) b->data, n + 1, fmt, arglist);
                    va_end(arglist);

                    b->data[n] = '\0';
                    b->slen = (int)strlen((char *) b->data);

                    if (b->slen < n)    break;
                    if (r > n)            n = r;
                    else                 n += n;
                }
                ret = *b;
                bdestroy(b);
            }
        }
        return ret;
    }


    void String::Formata(const char * fmt, ...)
    {
        bstring b;
        va_list arglist;
        int r, n;

        if (mlen <= 0)     bstringThrow("Write protection error");
        if (fmt == NULL)
        {
            *this += "<NULL>";
        }
        else
        {
            if ((b = bfromcstr("")) == NULL)
            {
                *this += "<NULL>";
            }
            else
            {
                if ((n = 2*(int)strlen(fmt)) < START_VSNBUFF)
                    n = START_VSNBUFF;
                for (;;)
                {
                    if (BSTR_OK != balloc(b, n + 2))
                    {
                        b = bformat("<OUTM>");
                        break;
                    }

                    va_start(arglist, fmt);
                    exvsnprintf(r, (char *) b->data, n + 1, fmt, arglist);
                    va_end(arglist);

                    b->data[n] = '\0';
                    b->slen = (int)strlen((char *) b->data);

                    if (b->slen < n)    break;
                    if (r > n)            n = r;
                    else                 n += n;
                }
                *this += *b;
                bdestroy(b);
            }
        }
    }

    bool String::caselessEqual(const String& b) const
    {
        int ret;
        if (BSTR_ERR ==(ret = biseqcaseless((bstring) this, (bstring) &b)))    bstringThrow("String::caselessEqual Unable to compare");
        return ret == 1;
    }

    int String::caselessCmp(const String& b) const
    {
        int ret;
        if (SHRT_MIN ==(ret = bstricmp((bstring) this, (bstring) &b)))        bstringThrow("String::caselessCmp Unable to compare");
        return ret;
    }

    int String::Count(const String & b) const
    {
        int i = 0, j = 0;
        int count = 0;

        for (; i + j < slen; )
        {
            if ((unsigned char) b[j] == data[i + j])
            {
                j++;
                if (j == b.slen)    { i+= j; j = 0; count ++; continue; }
                continue;
            }
            i++;
            j = 0;
        }

        return count;
    }

    int String::Find(const String& b, int pos) const
    {
        return binstr((bstring) this, pos, (bstring) &b);
    }

    int String::Find(const char * b, int pos) const
    {
        int i, j;

        if (NULL == b) return BSTR_ERR;

        if ((unsigned int) pos > (unsigned int) slen)
            return BSTR_ERR;
        if ('\0' == b[0])            return pos;
        if (pos == slen)            return BSTR_ERR;

        i = pos;
        j = 0;

        for (; i + j < slen; )
        {
            if ((unsigned char) b[j] == data[i + j])
            {
                j++;
                if ('\0' == b[j])    return i;
                continue;
            }
            i++;
            j = 0;
        }

        return BSTR_ERR;
    }

    int String::caselessFind(const String& b, int pos) const
    {
        return binstrcaseless((bstring) this, pos, (bstring) &b);
    }

    int String::caselessFind(const char * b, int pos) const
    {
        struct tagbstring t;

        if (NULL == b) return BSTR_ERR;

        if ((unsigned int) pos > (unsigned int) slen)    return BSTR_ERR;
        if ('\0' == b[0])            return pos;
        if (pos == slen)            return BSTR_ERR;

        btfromcstr(t, b);
        return binstrcaseless((bstring) this, pos, (bstring) &t);
    }

    int String::Find(char c, int pos) const
    {
        if (pos < 0)    return BSTR_ERR;
        for (; pos < slen; pos++)
        {
            if (data[pos] ==(unsigned char) c)
                return pos;
        }
        return BSTR_ERR;
    }

    int String::reverseFind(const String& b, int pos) const
    {
        return binstrr((bstring) this, pos, (bstring) &b);
    }

    int String::reverseFind(const char * b, int pos) const
    {
        struct tagbstring t;
        if (NULL == b) return BSTR_ERR;
        cstr2tbstr(t, b);
        return binstrr((bstring) this, pos, &t);
    }

    int String::caselessReverseFind(const String& b, int pos) const
    {
        return binstrrcaseless((bstring) this, pos, (bstring) &b);
    }

    int String::caselessReverseFind(const char * b, int pos) const
    {
        struct tagbstring t;

        if (NULL == b) return BSTR_ERR;

        if ((unsigned int) pos > (unsigned int) slen)        return BSTR_ERR;
        if ('\0' == b[0])                                    return pos;
        if (pos == slen)                                    return BSTR_ERR;

        btfromcstr(t, b);
        return binstrrcaseless((bstring) this, pos, (bstring) &t);
    }

    int String::reverseFind(char c, int pos) const
    {
        if (pos < 0) pos = slen-1;
        if (pos > slen)            return BSTR_ERR;
        if (pos == slen)        pos--;
        for (; pos >= 0; pos--)
        {
            if (data[pos] ==(unsigned char) c)    return pos;
        }
        return BSTR_ERR;
    }

    int String::findAnyChar(const String& b, int pos) const
    {
        return binchr((bstring) this, pos, (bstring) &b);
    }

    int String::findAnyChar(const char * s, int pos) const
    {
        struct tagbstring t;
        if (NULL == s) return BSTR_ERR;
        cstr2tbstr(t, s);
        return binchr((bstring) this, pos, (bstring) &t);
    }

    int String::invFindAnyChar(const String& b, int pos) const
    {
        return bninchr((bstring) this, pos, (bstring) &b);
    }

    int String::invFindAnyChar(const char * s, int pos) const
    {
        struct tagbstring t;
        if (NULL == s) return BSTR_ERR;
        cstr2tbstr(t, s);
        return bninchr((bstring) this, pos, &t);
    }

    int String::reverseFindAnyChar(const String& b, int pos) const
    {
        return binchrr((bstring) this, pos, (bstring) &b);
    }

    int String::reverseFindAnyChar(const char * s, int pos) const
    {
        struct tagbstring t;
        if (NULL == s) return BSTR_ERR;
        cstr2tbstr(t, s);
        return binchrr((bstring) this, pos, &t);
    }

    int String::invReverseFindAnyChar(const String& b, int pos) const
    {
        return bninchrr((bstring) this, pos, (bstring) &b);
    }

    int String::invReverseFindAnyChar(const char * s, int pos) const
    {
        struct tagbstring t;
        if (NULL == s) return BSTR_ERR;
        cstr2tbstr(t, s);
        return bninchrr((bstring) this, pos, &t);
    }


    String String::extractToken(char c, int & pos) const
    {
        String ret;
        if (pos >= slen) return ret;

        int findNextPos = Find(c, pos);
        if (findNextPos == -1) findNextPos = slen;
        ret = midString(pos, findNextPos - pos);
        pos = findNextPos + 1;
        return ret;
    }

    const String String::midString(int left, int len) const
    {
        struct tagbstring t;
        if (len < 0)
        {   // Want data from the right of the string, without specifying the start point
            // For example String("abcdefgh").midString(0, -3) => "abcde"
            if (-len >= slen) left = 0;
            else
            {   // Check for String("abcdef").midString(-3, -1) => "de"
                if (left < 0) { left = max(0, slen + left); len = slen + len - left; }
                else len += slen;
            }
        }
        if (left < 0)
        {   // Want data from the right of the string
            len = len > -left ? -left : len;
            left = slen + left;
        }
        if (len > slen - left)    len = slen - left;
        if (len <= 0 || left < 0)    return String("");
        blk2tbstr(t, data + left, len);
        return String(t);
    }

    char * String::Alloc(int length)
    {
        if (BSTR_ERR == balloc((bstring)this, length))    bstringThrow("Failure in Alloc");
        return (char*)data;
    }

    void String::Fill(int length, unsigned char fill)
    {
        slen = 0;
        if (BSTR_ERR == bsetstr(this, length, NULL, fill))    bstringThrow("Failure in fill");
    }

    void String::setSubstring(int pos, const String& b, unsigned char fill)
    {
        if (BSTR_ERR == bsetstr(this, pos, (bstring) &b, fill))    bstringThrow("Failure in setstr");
    }

    void String::setSubstring(int pos, const char * s, unsigned char fill)
    {
        struct tagbstring t;
        if (NULL == s) return;
        cstr2tbstr(t, s);
        if (BSTR_ERR == bsetstr(this, pos, &t, fill))
        {
            bstringThrow("Failure in setstr");
        }
    }

    void String::Insert(int pos, const String& b, unsigned char fill)
    {
        if (BSTR_ERR == binsert(this, pos, (bstring) &b, fill))    bstringThrow("Failure in insert");
    }

    void String::Insert(int pos, const char * s, unsigned char fill)
    {
        struct tagbstring t;
        if (NULL == s) return;
        cstr2tbstr(t, s);
        if (BSTR_ERR == binsert(this, pos, &t, fill))    bstringThrow("Failure in insert");
    }

    void String::insertChars(int pos, int len, unsigned char fill)
    {
        if (BSTR_ERR == binsertch(this, pos, len, fill)) bstringThrow("Failure in insertchrs");
    }

    void String::Replace(int pos, int len, const String& b, unsigned char fill)
    {
        if (BSTR_ERR == breplace(this, pos, len, (bstring) &b, fill)) bstringThrow("Failure in Replace");
    }

    void String::Replace(int pos, int len, const char * s, unsigned char fill)
    {
        struct tagbstring t;
        int q;

        if (mlen <= 0)                        bstringThrow("Write protection error");
        if (NULL == s || (pos | len) < 0)    return;


        if (pos + len >= slen)
        {
            cstr2tbstr(t, s);
            if (BSTR_ERR == bsetstr(this, pos, &t, fill))
                bstringThrow("Failure in Replace");
            else if (pos + t.slen < slen)
            {
                slen = pos + t.slen;
                data[slen] = '\0';
            }
        }
        else
        {
            /* Aliasing case */
            if ((unsigned int)(data - (uint8 * ) s) < (unsigned int) slen)
            {
                Replace(pos, len, String(s), fill);
                return;
            }

            if ((q = (int)strlen(s)) > len)
            {
                Alloc(slen + q - len);
                if (NULL == data)    return;
            }
            if (q != len) memmove(data + pos + q, data + pos + len, slen - (pos + len));
            memcpy(data + pos, s, q);
            slen += q - len;
            data[slen] = '\0';
        }
    }

    String & String::findAndReplace(const String& find, const String& repl, int pos)
    {
        if (BSTR_ERR == bfindreplace(this, (bstring) &find, (bstring) &repl, pos))    bstringThrow("Failure in findreplace");
        return *this;
    }


    String & String::findAndReplace(const String& find, const char * repl, int pos)
    {
        struct tagbstring t;
        if (NULL == repl) return *this;
        cstr2tbstr(t, repl);
        if (BSTR_ERR == bfindreplace(this, (bstring) &find, (bstring) &t, pos))    bstringThrow("Failure in findreplace");
        return *this;
    }

    String & String::findAndReplace(const char * find, const String& repl, int pos)
    {
        struct tagbstring t;
        if (NULL == find) return *this;

        cstr2tbstr(t, find);
        if (BSTR_ERR == bfindreplace(this, (bstring) &t, (bstring) &repl, pos))    bstringThrow("Failure in findreplace");
        return *this;
    }

    String & String::findAndReplace(const char * find, const char * repl, int pos)
    {
        struct tagbstring t, u;
        if (NULL == repl || NULL == find) return *this;
        cstr2tbstr(t, find);
        cstr2tbstr(u, repl);
        if (BSTR_ERR == bfindreplace(this, (bstring) &t, (bstring) &u, pos)) bstringThrow("Failure in findreplace");
        return *this;
    }

    String & String::findAndReplaceCaseless(const String& find, const String& repl, int pos)
    {
        if (BSTR_ERR == bfindreplacecaseless(this, (bstring) &find, (bstring) &repl, pos))    bstringThrow("Failure in findreplacecaseless");
        return *this;
    }


    String & String::findAndReplaceCaseless(const String& find, const char * repl, int pos)
    {
        struct tagbstring t;
        if (NULL == repl) return *this;
        cstr2tbstr(t, repl);
        if (BSTR_ERR == bfindreplacecaseless(this, (bstring) &find, (bstring) &t, pos))    bstringThrow("Failure in findreplacecaseless");
        return *this;
    }

    String & String::findAndReplaceCaseless(const char * find, const String& repl, int pos)
    {
        struct tagbstring t;
        if (NULL == find) return *this;
        cstr2tbstr(t, find);
        if (BSTR_ERR == bfindreplacecaseless(this, (bstring) &t, (bstring) &repl, pos))    bstringThrow("Failure in findreplacecaseless");
        return *this;
    }

    String & String::findAndReplaceCaseless(const char * find, const char * repl, int pos)
    {
        struct tagbstring t, u;
        if (NULL == repl || NULL == find) return *this;
        cstr2tbstr(t, find);
        cstr2tbstr(u, repl);
        if (BSTR_ERR == bfindreplacecaseless(this, (bstring) &t, (bstring) &u, pos)) bstringThrow("Failure in findreplacecaseless");
        return *this;
    }

    void String::Remove(int pos, int len)
    {
        if (BSTR_ERR == bdelete(this, pos, len)) bstringThrow("Failure in remove");
    }

    void String::Truncate(int len)
    {
        if (len < 0) return;
        if (len < slen)
        {
            slen = len;
            data[len] = '\0';
        }
    }

    void String::leftTrim(const String& b)
    {
                if (!b.slen) return;
        int l = invFindAnyChar(b, 0);
        if (l == BSTR_ERR)    l = slen;
        Remove(0, l);
    }

    void String::rightTrim(const String& b)
    {
                if (!b.slen) return;
        int l = invReverseFindAnyChar(b, slen - 1);
        if (l == BSTR_ERR)    l = slen - 1;
        slen = l + 1;
        if (mlen > slen) data[slen] = '\0';
    }

    void String::toUppercase()
    {
        if (BSTR_ERR == btoupper((bstring) this))    bstringThrow("Failure in toupper");
    }

    void String::toLowercase()
    {
        if (BSTR_ERR == btolower((bstring) this))    bstringThrow("Failure in tolower");
    }

    void String::Repeat(int count)
    {
        count *= slen;
        if (count <= 0)
        {
            Truncate(0);
            return;
        }
        if (BSTR_ERR == bpattern(this, count))    bstringThrow("Failure in repeat");
    }
/*
    int String::gets(bNgetc getcPtr, void * parm, char terminator)
    {
        if (mlen <= 0)    bstringThrow("Write protection error");
        bstring b = bgets(getcPtr, parm, terminator);
        if (b == NULL)
        {
            slen = 0;
            return -1;
        }
        *this = *b;
        bdestroy(b);
        return 0;
    }

    int String::read(bNread readPtr, void * parm)
    {
        if (mlen <= 0)    bstringThrow("Write protection error");
        bstring b = bread(readPtr, parm);
        if (b == NULL)
        {
            slen = 0;
            return -1;
        }
        *this = *b;
        bdestroy(b);
        return 0;
    }
*/
    const String operator+(const char *a, const String& b)
    {
        return String(a) + b;
    }

    const String operator+(const uint8 * a, const String& b)
    {
        return String((const char *)a) + b;
    }

    const String operator+(char c, const String& b)
    {
        return String(c) + b;
    }

    const String operator+(unsigned char c, const String& b)
    {
        return String(c) + b;
    }

    const String operator+(const tagbstring& x, const String& b)
    {
        return String(x) + b;
    }

    String String::normalizedPath(char sep, const bool includeLastSep) const
    {
        int rightLimit = slen - 1;
        while (rightLimit >= 0 && data[rightLimit] == sep) rightLimit--;
        return includeLastSep || rightLimit < 0 ? midString(0, rightLimit+1) + sep : midString(0, rightLimit+1);
    }

    String & String::replaceAllTokens(char from, char to)
    {
        if (!from || !to) return *this;
        for (int i = 0; i < slen; i++)
            if (data[i] == from) data[i] = to;
        return *this;
    }

    String String::replacedAll(const String & find, const String & by) const
    {
        int count = Count(find);
        int replacedLength = (slen + count * (by.slen - find.slen)) + 1;
        char * rep = (char*)malloc(replacedLength);
        if (rep == NULL) return String();
        int pos = 0, repos = 0, newPos = 0;
        while ((newPos = Find(find, pos)) != -1)
        {
            memcpy(&rep[repos], &data[pos], (newPos - pos));
            repos += (newPos - pos);
            memcpy(&rep[repos], by.data, by.slen);
            repos += by.slen;
            newPos += find.slen;
            pos = newPos;
        }
        // Copy the remaining part
        if (pos < slen)
        {
            memcpy(&rep[repos], &data[pos], slen - pos);
            repos += slen - pos;
        }
        rep[repos] = '\0';
        return String(rep, replacedLength - 1, replacedLength);
    }


    // Get the string up to the first occurrence of the given string
    const String String::upToFirst(const String & find, const bool includeFind) const
    {
        int pos = Find(find, 0);
        if (pos == -1) return includeFind ? "" : *this;
        return midString(0, includeFind ? pos + find.getLength() : pos);
    }
    // Get the string up to the last occurrence of the given string
    const String String::upToLast(const String & find, const bool includeFind) const
    {
        int pos = reverseFind(find, slen - 1);
        if (pos == -1) return includeFind ? "" : *this;
        return midString(0, includeFind ? pos + find.getLength() : pos);
    }
    // Get the string from the first occurrence of the given string
    const String String::fromFirst(const String & find, const bool includeFind) const
    {
        int pos = Find(find, 0);
        if (pos == -1) return includeFind ? *this : String();
        return midString(includeFind ? pos : pos + find.getLength(), slen);
    }
    // Get the string from the first occurrence of the given string
    const String String::dropUpTo(const String & find, const bool includeFind) const
    {
        const unsigned int pos = Find(find);
        if (pos == (unsigned int)-1) return *this;
        return midString(includeFind ? pos : pos + find.getLength(), slen);
    }

    // Get the string from the last occurrence of the given string
    const String String::fromLast(const String & find, const bool includeFind) const
    {
        int pos = reverseFind(find, slen - 1);
        if (pos == -1) return includeFind ? *this : String();
        return midString(includeFind ? pos : pos + find.getLength(), slen);
    }
    // Split a string when the needle is found first, returning the part before the needle, and
    // updating the string to start on or after the needle.
    const String String::splitFrom(const String & find, const bool includeFind)
    {
        int pos = Find(find, 0);
        if (pos == -1)
        {
            if (includeFind)
            {
                String ret(*this);
                *this = "";
                return ret;
            }
            return String();
        }
        int size = pos + find.getLength();
        String ret = midString(0, includeFind ? size : pos);
        if (BSTR_ERR == bdelete(this, 0, size)) bstringThrow("Failure in remove");
        return ret;
    }
    // Split a string when the needle is found first, returning the part before the needle, and
    // updating the string to start on or after the needle.
    const String String::fromTo(const String & from, const String & to, const bool includeFind) const
    {
        const int fromPos = Find(from);
        if (fromPos == -1) return "";
        const int toPos = Find(to, fromPos + from.slen);
        return midString(includeFind ? fromPos : fromPos + from.slen, toPos != -1 ? (includeFind ? toPos + to.slen - fromPos : toPos - fromPos - from.slen)
                                           // If the "to" needle was not found, either we return the whole string (includeFind) or an empty string
                                           : (includeFind ? slen - fromPos : 0));
    }
    // Get the substring up to the given needle if found, or the whole string if not, and split from here.
    const String String::splitUpTo(const String & find, const bool includeFind)
    {
        int pos = Find(find, 0);
        if (pos == -1)
        {
            String ret(*this);
            *this = "";
            return ret;
        }
        int size = pos + find.getLength();
        String ret = midString(0, includeFind ? size : pos);
        if (BSTR_ERR == bdelete(this, 0, size)) bstringThrow("Failure in remove");
        return ret;
    }
    // Split the string at the given position.
    const String String::splitAt(const int pos)
    {
        String ret = midString(0, pos);
        if (BSTR_ERR == bdelete(this, 0, pos)) bstringThrow("Failure in remove");
        return ret;
    }
    // Split the string when no more in the given set
    const String String::splitWhenNoMore(const String & set)
    {
        int pos = invFindAnyChar(set);
        String ret = midString(0, pos == -1 ? slen : pos);
        if (BSTR_ERR == bdelete(this, 0, ret.slen)) bstringThrow("Failure in remove");
        return ret;
    }

    // Align the string so it fits in the given length.
    String String::alignedTo(const int length, int side, char fill) const
    {
        if (slen > length) return *this;
        int diffInSize = (length - slen),
            leftCount = side == 1 ? diffInSize : (side == 0 ? diffInSize / 2 : 0),
            rightCount = side == -1 ? diffInSize : (side == 0 ? (diffInSize + 1) / 2 : 0);

        return String().Filled(leftCount, fill) + *this + Filled(rightCount, fill);
    }




    void String::writeProtect()
    {
        if (mlen >= 0)    mlen = -1;
    }

    void String::writeAllow()
    {
        if (mlen == -1)        mlen = slen + (slen == 0);
        else if (mlen < 0)    bstringThrow("Cannot unprotect a constant");
    }

    uint32 getUnicode(const unsigned char * & array)
    {
        // Get the current char
        const uint8 c = *array;
        if ((c & 0x80))
        {
            // The data is in the 7 low bits
            uint32 dataMask = 0x7f;
            // The count bit mask
            uint32 bitCountMask = 0x40;
            // The consumption count
            int charCount = 0;

            while ((c & bitCountMask) != 0 && bitCountMask)
            {
                ++charCount;
                dataMask >>= 1; bitCountMask >>= 1;
            }

            // Get the few bits remaining here
            uint32 n = (c & dataMask);

            // Then extract the remaining bits
            ++array;
            while (--charCount >= 0 && *array)
            {
                const uint8 extra = *array;
                // Make sure it's a valid UTF8 encoding
                if ((extra & 0xc0) != 0x80) break;

                // Store the new bits too
                n <<= 6; n |= (extra & 0x3f);
                ++array;
            }
            return n;
        }
        if (c) ++array;
        return c;
    }
    int getUnicodeCharCount(const unsigned char * array)
    {
        const uint8 c = *array;
        if ((c & 0x80))
        {
            // The count bit mask
            uint32 bitCountMask = 0x40;
            // The consumption count
            int charCount = 0;

            while ((c & bitCountMask) != 0 && bitCountMask)
            {
                ++charCount;
                bitCountMask >>= 1;
            }
            return charCount;
        }
        return c == 0 ? 0 : 1;
    }

    // Get the i-th unicode char.
    uint32 String::getUnicodeChar(int pos) const
    {
        // 1. Iterate to the given position
        const unsigned char * array = data;
        while (pos && *array)
        {
            (void)getUnicode(array);
            pos--;
        }
        return getUnicode(array);
    }
    // This counts the number of Unicode characters in the string.
    size_t String::getUnicodeLength() const
    {
        size_t size = 0;
        unsigned char * array = data, * end = data + slen;
        int cnt = getUnicodeCharCount(&array[size]);
        while (cnt && array < end)
        {
            size ++;
            array += cnt;
            cnt = getUnicodeCharCount(array);
        }
        return size;
    }

#if (WantRegularExpressions == 1)

    // Forward declare the functions we'll be using
    namespace RegExp
    {
        #define TestReturn(cond, error) if (cond) return (error)

        struct Branch
        {
            /** Index of the bracket pair */
            int bracketIndex;
            /** Position of branching in the regular expression */
            const char * branchPos;
        };

        struct BracketPair
        {
            /** Index of the branch for this pair */
            int branchIndex;
            /** The number of branches in this pair */
            int branchesCount;
            /** First char after '(' in expression  */
            const char * ptr;
            /** Text length in char between parenthesis */
            int len;
        };

        /** A Capture */
        struct Cap
        {
            const char * ptr;
            int          len;
        };


        /** Default construct the information with stack based allocation, and only expand if required to heap based allocation */
        struct RegexpInfo
        {
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

            /** Store all branches in the regular expression. */
            Branch * branches;
            int branchesCount;

            /** Store all brackets pair in the expression. The first entry contains the whole expression */
            BracketPair * brackets;
            int bracketsCount;


            /* Array of captures provided by the user */
            Cap * caps;
            int capsCount;
            /** You can use (?i) to start the Regular Expression, or use caseInsensitive search while matching */
            bool caseSensitive;
            /** First position for the match */
            int firstMatchPos;

            /** The stack based allocation for the branches */
            Branch stackBranch[10];
            /** The stack based allocation for the pairs */
            BracketPair stackPair[10];

            /** Construction */
            RegexpInfo(Cap * capts, int capsCount, bool caseSensitive = true)
            : branches(stackBranch), branchesCount(0), brackets(stackPair), bracketsCount(0), caps(capts), capsCount(capsCount), caseSensitive(caseSensitive), firstMatchPos(-1)
            {}

            ~RegexpInfo()
            {
                if (branches != stackBranch) { free(branches); branches = 0; }
                if (brackets != stackPair) { free(brackets); brackets = 0; }
            }

            /** This is used for marking allowed metacharacters and quantifiers */
            static uint8 charMap[256];

            bool addBrackets(const char * ptr, int len)
            {
                if ((size_t)(bracketsCount + 1) >= ArrSz(stackPair))
                {
                    // Need to re-allocate the brackets pair
                    BracketPair * newPair = (BracketPair *)malloc((bracketsCount + 1) * sizeof(BracketPair));
                    if (!newPair) return false;
                    memcpy(newPair, brackets, bracketsCount * sizeof(BracketPair));
                    if (brackets != stackPair) free(brackets);
                    brackets = newPair;
                }
                brackets[bracketsCount].ptr = ptr;
                brackets[bracketsCount].len = len;
                bracketsCount++;
                return true;
            }

            bool addBranch(int index, const char * pos)
            {
                if ((size_t)(branchesCount + 1) >= ArrSz(stackBranch))
                {
                    // Need to re-allocate the brackets pair
                    Branch * newBranch = (Branch *)malloc((branchesCount + 1) * sizeof(Branch));
                    if (!newBranch) return false;
                    memcpy(newBranch, branches, branchesCount * sizeof(Branch));
                    if (branches != stackBranch) free(branches);
                    branches = newBranch;
                }
                branches[branchesCount].bracketIndex = index;
                branches[branchesCount].branchPos = pos;
                branchesCount++;
                return true;
            }

            /** Helper functions */
            static inline int operatorLen(const char *re) { return re[0] == '\\' && re[1] == 'x' ? 4 : re[0] == '\\' ? 2 : 1; }
            static inline int getOperatorLen(const char *re, int regExpLen)
            {
                if (re[0] != '[') return operatorLen(re);
                int len = 1;
                while (len < regExpLen && re[len] != ']') len += operatorLen(re + len);
                return regExpLen > len ? len + 1 : 0;
            }
            static inline int hexToInteger(const uint8 * s) { return (charMap[s[0]] & 0xF0) | (charMap[s[1]] >> 4); }

            int matchOperand(const uint8 * re, const uint8 * s)
            {
                switch (*re)
                {
                // If we reach those, the expression is malformed
                case '|': return SyntaxError;
                case '$': return NotFound;
                // Any char would do, let's accept it
                case '.': break;

                case '\\':
                    // Metacharacters
                    switch (re[1])
                    {
                    case 'S':
                        TestReturn((charMap[*s] & 4), NotFound);
                        break;

                    case 's':
                        TestReturn(!(charMap[*s] & 4), NotFound);
                        break;

                    case 'd':
                        TestReturn(!(charMap[*s] & 8), NotFound);
                        break;

                    case 'x':
                        // Match byte, \xHH where HH is hexadecimal byte representaion
                        TestReturn(hexToInteger(re + 2) != *s, NotFound);
                        break;

                    default:
                        // Valid metacharacter check is done in internalProcess()
                        TestReturn(re[1] != s[0], NotFound);
                        break;
                    }
                break;

                    // Any normal character
                default:
                    if (!caseSensitive)
                    {
                        TestReturn(tolower(*re) != tolower(*s), NotFound);
                    } else
                    {
                        TestReturn(*re != *s, NotFound);
                    }
                    break;
                }
                return 1;
            }

            // This is used to match a "[]" based set
            int matchSet(const char *re, int regExpLen, const char *s)
            {
                int len = 0, result = -1, invert = re[0] == '^';
                // If the set is inverted, eat the first char
                if (invert) re++,regExpLen--;

                // Inverted logic, result should stay 0 for matching
                while (len <= regExpLen && re[len] != ']' && result <= 0)
                {
                    // Check range
                    if (   (re[len] != '-' && re[len + 1] == '-' && re[len + 2] != ']')
                        && (re[len + 2] != '\0'))
                    {
                        result = !caseSensitive ? (*s >= re[len] && *s <= re[len + 2]) :
                                                  (tolower(*s) >= tolower(re[len]) && tolower(*s) <= tolower(re[len + 2]));
                        len += 3;
                    } else
                    {
                        result = matchOperand((uint8 * )re + len, (uint8 *)s);
                        len += operatorLen(re + len);
                    }
                }
                return (invert && result <= 0) || (!invert && result > 0) ? 1 : -1;
            }


            int internalProcess(const char *re, int regExpLen, const char *s, int s_len, int bracketIndex)
            {
                int reIndex = 0, strIndex = 0, n, step;

                for (reIndex = strIndex = 0; reIndex < regExpLen && strIndex <= s_len; reIndex += step)
                {
                    // Handle quantifiers. Get the length of the chunk.
                    step = re[reIndex] == '(' ? brackets[bracketIndex + 1].len + 2 : getOperatorLen(re + reIndex, regExpLen - reIndex);

                    TestReturn(charMap[(uint8)re[reIndex]] & 2, UnexpectedQuantifier);
                    TestReturn(step <= 0, BadSetOfCharacter);

                    // Is it a quantifier ?
                    if (reIndex + step < regExpLen && (charMap[(uint8)re[reIndex + step]] & 2))
                    {
                        if (re[reIndex + step] == '?')
                        {   // Zero or once
                            int result = internalProcess(re + reIndex, step, s + strIndex, s_len - strIndex, bracketIndex);
                            strIndex += result > 0 ? result : 0; // Silent failure
                            reIndex++;
                        } else if (re[reIndex + step] == '+' || re[reIndex + step] == '*')
                        {   // Zero|One or more
                            bool greedy = true;
                            int strMatchEndIndex = strIndex, strEndIndex = strIndex, strFirstMatchIndex, strSecondMatchIndex = -1, reEndIndex;

                            // Check for greedyness mode
                            reEndIndex = reIndex + step + 1;
                            if (reEndIndex < regExpLen && re[reEndIndex] == '?')
                            {
                                greedy = false;
                                reEndIndex++;
                            }

                            do
                            {
                                // Match the quantified expression only once
                                if ((strFirstMatchIndex = internalProcess(re + reIndex, step, s + strMatchEndIndex, s_len - strMatchEndIndex, bracketIndex)) > 0)
                                    strMatchEndIndex += strFirstMatchIndex;

                                // Is only one match required (and this one failed) ?
                                if (re[reIndex + step] == '+' && strFirstMatchIndex < 0) break;

                                // Check if we have something after quantifier
                                if (reEndIndex >= regExpLen)
                                    // Nothing left in expression after quantifier, let's try to eat as much as possible
                                    strEndIndex = strMatchEndIndex;
                                // Try to match the rest of the expression after the quantifier
                                else if ((strSecondMatchIndex = internalProcess(re + reEndIndex, regExpLen - reEndIndex, s + strMatchEndIndex, s_len - strMatchEndIndex, bracketIndex)) >= 0)
                                    // Matched after quantifier
                                    strEndIndex = strMatchEndIndex + strSecondMatchIndex;

                                // Continue if greedy and something else to eat
                                if (strEndIndex > strIndex && !greedy) break;
                            } while (strFirstMatchIndex > 0);

                            // Try zero match too (try to match the rest of the expression after quantifier)
                            if (strFirstMatchIndex < 0 && re[reIndex + step] == '*' && strSecondMatchIndex < 0 &&
                                (strSecondMatchIndex = internalProcess(re + reEndIndex, regExpLen - reEndIndex, s + strIndex, s_len - strIndex, bracketIndex)) > 0)
                                strEndIndex = strIndex + strSecondMatchIndex;

                            TestReturn(re[reIndex + step] == '+' && strEndIndex == strIndex, NotFound);

                            // Check the rest of the regex in case of zero match (ie: on *)
                            TestReturn(strEndIndex == strIndex && reEndIndex < regExpLen && strSecondMatchIndex < 0, NotFound);

                            // Expression matched completely
                            return strEndIndex;
                        }
                        continue;
                    }
                    n = 0;
                    switch(re[reIndex])
                    {
                        // Is a set ?
                        case '[':
                            n = matchSet(re + reIndex + 1, regExpLen - (reIndex + 2), s + strIndex);
                            TestReturn(n <= 0, NotFound);
                            break;
                        // Is a bracket ?
                        case '(':
                            n = NotFound;
                            bracketIndex++;
                            TestReturn(bracketIndex >= bracketsCount, SyntaxError);

                            if (regExpLen - (reIndex + step) <= 0)
                            {
                                // Nothing after the bracket
                                n = processBranch(s + strIndex, s_len - strIndex, bracketIndex);
                            } else
                            {
                                // Need to process the branch, and then the remaining expression
                                int j2;
                                for (j2 = 0; j2 <= s_len - strIndex; j2++)
                                {
                                    if (   (n = processBranch(s + strIndex, s_len - (strIndex + j2), bracketIndex)) >= 0
                                        && internalProcess(re + reIndex + step, regExpLen - (reIndex + step), s + strIndex + n, s_len - (strIndex + n), bracketIndex) >= 0)
                                        break;
                                }
                            }

                            TestReturn(n < 0, n);
                            if (caps != NULL && n > 0)
                            {
                                caps[bracketIndex - 1].ptr = s + strIndex;
                                caps[bracketIndex - 1].len = n;
                            }
                            break;
                        // Begin of section
                        case '^':
                            TestReturn(strIndex != 0, NotFound);
                            break;
                        // End of section
                        case '$':
                            TestReturn(strIndex != s_len, NotFound);
                            break;
                        default:
                            TestReturn(strIndex >= s_len, NotFound);
                            n = matchOperand((uint8 * ) (re + reIndex), (uint8 * ) (s + strIndex));
                            TestReturn(n <= 0, n);
                            break;
                    }
                    strIndex += n;
                }

                return strIndex;
            }

            // Process branch points
            int processBranch(const char *s, int s_len, int branchIndex)
            {
               const BracketPair * b = &brackets[branchIndex];
               int i = 0, len, result;
               const char *p;

               do
               {
                   p = i == 0 ? b->ptr : branches[b->branchIndex + i - 1].branchPos + 1;
                   len = b->branchesCount == 0 ? b->len : (i == b->branchesCount ? b->ptr + b->len - p : branches[b->branchIndex + i].branchPos - p);
                   result = internalProcess(p, len, s, s_len, branchIndex);
               } while (result <= 0 && i++ < b->branchesCount);  // At least 1 iteration

               return result;
            }

            int process(const char *s, int s_len)
            {
                int result = -1;
                bool isAnchored = brackets[0].ptr[0] == '^';

                for (int i = 0; i <= s_len; i++)
                {
                    result = processBranch(s + i, s_len - i, 0);
                    if (result >= 0)
                    {   // Completed search
                        firstMatchPos = i;
                        result += i;
                        break;
                    }
                    if (isAnchored) break;
                }

                return result;
            }

            void setupBranchPoints()
            {
               int i, j;
               Branch tmp;

               // Sort branches first based on bracketIndex
               for (i = 0; i < branchesCount; i++)
               {
                   for (j = i + 1; j < branchesCount; j++)
                   {
                       if (branches[i].bracketIndex > branches[j].bracketIndex)
                       {
                           tmp = branches[i];
                           branches[i] = branches[j];
                           branches[j] = tmp;
                       }
                   }
               }

               // Sort brackets too to know beforehand where (and how many branches) the branches points
               for (i = j = 0; i < bracketsCount; i++)
               {
                   brackets[i].branchesCount = 0;
                   brackets[i].branchIndex = j;
                   while (j < branchesCount && branches[j].bracketIndex == i)
                   {
                       brackets[i].branchesCount++;
                       j++;
                   }
               }
            }

            int analyze(const char *re, int regExpLen, const char *s, int s_len, int & capsRequired)
            {
                int step = 0, depth = 0;

                // Capture the complete expression in the first bracket
                TestReturn(!addBrackets(re, regExpLen), NotEnoughMemory);

                // Compute the number of brackets and branches required, and test basic validity for the expression
                for (int i = 0; i < regExpLen; i += step)
                {
                    step = getOperatorLen(re + i, regExpLen - i);

                    if (re[i] == '|')
                    {
                        TestReturn(!addBranch(brackets[bracketsCount - 1].len == -1 ? bracketsCount - 1 : depth, re+i), NotEnoughMemory);
                    } else if (re[i] == '\\')
                    {
                        TestReturn(i >= regExpLen - 1, BadMetaCharacter);
                        if (re[i + 1] == 'x')
                        {
                            // Hex digit specification must follow hexadecimal numbers
                            TestReturn(re[i + 1] == 'x' && i >= regExpLen - 3, BadMetaCharacter);
                            TestReturn(re[i + 1] == 'x' && !((charMap[(uint8)re[i + 2]] & 0xF8) && (charMap[(uint8)re[i + 3]] & 0xF8)), BadMetaCharacter);
                        } else
                        {
                            TestReturn((RegexpInfo::charMap[(uint8)re[i + 1]] & 1) == 0, BadMetaCharacter);
                        }
                    } else if (re[i] == '(')
                    {
                        // Need to increment the depth to set the correct index on matching parenthesis
                        depth++;
                        TestReturn(!addBrackets(re+i+1, -1), NotEnoughMemory);
                        capsRequired++;
                        TestReturn(s && capsCount > 0 && bracketsCount - 1 > capsCount, CapturesArrayTooSmall);
                    } else if (re[i] == ')')
                    {
                        // Fix index
                        int ind = brackets[bracketsCount - 1].len == -1 ? bracketsCount - 1 : depth;
                        brackets[ind].len = &re[i] - brackets[ind].ptr;
                        depth--;
                        TestReturn(depth < 0, UnbalancedBrackets);
                        // Shortcut bad expression failure
                        TestReturn(i > 0 && re[i - 1] == '(', NotFound);
                    }
                }

                TestReturn(depth != 0, UnbalancedBrackets);
                setupBranchPoints();

                return 0;
            }
        };

        // Stores : bit 0 = metacharacter
        //          bit 1 = quantifier
        //          bit 2 = space
        //          bit 3 = digit

        //          bit 4-7 = hex/decimal value
        uint8 RegexpInfo::charMap[] =
        {
0 /*   0 */, 0 /*   1 */, 0 /*   2 */, 0 /*   3 */, 0 /*   4 */, 0 /*   5 */, 0 /*   6 */, 0 /*   7 */, 0 /*   8 */, 4 /*   9 */, 4 /*  10 */, 4 /*  11 */, 4 /*  12 */, 4 /*  13 */, 0 /*  14 */, 0 /*  15 */,
0 /*  16 */, 0 /*  17 */, 0 /*  18 */, 0 /*  19 */, 0 /*  20 */, 0 /*  21 */, 0 /*  22 */, 0 /*  23 */, 0 /*  24 */, 0 /*  25 */, 0 /*  26 */, 0 /*  27 */, 0 /*  28 */, 0 /*  29 */, 0 /*  30 */, 0 /*  31 */,
4 /* ' ' */, 0 /* '!' */, 0 /* '"' */, 0 /* '#' */, 1 /* '$' */, 0 /* '%' */, 0 /* '&' */, 0 /* ''' */, 1 /* '(' */, 1 /* ')' */, 3 /* '*' */, 3 /* '+' */, 0 /* ',' */, 0 /* '-' */, 1 /* '.' */, 0 /* '/' */,
8 /* '0' */, 0x18 /* '1' */, 0x28 /* '2' */, 0x38 /* '3' */, 0x48 /* '4' */, 0x58 /* '5' */, 0x68 /* '6' */, 0x78 /* '7' */, 0x88 /* '8' */, 0x98 /* '9' */, 0 /* ':' */, 0 /* ';' */, 0 /* '<' */, 0 /* '=' */, 0 /* '>' */, 3 /* '?' */,
0 /* '@' */, 0xA0 /* 'A' */, 0xB0 /* 'B' */, 0xC0 /* 'C' */, 0xD0 /* 'D' */, 0xE0 /* 'E' */, 0xF0 /* 'F' */, 0 /* 'G' */, 0 /* 'H' */, 0 /* 'I' */, 0 /* 'J' */, 0 /* 'K' */, 0 /* 'L' */, 0 /* 'M' */, 0 /* 'N' */, 0 /* 'O' */,
0 /* 'P' */, 0 /* 'Q' */, 0 /* 'R' */, 1 /* 'S' */, 0 /* 'T' */, 0 /* 'U' */, 0 /* 'V' */, 0 /* 'W' */, 0 /* 'X' */, 0 /* 'Y' */, 0 /* 'Z' */, 1 /* '[' */, 1 /* '\' */, 1 /* ']' */, 1 /* '^' */, 0 /* '_' */,
0 /* '`' */, 0xA0 /* 'a' */, 0xB0 /* 'b' */, 0xC0 /* 'c' */, 0xD1 /* 'd' */, 0xE0 /* 'e' */, 0xF0 /* 'f' */, 0 /* 'g' */, 0 /* 'h' */, 0 /* 'i' */, 0 /* 'j' */, 0 /* 'k' */, 0 /* 'l' */, 0 /* 'm' */, 0 /* 'n' */, 0 /* 'o' */,
0 /* 'p' */, 0 /* 'q' */, 0 /* 'r' */, 1 /* 's' */, 0 /* 't' */, 0 /* 'u' */, 0 /* 'v' */, 0 /* 'w' */, 0 /* 'x' */, 0 /* 'y' */, 0 /* 'z' */, 0 /* '{' */, 1 /* '|' */, 0 /* '}' */, 0 /* '~' */, 0 /* '' */,
        };

        static int RegExpMatch(const char *regexp, const char *s, int s_len, Cap * caps, int capsCount, int & capsRequired, bool caseSensitive, int * matchPos = 0)
        {
            if (memcmp(regexp, "(?i)", 4) == 0)
            {
                caseSensitive = false;
                regexp += 4;
            }
            RegexpInfo info(caps, capsCount, caseSensitive);
            capsRequired = 0;
            int res = info.analyze(regexp, strlen(regexp), s, s_len, capsRequired);
            if (res != 0) return res;
            if (!s) return 0;
            res = info.process(s, s_len);
            if (matchPos) *matchPos = info.firstMatchPos;
            return res;
        }
    }
    // Match the expression against the compiled regular expression
    int String::regExMatch(const String & regEx, String * captures, int & capCount, const bool caseSensitive, int * matchPos) const
    {
        // If no captures, run a trial run to get the amount of captures required
        if (!captures)
            return RegExp::RegExpMatch(regEx, (const char*)0, 0, 0, 0, capCount, caseSensitive);

        RegExp::Cap * caps = new RegExp::Cap[capCount];
        if (!caps) return NotEnoughMemory;
        memset(caps, 0, capCount * sizeof(*caps));

        int res = RegExp::RegExpMatch(regEx, (const char*)data, slen, caps, capCount, capCount, caseSensitive, matchPos);
        for (int i = 0; i < capCount; i++)
            captures[i] = String(caps[i].ptr, caps[i].len);
        delete[] caps;
        return res;
    }
    // Fit the regular expression for this string
    bool String::regExFit(const String & regEx, const bool caseSensitive, void ** captOpt, int * captCount) const
    {
        int localCap = 0; void * localOpt = 0;
        int & capturesCount = captCount ? *captCount : localCap;
        void * & capturesOpt = captOpt ? *captOpt : localOpt;
        if (!capturesCount || !capturesOpt)
        {
            if (RegExp::RegExpMatch(regEx, (const char*)0, 0, 0, 0, capturesCount, caseSensitive) < 0) return false;
            // Allocate the buffer now
            capturesOpt = malloc(capturesCount * sizeof(RegExp::Cap));
        }
        // Cast the buffer to something we can support
        RegExp::Cap * caps = (RegExp::Cap *)capturesOpt;
        if (!caps) return false;
        memset(caps, 0, capturesCount * sizeof(*caps));

        int req = 0;
        int res = RegExp::RegExpMatch(regEx, (const char*)data, slen, caps, capturesCount, req, caseSensitive);
        if (&capturesOpt == &localOpt) free(localOpt);
        return res >= 0;
    }

    // Return a string with value replaced as regular expression
    String String::regExReplace(const String & regEx, const String & _with, const bool caseSensitive, int iterations) const
    {
        int capCount = 0;
        int res = RegExp::RegExpMatch(regEx, (const char*)0, 0, 0, 0, capCount, caseSensitive);
        if (res) return "";

        RegExp::Cap * caps = new RegExp::Cap[capCount + 1];
        if (!caps) return "";

        int processedData = 0;
        String ret;
        while(iterations--)
        {
            memset(caps, 0, capCount * sizeof(*caps));
            String with = _with;
            int matchPos = 0;
            res = RegExp::RegExpMatch(regEx, (const char*)data + processedData, slen - processedData, caps+1, capCount, capCount, caseSensitive, &matchPos);
            if (res <= 0) break;

            caps[0].ptr = (const char*)data + processedData + matchPos; caps[0].len = res - processedData - matchPos;

            ret += midString(processedData, matchPos) + with.splitUpTo("\\");
            while (with.slen)
            {
                int noMoreDigitPos = with.invFindAnyChar("0123456789");
                if (noMoreDigitPos == 0) { ret += '\\'; }
                else if (noMoreDigitPos == -1) { noMoreDigitPos = with.getLength(); }

                if (noMoreDigitPos > 0)
                {
                    int pos = with.midString(0, noMoreDigitPos).parseInt(10);
                    if (pos <= capCount)
                    {
                        ret += String(caps[pos].ptr, caps[pos].len);
                        with.splitAt(noMoreDigitPos);
                    }
                    else
                    {
                        // Output the slashed text
                        ret += '\\'; ret += with.data[0];
                        with.splitAt(1);
                    }
                }
                // Output the rest
                ret += with.splitUpTo("\\");
            }
            processedData += res;
        }
        // Copy the remaining part
        ret += midString(processedData, slen);
        delete[] caps;
        return ret;
    }
#endif
} // namespace Bstrlib
