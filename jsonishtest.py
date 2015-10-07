#!/usr/bin/python
# -*- coding: utf-8 -*-

## this test suite is an almost verbatim copy of the jsontest.py test suite
## found in json-py available from http://sourceforge.net/projects/json-py/
##
## Copyright (C) 2005  Patrick D. Logan
## Contact mailto:patrickdlogan@stardecisions.com
##
## This library is free software; you can redistribute it and/or
## modify it under the terms of the GNU Lesser General Public
## License as published by the Free Software Foundation; either
## version 2.1 of the License, or (at your option) any later version.
##
## This library is distributed in the hope that it will be useful,
## but WITHOUT ANY WARRANTY; without even the implied warranty of
## MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
## Lesser General Public License for more details.
##
## You should have received a copy of the GNU Lesser General Public
## License along with this library; if not, write to the Free Software
## Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA

import os
import sys

import itertools
import unittest

import cjsonish
_exception = cjsonish.DecodeError

# The object tests should be order-independent. They're not.
# i.e. they should test for existence of keys and values
# with read/write invariance.

def _removeWhitespace(str):
    return str.replace(" ", "")

class JsonTest(unittest.TestCase):

    # *** [pdl]'s tests copiedish from jsontest.py.

    def testReadEmptyObject(self):
        obj = cjsonish.decode("{}")
        self.assertEqual({}, obj)

    def testWriteEmptyObject(self):
        s = cjsonish.encode({})
        self.assertEqual("{}", _removeWhitespace(s))

    def testReadStringValue(self):
        obj = cjsonish.decode('{ "name" : "Patrick" }')
        self.assertEqual({ "name" : "Patrick" }, obj)

    def testReadEscapedQuotationMark(self):
        obj = cjsonish.decode(r'"\""')
        self.assertEqual(r'"', obj)

    def testReadEscapedSolidus(self):
        obj = cjsonish.decode(r'"\/"')
        self.assertEqual(r'/', obj)

    def testReadEscapedReverseSolidus(self):
        obj = cjsonish.decode(r'"\\"')
        self.assertEqual('\\', obj)
    def testReadEscapedEndingQuote(self):
        self.assertRaises(cjsonish.DecodeError, self._testReadEscapedEndingQuote)
    def _testReadEscapedEndingQuote(self):
        cjsonish.decode('"\\"')

    def testReadEscapedBackspace(self):
        obj = cjsonish.decode(r'"\b"')
        self.assertEqual("\b", obj)

    def testReadEscapedFormfeed(self):
        obj = cjsonish.decode(r'"\f"')
        self.assertEqual("\f", obj)

    def testReadEscapedNewline(self):
        obj = cjsonish.decode(r'"\n"')
        self.assertEqual("\n", obj)

    def testReadEscapedCarriageReturn(self):
        obj = cjsonish.decode(r'"\r"')
        self.assertEqual("\r", obj)

    def testReadEscapedHorizontalTab(self):
        obj = cjsonish.decode(r'"\t"')
        self.assertEqual("\t", obj)

    def testReadEscapedHexCharacter(self):
        obj = cjsonish.decode(r'"\u000A"')
        self.assertEqual("\n", obj)
        obj = cjsonish.decode(r'"\u1001"')
        self.assertEqual(u'\u1001', obj)

    def testWriteEscapedQuotationMark(self):
        s = cjsonish.encode(r'"')
        self.assertEqual(r'"\""', _removeWhitespace(s))

    def testWriteEscapedSolidus(self):
        s = cjsonish.encode(r'/')
        self.assertEqual(r'"\/"', _removeWhitespace(s))
        # Same as: self.assertEqual('"\\/"', _removeWhitespace(s))

    def testWriteNonEscapedSolidus(self):
        s = cjsonish.encode(r'/')
        self.assertEqual('"\\/"', _removeWhitespace(s))

    def testWriteEscapedReverseSolidus(self):
        s = cjsonish.encode("\\")
        self.assertEqual(r'"\\"', _removeWhitespace(s))

    def testWriteEscapedBackspace(self):
        s = cjsonish.encode("\b")
        self.assertEqual(r'"\b"', _removeWhitespace(s))

    def testWriteEscapedFormfeed(self):
        if sys.version_info[0] >= 3:
            # Hrmm. Return gets interrupted as KeyboardInterrupt:
            #    File "jsonishtest.py", line 111, in testWriteEscapedFormfeed
            #      s = cjsonish.encode("\f")
            #  KeyboardInterrupt
            pass
        else:
            s = cjsonish.encode("\f")
            self.assertEqual(r'"\f"', _removeWhitespace(s))

    def testWriteEscapedNewline(self):
        s = cjsonish.encode("\n")
        self.assertEqual(r'"\n"', _removeWhitespace(s))

    def testWriteEscapedCarriageReturn(self):
        s = cjsonish.encode("\r")
        self.assertEqual(r'"\r"', _removeWhitespace(s))

    def testWriteEscapedHorizontalTab(self):
        s = cjsonish.encode("\t")
        self.assertEqual(r'"\t"', _removeWhitespace(s))

    def testWriteEscapedHexCharacter(self):
        s = cjsonish.encode(u'\u1001')
        if sys.version_info[0] >= 3:
            self.assertEqual(r'"ခ"', _removeWhitespace(s))
        else:
            #self.assertEqual(r'"\u1001"', _removeWhitespace(s))
            self.assertEqual(r'u"\u1001"', _removeWhitespace(s))

    def testReadBadEscapedHexCharacter(self):
        self.assertRaises(_exception, self.doReadBadEscapedHexCharacter)

    def doReadBadEscapedHexCharacter(self):
        cjsonish.decode(r'"\u10K5"')

    def testReadBadObjectKey(self):
        self.assertRaises(_exception, self.doReadBadObjectKey)

    def doReadBadObjectKey(self):
        cjsonish.decode('{ 44 : "age" }')

    def testReadBadArray(self):
        self.assertRaises(_exception, self.doReadBadArray)

    def doReadBadArray(self):
        cjsonish.decode('[1,2,3,,]')
        
    def testReadBadObjectSyntax(self):
        self.assertRaises(_exception, self.doReadBadObjectSyntax)

    def doReadBadObjectSyntax(self):
        cjsonish.decode('{"age", 44}')

    def testWriteStringValue(self):
        s = cjsonish.encode({ "name" : "Patrick" })
        self.assertEqual('{"name":"Patrick"}', _removeWhitespace(s))

    def testReadIntegerValue(self):
        obj = cjsonish.decode('{ "age" : 44 }')
        self.assertEqual({ "age" : 44 }, obj)

    def testReadNegativeIntegerValue(self):
        obj = cjsonish.decode('{ "key" : -44 }')
        self.assertEqual({ "key" : -44 }, obj)
        
    def testReadFloatValue(self):
        obj = cjsonish.decode('{ "age" : 44.5 }')
        self.assertEqual({ "age" : 44.5 }, obj)

    def testReadNegativeFloatValue(self):
        obj = cjsonish.decode(' { "key" : -44.5 } ')
        self.assertEqual({ "key" : -44.5 }, obj)

    def testReadBadNumber(self):
        self.assertRaises(_exception, self.doReadBadNumber)

    def doReadBadNumber(self):
        cjsonish.decode('-44.4.4')

    def testReadSmallObject(self):
        obj = cjsonish.decode('{ "name" : "Patrick", "age":44} ')
        self.assertEqual({ "age" : 44, "name" : "Patrick" }, obj)        

    def testReadEmptyArray(self):
        obj = cjsonish.decode('[]')
        self.assertEqual([], obj)

    def testWriteEmptyArray(self):
        self.assertEqual("[]", _removeWhitespace(cjsonish.encode([])))

    def testReadSmallArray(self):
        obj = cjsonish.decode(' [ "a" , "b", "c" ] ')
        self.assertEqual(["a", "b", "c"], obj)

    def testWriteSmallArray(self):
        self.assertEqual('[1,2,3,4]', _removeWhitespace(cjsonish.encode([1, 2, 3, 4])))

    def testWriteSmallObject(self):
        s = cjsonish.encode({ "name" : "Patrick", "age": 44 })
        # HA! This is a hack.
        self.assertTrue(
            _removeWhitespace(s) in [
                '{"name":"Patrick","age":44}',
                '{"age":44,"name":"Patrick"}',
            ]
        )

    def testWriteFloat(self):
        n = 3.44556677
        self.assertEqual(repr(n), _removeWhitespace(cjsonish.encode(n)))

    def testReadTrue(self):
        self.assertEqual(True, cjsonish.decode("true"))

    def testReadFalse(self):
        self.assertEqual(False, cjsonish.decode("false"))

    def testReadNull(self):
        self.assertEqual(None, cjsonish.decode("null"))

    def testWriteTrue(self):
        self.assertEqual("true", _removeWhitespace(cjsonish.encode(True)))

    def testWriteFalse(self):
        self.assertEqual("false", _removeWhitespace(cjsonish.encode(False)))

    def testWriteNull(self):
        self.assertEqual("null", _removeWhitespace(cjsonish.encode(None)))

    def testReadArrayOfSymbols(self):
        self.assertEqual([True, False, None], cjsonish.decode(" [ true, false,null] "))

    def testWriteArrayOfSymbolsFromList(self):
        self.assertEqual("[true,false,null]", _removeWhitespace(cjsonish.encode([True, False, None])))

    def testWriteArrayOfSymbolsFromTuple(self):
        self.assertEqual("[true,false,null]", _removeWhitespace(cjsonish.encode((True, False, None))))

    def testReadComplexObject(self):
        src = '''
    { "name": "Patrick", "age" : 44, "Employed?" : true, "Female?" : false, "grandchildren":null }
'''
        obj = cjsonish.decode(src)
        self.assertEqual({"name":"Patrick","age":44,"Employed?":True,"Female?":False,"grandchildren":None}, obj)

    def testReadLongArray(self):
        src = '''[    "used",
    "abused",
    "confused",
    true, false, null,
    1,
    2,
    [3, 4, 5]]
'''
        obj = cjsonish.decode(src)
        self.assertEqual(["used","abused","confused", True, False, None,
                          1,2,[3,4,5]], obj)

    def testReadIncompleteArray(self):
        self.assertRaises(_exception, self.doReadIncompleteArray)

    def doReadIncompleteArray(self):
        cjsonish.decode('[')

    def testReadComplexArray(self):
        src = '''
[
    { "name": "Patrick", "age" : 44,
      "Employed?" : true, "Female?" : false,
      "grandchildren":null },
    "used",
    "abused",
    "confused",
    1,
    2,
    [3, 4, 5]
]
'''
        obj = cjsonish.decode(src)
        self.assertEqual([{"name":"Patrick","age":44,"Employed?":True,"Female?":False,"grandchildren":None},
                          "used","abused","confused",
                          1,2,[3,4,5]], obj)

    def testWriteComplexArray(self):
        obj = [{"name":"Patrick","age":44,"Employed?":True,"Female?":False,"grandchildren":None},
               "used","abused","confused",
               1,2,[3,4,5]]
        # HA! This is a hack: Programmatically generate the list of
        # acceptable answers, since order is not predictable.
        kvals = [
            '"age":44',
            '"Female?":false',
            '"name":"Patrick"',
            '"Employed?":true',
            '"grandchildren":null',
        ]
        acceptable_answers = set([
            ('[{%s},"used","abused","confused",1,2,[3,4,5]]' % ','.join(x))
            for x in itertools.permutations(kvals)
        ])
        self.assertTrue(_removeWhitespace(cjsonish.encode(obj)) in acceptable_answers)

    def testReadWriteCopies(self):
        orig_obj = {'a':' " '}
        json_str = cjsonish.encode(orig_obj)
        copy_obj = cjsonish.decode(json_str)
        self.assertEqual(orig_obj, copy_obj)
        self.assertEqual(True, orig_obj == copy_obj)
        self.assertEqual(False, orig_obj is copy_obj)

    def testStringEncoding(self):
        s = cjsonish.encode([1, 2, 3])
        if sys.version_info[0] >= 3:
            encoded = "[1,2,3]"
        else:
            encoded = unicode("[1,2,3]", "utf-8")
        self.assertEqual(encoded, _removeWhitespace(s))

    def testReadEmptyObjectAtEndOfArray(self):
        self.assertEqual(["a","b","c",{}],
                         cjsonish.decode('["a","b","c",{}]'))

    def testReadEmptyObjectMidArray(self):
        self.assertEqual(["a","b",{},"c"],
                         cjsonish.decode('["a","b",{},"c"]'))

    def testReadClosingObjectBracket(self):
        self.assertEqual({"a":[1,2,3]}, cjsonish.decode('{"a":[1,2,3]}'))

    def testEmptyObjectInList(self):
        obj = cjsonish.decode('[{}]')
        self.assertEqual([{}], obj)

    def testObjectWithEmptyList(self):
        obj = cjsonish.decode('{"test": [] }')
        self.assertEqual({"test":[]}, obj)

    def testObjectWithNonEmptyList(self):
        obj = cjsonish.decode('{"test": [3, 4, 5] }')
        self.assertEqual({"test":[3, 4, 5]}, obj)

    def testWriteLong(self):
        self.assertEqual("12345678901234567890", cjsonish.encode(12345678901234567890))

    def testWriteLongUnicode(self):
        # This test causes a buffer overrun in cjsonish 1.0.5, on UCS4 builds.
        # The string length is only resized for wide unicode characters if
        # there is less than 12 bytes of space left. Padding with
        # narrow-but-escaped characters prevents string resizing.
        # Note that u'\U0001D11E\u1234' also breaks, but sometimes goes
        # undetected.
        s = cjsonish.encode(u'\U0001D11E\U0001D11E\U0001D11E\U0001D11E'
                         u'\u1234\u1234\u1234\u1234\u1234\u1234')
        if sys.version_info[0] >= 3:
            # Wha?
