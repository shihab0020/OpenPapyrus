// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/*
**********************************************************************
* Copyright (c) 2004-2011, International Business Machines
* Corporation and others.  All Rights Reserved.
**********************************************************************
* Author: Alan Liu
* Created: March 22 2004
* Since: ICU 3.0
**********************************************************************
*/
#ifndef __ICU_INTLTEST_TOKITER__
#define __ICU_INTLTEST_TOKITER__

#include "intltest.h"

class TextFile;

/**
 * An iterator class that returns successive string tokens from some
 * source.  String tokens are, in general, separated by Pattern_White_Space
 * in the source test.  Furthermore, they may be delimited by
 * either single or double quotes (opening and closing quotes must
 * match).  Escapes are processed using standard ICU unescaping.
 */
class TokenIterator {
 public:

    /**
  * Construct an iterator over the tokens returned by the given
  * TextFile, ignoring blank lines and comment lines (first
  * non-blank character is '#').  Note that trailing comments on a
  * line, beginning with the first unquoted '#', are recognized.
     */
    TokenIterator(TextFile* r);

    virtual ~TokenIterator();

    /**
  * Return the next token from this iterator.
  * @return TRUE if a token was read, or FALSE if no more tokens
  * are available or an error occurred.
     */
    bool next(UnicodeString & token, UErrorCode & ec);

    /**
  * Return the one-based line number of the line of the last token
  * returned by next(). Should only be called after a call to
  * next(); otherwise the return value is undefined.
     */
    int32_t getLineNumber() const;
    
    /**
  * Return a string description of the position of the last line
  * returned by readLine() or readLineSkippingComments().
     */
    //public String describePosition() {
    //    return reader.describePosition() + ':' + (lastpos+1);
    //}
    
 private:
    bool nextToken(UnicodeString & token, UErrorCode & ec);

    TextFile* reader; // alias
    UnicodeString line;
    bool done;
    bool haveLine;
    int32_t pos;
    int32_t lastpos;
};

#endif