# FIXME: This has got to be wrong.......... or is this just unicode output?
            self.assertEqual(
                '"𝄞𝄞𝄞𝄞ሴሴሴሴሴሴ"'
                , s
            )
        else:
            #self.assertEqual(r'"\U0001d11e\U0001d11e\U0001d11e\U0001d11e'
            #                 r'\u1234\u1234\u1234\u1234\u1234\u1234"', s)
            self.assertEqual(r'u"\U0001d11e\U0001d11e\U0001d11e\U0001d11e'
                             r'\u1234\u1234\u1234\u1234\u1234\u1234"', s)

    # *** [lb]'s cjsonish tests.

    def testObjectWithTrailingCommaAndComment(self):
        obj = cjsonish.decode('{"a":123,} // nothing')
        self.assertEqual({"a": 123}, obj)

    def testObjectWithDashAndTrailingCommaAndComment(self):
        obj = cjsonish.decode('{"a-b": 123,} // nothing')
        self.assertEqual({"a-b": 123}, obj)

    def testObjectWithEmDashAndTrailingCommaAndComment(self):
        obj = cjsonish.decode('{"a–b":123,} // nothing')
        self.assertEqual({"a–b": 123}, obj)

    def testObjectWithUuencodedEmDashAndTrailingCommaAndComment(self):
        obj = cjsonish.decode('{"a\–b":123,} // nothing')
        # FIXME/EXPLAIN: Is cjsonish really decoding this correctly? Seems weird.
        self.assertEqual({"a\\u2013b": 123}, obj)

    def testObjectWithBackslashAndEmDashAndTrailingCommaAndMLComment(self):
        obj = cjsonish.decode('{"a\\–b":123,} /* nothing   */ \r\n')
        # NOTE: Because of how \ works, sometimes \ and \\ are the same:
        #       The string that Python reads and passes to cjosnish interprets
        #       \ and \\ as the same: just one backslash.
        self.assertEqual({'a\\u2013b': 123}, obj)

    def testObjectWithBackslashAndEndOfString(self):
        self.assertRaises(cjsonish.DecodeError, self._testObjectWithBackslashAndEndOfString)
    def _testObjectWithBackslashAndEndOfString(self):
        # This gets interpreted as a string key ('a": ')
        # missing a colon (and instead finds a stray 'x').
        cjsonish.decode('{"a\\": "x"}')

    def testObjectWithCRNewlineAndCommentAndNewlineAndListTuple(self):
        self.assertRaises(cjsonish.DecodeError, self._testObjectWithCRNewlineAndCommentAndNewlineAndListTuple)
    def _testObjectWithCRNewlineAndCommentAndNewlineAndListTuple(self):
        cjsonish.decode('{"a":null, \r    // nothing  \r"tup":(1,"a",True,),\r  }')

    def testObjectWithCRNewlineAndCommentAndNewlineAndListListAndCapitalizedTrue(self):
        self.assertRaises(
            cjsonish.DecodeError,
            self._testObjectWithCRNewlineAndCommentAndNewlineAndListListAndCapitalizedTrue
        )
    def _testObjectWithCRNewlineAndCommentAndNewlineAndListListAndCapitalizedTrue(self):
        cjsonish.decode('{"a":null, \r    // nothing  \r"tup":[1,"a",True,],\r  }')

    def testObjectWithCRNewlineAndCommentAndNewlineAndListListAndLowercaseTrue(self):
        obj = cjsonish.decode('{"a":null, \r    // nothing  \r"tup":[1,"a",true,],\r  }')
        self.assertEqual({"a": None, "tup": [1, "a", True],}, obj)

    def testObjectWithoutLeadingZeroInNumber(self):
        obj = cjsonish.decode('{"a":.123,} // nothing')
        self.assertEqual({"a": 0.123,}, obj)

    def testObjectWithEscapeLineContinuationsLoose(self):
        obj = cjsonish.decode('{"string": "blah blah \\\n more blahs \\\r\n",} // nothing')
        self.assertEqual({"string": "blah blah  more blahs ",}, obj)

    def testObjectWithEscapeLineContinuations(self):
        self.assertRaises(cjsonish.DecodeError, self._testObjectWithEscapeLineContinuations)
    def _testObjectWithEscapeLineContinuations(self):
        cjsonish.decode('{"string": "blah blah \\\n more blahs \\\r\n",} // nothing', strict=True)

    def testDecodeWithNewLinesLoose_01(self):
        self.assertRaises(cjsonish.DecodeError, self._testDecodeWithNewLinesLoose_01)
    def _testDecodeWithNewLinesLoose_01(self):
        cjsonish.decode('{"string": "blah blah \n more blahs \r\n",} // nothing')

    def testDecodeWithNewLinesLoose_02(self):
        # cjsonish accects newlines but they have to be escaped
        # (you can't just hit Enter in the middle of typing a string).
        obj = cjsonish.decode('{"string": "blah blah \\n more blahs \\r\\n",} // nothing')
        self.assertEqual({"string": "blah blah \n more blahs \r\n",}, obj)

    def testDecodeWithNewLinesStrict(self):
        self.assertRaises(cjsonish.DecodeError, self._testDecodeWithNewLinesStrict)
    def _testDecodeWithNewLinesStrict(self):
        cjsonish.decode('{"string": "blah blah \n more blahs \r\n"}', strict=True)

    def testObjectBasicString_01(self):
        obj = cjsonish.decode(r'"\\"')
        self.assertEqual('\\', obj)
        obj = cjsonish.decode("\"\\\\\"")
        self.assertEqual('\\', obj)

    def testObjectBasicString_02(self):
        obj = cjsonish.decode('"\"')
        # This reduces to the empty string because '"\"' is interpreted
        # by Python as '""'.
        self.assertEqual('', obj)
    def testObjectBasicString_03(self):
        self.assertRaises(cjsonish.DecodeError, self._testObjectBasicString_03)
    def _testObjectBasicString_03(self):
        cjsonish.decode('"\\"')

    def testObjectBasicString_04(self):
        obj = cjsonish.encode("\f")
        self.assertEqual('"\\f"', obj)

    def testDecodeBasicList(self):
        obj = cjsonish.decode(" [ true, false,null] ")
        self.assertEqual([True, False, None], obj)

    def testEncodeStringEscapes(self):
        # FIXME: How do you account for ordering?
        #  obj = cjsonish.encode({"a\"b": 'zz', "22": (1,2),})
        # because it's one of these:
        #  self.assertEqual('{"a\\"b": "zz", "22": [1, 2]}', obj)
        #  self.assertEqual('{"22": [1, 2], "a\\"b": "zz"}', obj)
        obj = cjsonish.encode({"a\"b": 'zz'})
        self.assertEqual('{"a\\"b": "zz"}', obj)
        obj = cjsonish.encode({"22": (1,2),})
        self.assertEqual('{"22": [1, 2]}', obj)

    def testEncodeUnicodeStringLeader(self):
        obj = cjsonish.encode([u'xx','yy'])
        self.assertEqual('["xx", "yy"]', obj)

    def testDecodeSolidus_01(self):
        obj = cjsonish.decode('{"string": "\/",}')
        self.assertEqual({'string': '/'}, obj)

    def testDecodeSolidus_02(self):
        obj = cjsonish.decode('{"string": "o\/g",}')
        self.assertEqual({'string': 'o/g'}, obj)

    def testDecodeSolidus_03(self):
        obj = cjsonish.decode('{"string": "hello\/goodbye",}')
        self.assertEqual({'string': 'hello/goodbye'}, obj)

    def testDecodeSolidus_04(self):
        obj = cjsonish.decode('{"string": "hello/goodbye",}')
        self.assertEqual({'string': 'hello/goodbye'}, obj)

    def testEncodeSolidus_01(self):
        obj = cjsonish.encode("{'string': 'hello/goodbye'}")
        self.assertEqual('"{\'string\': \'hello\\/goodbye\'}"', obj)

    def testEncodeSolidus_02(self):
        obj = cjsonish.encode("{'string': 'hello\/goodbye'}")
        # NOTE: This might look wrong and you might think it should be: 'hello\\/goodbye'
        #       But [lb]'s understanding of the spec. is that \/ is for
        #       decoding: it's stripped on input, so if \/ is being encoded,
        #       it's a backslash which we need to escape, followed by a
        #       solidus which also needs to be escaped.
        self.assertEqual('"{\'string\': \'hello\\\\\\/goodbye\'}"', obj)

    def testDecodeStringEscapedSolidusAndTrailingComma(self):
        self.assertRaises(cjsonish.DecodeError, self._testDecodeStringEscapedSolidusAndTrailingComma)
    def _testDecodeStringEscapedSolidusAndTrailingComma(self):
        cjsonish.decode('{"string": "hello\/goodbye",}', strict=True)

    def testDecodeStringEscapedSolidusAndNoTrailingComma(self):
        obj = cjsonish.decode('{"string": "hello\/goodbye"}', strict=True)
        self.assertEqual({"string": "hello/goodbye",}, obj)

    def testDecodeObjectWithTrailingOnelineComment(self):
        self.assertRaises(cjsonish.DecodeError, self._testDecodeObjectWithTrailingOnelineComment)
    def _testDecodeObjectWithTrailingOnelineComment(self):
        cjsonish.decode('{"string": "blah blah more blahs "} // nothing', strict=True)

    def testDecodeLineContinuationsAndOtherEscapes(self):
        obj = cjsonish.decode('{"x\t\\\/": "a green \\\r cow \t mooed \f oh heavens \b\b\b",}')
        self.assertEqual({'x\t\\/': 'a green  cow \t mooed \x0c oh heavens \x08\x08\x08'}, obj)

def main():
    unittest.main()

if __name__ == '__main__':
    main()

# vim:tw=0:ts=4:sw=4:et

