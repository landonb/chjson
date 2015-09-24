// File: cjsonish.c
// Author: Landon Bouma (landonb &#x40; retrosoft &#x2E; com)
// Last Modified: 2015.09.24
// Project Page: https://github.com/landonb/cjsonish
// Original Code: Copyright (C) 2006-2007 Dan Pascu <dan@ag-projects.com>
// License: GPLv3
// Description: Loose JSON encoder/decoder Python C extension.
// vim:tw=0:ts=4:sw=4:et

#include <Python.h>
#include <stddef.h>
#include <stdio.h>
#include <ctype.h>
#include <math.h>
#include <signal.h> // To set breakpoints with: raise(SIGINT);

typedef struct JSONData {
    char *str; // the actual json string
    char *end; // pointer to the string end
    char *ptr; // pointer to the current parsing position
    int  all_unicode; // make all output strings unicode if true
} JSONData;

static PyObject* encode_object(PyObject *object);
#if PY_MAJOR_VERSION < 3
static PyObject* encode_string(PyObject *object);
#endif
static PyObject* encode_unicode(PyObject *object);
static PyObject* encode_tuple(PyTupleObject *object);
static PyObject* encode_list(PyListObject *object);
static PyObject* encode_dict(PyDictObject *object);

static PyObject* decode_json(JSONData *jsondata);
static PyObject* decode_null(JSONData *jsondata);
static PyObject* decode_bool(JSONData *jsondata);
static PyObject* decode_string(JSONData *jsondata);
static PyObject* decode_inf(JSONData *jsondata);
static PyObject* decode_nan(JSONData *jsondata);
static PyObject* decode_number(JSONData *jsondata);
static PyObject* decode_array(JSONData *jsondata);
static PyObject* decode_object(JSONData *jsondata);

static PyObject *JSON_Error;
static PyObject *JSON_EncodeError;
static PyObject *JSON_DecodeError;

#define _string(x) #x
#define string(x) _string(x)

#if PY_VERSION_HEX < 0x02050000
typedef int Py_ssize_t;
#define PY_SSIZE_T_MAX INT_MAX
#define PY_SSIZE_T_MIN INT_MIN

#define SSIZE_T_F "%d"
#else
#define SSIZE_T_F "%zd"
#endif

#define True  1
#define False 0

#ifndef INFINITY
#define INFINITY HUGE_VAL
#endif

#ifndef NAN
#define NAN (HUGE_VAL - HUGE_VAL)
#endif

#ifndef Py_IS_NAN
#define Py_IS_NAN(X) ((X) != (X))
#endif

// Py2to3 macros.

#define skipSpaces(d) while(isspace(*((d)->ptr))) (d)->ptr++;

#if PY_MAJOR_VERSION >= 3
    #define MOD_ERROR_VAL NULL
    #define MOD_SUCCESS_VAL(val) val
    #define MOD_INIT(name) PyMODINIT_FUNC PyInit_##name(void)
    #define MOD_DEF(ob, name, methods, doc) \
        ob = PyModule_Create(&cjsonish_moduledef);
#else
    #define MOD_ERROR_VAL
    #define MOD_SUCCESS_VAL(val)
    #define MOD_INIT(name) PyMODINIT_FUNC init##name(void)
    #define MOD_DEF(ob, name, methods, doc) \
        ob = Py_InitModule3(name, methods, doc);
#endif

#if PY_MAJOR_VERSION >= 3
    #define PyInt_Check PyLong_Check
    #define PyInt_FromString PyLong_FromUnicode
    //#define PyString_Check PyBytes_Check
    #define PyString_Check PyUnicode_Check
#endif

// ------------------------------ Decoding -----------------------------

static PyObject*
decode_null(JSONData *jsondata)
{
    ptrdiff_t left;

    left = jsondata->end - jsondata->ptr;

    if ((left >= 4) && (strncmp(jsondata->ptr, "null", 4) == 0)) {
        jsondata->ptr += 4;
        Py_INCREF(Py_None);
        return Py_None;
    }
    else {
        PyErr_Format(
            JSON_DecodeError,
            "cannot parse JSON description: %.20s",
            jsondata->ptr
        );
        return NULL;
    }
}

static PyObject*
decode_bool(JSONData *jsondata)
{
    ptrdiff_t left;

    left = jsondata->end - jsondata->ptr;

    if ((left >= 4) && (strncmp(jsondata->ptr, "true", 4) == 0)) {
        jsondata->ptr += 4;
        Py_INCREF(Py_True);
        return Py_True;
    }
    else if ((left >= 5) && (strncmp(jsondata->ptr, "false", 5) == 0)) {
        jsondata->ptr += 5;
        Py_INCREF(Py_False);
        return Py_False;
    }
    else {
        PyErr_Format(
            JSON_DecodeError,
            "cannot parse JSON description: %.20s",
            jsondata->ptr
        );
        return NULL;
    }
}

static PyObject*
decode_string(JSONData *jsondata)
{
    PyObject *object;
    int c, escaping, has_unicode, string_escape;
    Py_ssize_t len;
    char *ptr;

    // look for the closing quote
    escaping = has_unicode = string_escape = False;
    ptr = jsondata->ptr + 1;
    while (True) {
        c = *ptr;
        if (c == 0) {
            PyErr_Format(
                JSON_DecodeError,
                "unterminated string starting at position " SSIZE_T_F,
                (Py_ssize_t)(jsondata->ptr - jsondata->str)
            );
            return NULL;
        }
        if (!escaping) {
            if (c == '\\') {
                escaping = True;
            }
            else if (c == '"') {
                break;
            }
            else if (!isascii(c)) {
                has_unicode = True;
            }
        }
        else {
            switch(c) {
            case 'u':
                has_unicode = True;
                break;
            case '"':
            case 'r':
            case 'n':
            case 't':
            case 'b':
            case 'f':
            case '\\':
                string_escape = True;
                break;
            }
            escaping = False;
        }
        ptr++;
    }

    len = ptr - jsondata->ptr - 1;

    if (has_unicode || jsondata->all_unicode) {
        object = PyUnicode_DecodeUnicodeEscape(jsondata->ptr+1, len, NULL);
    }
    else if (string_escape) {
#if PY_MAJOR_VERSION >= 3
        PyObject *obj = PyBytes_DecodeEscape(jsondata->ptr+1, len, NULL, 0, NULL);
        object = PyUnicode_FromEncodedObject(obj, /*encoding=*/NULL, /*errors=*/NULL);
        Py_DECREF(obj);
        if (object == NULL) {
            // MAYBE: Set error object?
            return NULL;
        }
#else
        object = PyString_DecodeEscape(jsondata->ptr+1, len, NULL, 0, NULL);
#endif
    }
    else {
#if PY_MAJOR_VERSION >= 3
        object = PyUnicode_FromStringAndSize(jsondata->ptr+1, len);
#else
        object = PyString_FromStringAndSize(jsondata->ptr+1, len);
#endif
    }

    if (object == NULL) {
        PyObject *type, *value, *tb, *reason;

        PyErr_Fetch(&type, &value, &tb);
        if (type == NULL) {
            PyErr_Format(
                JSON_DecodeError,
                "invalid string starting at position " SSIZE_T_F,
                (Py_ssize_t)(jsondata->ptr - jsondata->str)
            );
        } else {
            if (PyErr_GivenExceptionMatches(type, PyExc_UnicodeDecodeError)) {
                reason = PyObject_GetAttrString(value, "reason");
                PyErr_Format(
                    JSON_DecodeError,
                    "cannot decode string starting at position " SSIZE_T_F ": %s",
                    (Py_ssize_t)(jsondata->ptr - jsondata->str),
#if PY_MAJOR_VERSION >= 3
                    //reason ? PyBytes_AsString(reason) : "bad format");
                    reason ? PyUnicode_AsUTF8(reason) : "bad format"
#else
                    reason ? PyString_AsString(reason) : "bad format"
#endif
                );
                Py_XDECREF(reason);
            }
            else {
                PyErr_Format(
                    JSON_DecodeError,
                    "invalid string starting at position " SSIZE_T_F,
                    (Py_ssize_t)(jsondata->ptr - jsondata->str)
                );
            }
        }
        Py_XDECREF(type);
        Py_XDECREF(value);
        Py_XDECREF(tb);
    }
    else {
        jsondata->ptr = ptr+1;
    }

    return object;
}

static PyObject*
decode_inf(JSONData *jsondata)
{
    PyObject *object;
    ptrdiff_t left;

    left = jsondata->end - jsondata->ptr;

    if ((left >= 8) && (strncmp(jsondata->ptr, "Infinity", 8) == 0)) {
        jsondata->ptr += 8;
        object = PyFloat_FromDouble(INFINITY);
        return object;
    }
    else if ((left >= 9) && (strncmp(jsondata->ptr, "+Infinity", 9) == 0)) {
        jsondata->ptr += 9;
        object = PyFloat_FromDouble(INFINITY);
        return object;
    }
    else if ((left >= 9) && (strncmp(jsondata->ptr, "-Infinity", 9) == 0)) {
        jsondata->ptr += 9;
        object = PyFloat_FromDouble(-INFINITY);
        return object;
    }
    else {
        PyErr_Format(
            JSON_DecodeError,
            "cannot parse JSON description: %.20s",
            jsondata->ptr
        );
        return NULL;
    }
}

static PyObject*
decode_nan(JSONData *jsondata)
{
    PyObject *object;
    ptrdiff_t left;

    left = jsondata->end - jsondata->ptr;

    if ((left >= 3) && (strncmp(jsondata->ptr, "NaN", 3) == 0)) {
        jsondata->ptr += 3;
        object = PyFloat_FromDouble(NAN);
        return object;
    }
    else {
        PyErr_Format(
            JSON_DecodeError,
            "cannot parse JSON description: %.20s",
            jsondata->ptr
        );
        return NULL;
    }
}

#define skipDigits(ptr) while(isdigit(*(ptr))) (ptr)++

static PyObject*
decode_number(JSONData *jsondata)
{
    PyObject *object, *str;
    int is_float;
    char *ptr;

    // validate number and check if it's floating point or not
    ptr = jsondata->ptr;
    is_float = False;

    if (*ptr == '-' || *ptr == '+') {
        ptr++;
    }

    if (*ptr == '0') {
        ptr++;
        if (isdigit(*ptr)) {
            goto number_error;
        }
    }
    else if (isdigit(*ptr)) {
        skipDigits(ptr);
    }
    else {
        goto number_error;
    }

    if (*ptr == '.') {
       is_float = True;
       ptr++;
       if (!isdigit(*ptr)) {
           goto number_error;
       }
       skipDigits(ptr);
    }

    if (*ptr == 'e' || *ptr == 'E') {
       is_float = True;
       ptr++;
       if (*ptr == '+' || *ptr == '-') {
           ptr++;
       }
       if (!isdigit(*ptr)) {
           goto number_error;
       }
       skipDigits(ptr);
    }

#if PY_MAJOR_VERSION >= 3
    //str = PyBytes_FromStringAndSize(jsondata->ptr, ptr - jsondata->ptr);
    str = PyUnicode_FromStringAndSize(jsondata->ptr, ptr - jsondata->ptr);
#else
    str = PyString_FromStringAndSize(jsondata->ptr, ptr - jsondata->ptr);
#endif
    if (str == NULL) {
        return NULL;
    }

    if (is_float) {
        #if PY_MAJOR_VERSION >= 3
            object = PyFloat_FromString(str);
        #else
            object = PyFloat_FromString(str, NULL);
        #endif
    }
    else {
        #if PY_MAJOR_VERSION >= 3
            object = PyInt_FromString(PyUnicode_AS_UNICODE(str), Py_SIZE(str), 10);
        #else
            object = PyInt_FromString(PyBytes_AS_STRING(str), NULL, 10);
        #endif
    }

    Py_DECREF(str);

    if (object == NULL) {
        goto number_error;
    }

    jsondata->ptr = ptr;

    return object;

number_error:
    PyErr_Format(
        JSON_DecodeError,
        "invalid number starting at position " SSIZE_T_F,
        (Py_ssize_t)(jsondata->ptr - jsondata->str)
    );
    return NULL;
}

typedef enum {
    ArrayItem_or_ClosingBracket=0,
    Comma_or_ClosingBracket,
    ArrayItem,
    ArrayDone
} ArrayState;

static PyObject*
decode_array(JSONData *jsondata)
{
    PyObject *object, *item;
    ArrayState next_state;
    int c, result;
    char *start;

    object = PyList_New(0);

    start = jsondata->ptr;
    jsondata->ptr++;

    next_state = ArrayItem_or_ClosingBracket;

    while (next_state != ArrayDone) {
        skipSpaces(jsondata);
        c = *jsondata->ptr;
        if (c == 0) {
            PyErr_Format(
                JSON_DecodeError,
                "unterminated array starting at position " SSIZE_T_F,
                (Py_ssize_t)(start - jsondata->str)
            );
            goto failure;
        }
        switch (next_state) {
        case ArrayItem_or_ClosingBracket:
            if (c == ']') {
                jsondata->ptr++;
                next_state = ArrayDone;
                break;
            }
        case ArrayItem:
            if ((c == ',') || (c == ']')) {
                PyErr_Format(
                    JSON_DecodeError,
                    "expecting array item at position " SSIZE_T_F,
                    (Py_ssize_t)(jsondata->ptr - jsondata->str)
                );
                goto failure;
            }
            item = decode_json(jsondata);
            if (item == NULL) {
                goto failure;
            }
            result = PyList_Append(object, item);
            Py_DECREF(item);
            if (result == -1) {
                goto failure;
            }
            next_state = Comma_or_ClosingBracket;
            break;
        case Comma_or_ClosingBracket:
            if (c == ']') {
                jsondata->ptr++;
                next_state = ArrayDone;
            }
            else if (c == ',') {
                jsondata->ptr++;
                // cjsonish: Allow trailing comma.
                //next_state = ArrayItem;
                next_state = ArrayItem_or_ClosingBracket;
            }
            else {
                PyErr_Format(
                    JSON_DecodeError,
                    "expecting ',' or ']' at position " SSIZE_T_F,
                    (Py_ssize_t)(jsondata->ptr - jsondata->str)
                );
                goto failure;
            }
            break;
        case ArrayDone:
            // this will never be reached, but keep compilers happy
            break;
        }
    }

    return object;

failure:
    Py_DECREF(object);
    return NULL;
}

typedef enum {
    DictionaryKey_or_ClosingBrace=0,
    Comma_or_ClosingBrace,
    DictionaryKey,
    DictionaryDone
} DictionaryState;

static PyObject*
decode_object(JSONData *jsondata)
{
    PyObject *object, *key, *value;
    DictionaryState next_state;
    int c, result;
    char *start;

    object = PyDict_New();

    start = jsondata->ptr;
    jsondata->ptr++;

    next_state = DictionaryKey_or_ClosingBrace;

    while (next_state != DictionaryDone) {
        skipSpaces(jsondata);
        c = *jsondata->ptr;
        if (c == 0) {
            PyErr_Format(
                JSON_DecodeError,
                "unterminated object starting at position " SSIZE_T_F,
                (Py_ssize_t)(start - jsondata->str)
            );
            goto failure;;
        }

        switch (next_state) {
        case DictionaryKey_or_ClosingBrace:
            if (c == '}') {
                jsondata->ptr++;
                next_state = DictionaryDone;
                break;
            }
        case DictionaryKey:
            if (c != '"') {
                PyErr_Format(
                    JSON_DecodeError,
                    "expecting object property name at position " SSIZE_T_F,
                    (Py_ssize_t)(jsondata->ptr - jsondata->str)
                );
                goto failure;
            }

            key = decode_json(jsondata);
            if (key == NULL) {
                goto failure;
            }

            skipSpaces(jsondata);
            if (*jsondata->ptr != ':') {
                PyErr_Format(
                    JSON_DecodeError,
                    "missing colon after object property name at position " SSIZE_T_F,
                    (Py_ssize_t)(jsondata->ptr - jsondata->str)
                );
                Py_DECREF(key);
                goto failure;
            }
            else {
                jsondata->ptr++;
            }

            skipSpaces(jsondata);
            if ((*jsondata->ptr == ',') || (*jsondata->ptr == '}')) {
                PyErr_Format(
                    JSON_DecodeError,
                    "expecting object property value at position " SSIZE_T_F,
                    (Py_ssize_t)(jsondata->ptr - jsondata->str)
                );
                Py_DECREF(key);
                goto failure;
            }

            value = decode_json(jsondata);
            if (value == NULL) {
                Py_DECREF(key);
                goto failure;
            }

            result = PyDict_SetItem(object, key, value);
            Py_DECREF(key);
            Py_DECREF(value);
            if (result == -1) {
                goto failure;
            }
            next_state = Comma_or_ClosingBrace;
            break;
        case Comma_or_ClosingBrace:
            if (c == '}') {
                jsondata->ptr++;
                next_state = DictionaryDone;
            }
            else if (c == ',') {
                jsondata->ptr++;
                // cjsonish: Allow trailing comma.
                //next_state = DictionaryKey;
                next_state = DictionaryKey_or_ClosingBrace;
            }
            else {
                PyErr_Format(
                    JSON_DecodeError,
                    "expecting ',' or '}' at position " SSIZE_T_F,
                    (Py_ssize_t)(jsondata->ptr - jsondata->str)
                );
                goto failure;
            }
            break;
        case DictionaryDone:
            // this will never be reached, but keep compilers happy
            break;
        }
    }

    return object;

failure:
    Py_DECREF(object);
    return NULL;
}

static PyObject*
decode_json(JSONData *jsondata)
{
    PyObject *object;

    skipSpaces(jsondata);
    switch(*jsondata->ptr) {
    case 0:
        PyErr_SetString(JSON_DecodeError, "empty JSON description");
        return NULL;
    case '{':
        object = decode_object(jsondata);
        break;
    case '[':
        object = decode_array(jsondata);
        break;
    case '"':
        object = decode_string(jsondata);
        break;
    case 't':
    case 'f':
        object = decode_bool(jsondata);
        break;
    case 'n':
        object = decode_null(jsondata);
        break;
    case 'N':
        object = decode_nan(jsondata);
        break;
    case 'I':
        object = decode_inf(jsondata);
        break;
    case '+':
    case '-':
        if (*(jsondata->ptr+1) == 'I') {
            object = decode_inf(jsondata);
            break;
        }
        // fall through
    case '0':
    case '1':
    case '2':
    case '3':
    case '4':
    case '5':
    case '6':
    case '7':
    case '8':
    case '9':
        object = decode_number(jsondata);
        break;
    default:
        PyErr_SetString(JSON_DecodeError, "cannot parse JSON description");
        return NULL;
    }

    return object;
}

// ------------------------------ Encoding -----------------------------

#if PY_MAJOR_VERSION < 3
// This function is an almost verbatim copy of PyString_Repr() from
// Python's stringobject.c with the following differences:
// - it always quotes the output using double quotes.
// - it also quotes \b and \f
// - it replaces any non ASCII character hh with \u00hh instead of \xhh
/* From older Python2:
static PyObject*
encode_string(PyObject *string)
{
    register PyStringObject* op = (PyStringObject*) string;
    size_t newsize = 2 + 6 * op->ob_size;
    PyObject *v;

    if (op->ob_size > (PY_SSIZE_T_MAX-2)/6) {
        PyErr_SetString(
            PyExc_OverflowError,
            "string is too large to make repr"
        );
        return NULL;
    }
    v = PyString_FromStringAndSize((char *)NULL, newsize);
    if (v == NULL) {
        return NULL;
    }
    else {
        register Py_ssize_t i;
        register char c;
        register char *p;
        int quote;

        quote = '"';

        p = PyString_AS_STRING(v);
        *p++ = quote;
        for (i = 0; i < op->ob_size; i++) {
            // There's at least enough room for a hex escape
            // and a closing quote.
            assert(newsize - (p - PyString_AS_STRING(v)) >= 7);
            c = op->ob_sval[i];
            if (c == quote || c == '\\') {
                *p++ = '\\', *p++ = c;
            }
            else if (c == '\t') {
                *p++ = '\\', *p++ = 't';
            }
            else if (c == '\n') {
                *p++ = '\\', *p++ = 'n';
            }
            else if (c == '\r') {
                *p++ = '\\', *p++ = 'r';
            }
            else if (c == '\f') {
                *p++ = '\\', *p++ = 'f';
            }
            else if (c == '\b') {
                *p++ = '\\', *p++ = 'b';
            }
            else if (c < ' ' || c >= 0x7f) {
                // For performance, we don't want to call
                // PyOS_snprintf here (extra layers of
                // function call).
                sprintf(p, "\\u%04x", c & 0xff);
                p += 6;
            }
            else {
                *p++ = c;
            }
        }
        assert(newsize - (p - PyString_AS_STRING(v)) >= 1);
        *p++ = quote;
        *p = '\0';
        _PyString_Resize(&v, (int) (p - PyString_AS_STRING(v)));
        return v;
    }
}
*/
// From Python-2.7.10:
PyObject *
//PyString_Repr(PyObject *obj, int smartquotes)
encode_string(PyObject *obj)
{
    register PyStringObject* op = (PyStringObject*) obj;
    size_t newsize;
    PyObject *v;
    if (Py_SIZE(op) > (PY_SSIZE_T_MAX - 2)/4) {
        PyErr_SetString(
            PyExc_OverflowError,
            "string is too large to make repr"
        );
        return NULL;
    }
    newsize = 2 + 4*Py_SIZE(op);
    v = PyString_FromStringAndSize((char *)NULL, newsize);
    if (v == NULL) {
        return NULL;
    }
    else {
        register Py_ssize_t i;
        register char c;
        register char *p;
        int quote;

        // OC:
        //  figure out which quote to use; single is preferred
        //  quote = '\'';
        //  if (smartquotes
        //      && memchr(op->ob_sval, '\'', Py_SIZE(op))
        //      && !memchr(op->ob_sval, '"', Py_SIZE(op))
        //  ) {
        //      quote = '"';
        //  }
        quote = '"';

        p = PyString_AS_STRING(v);
        *p++ = quote;
        for (i = 0; i < Py_SIZE(op); i++) {
            // There's at least enough room for a hex escape
            // and a closing quote.
            assert(newsize - (p - PyString_AS_STRING(v)) >= 5);
            c = op->ob_sval[i];
            if (c == quote || c == '\\') {
                *p++ = '\\', *p++ = c;
            }
            else if (c == '\t') {
                *p++ = '\\', *p++ = 't';
            }
            else if (c == '\n') {
                *p++ = '\\', *p++ = 'n';
            }
            else if (c == '\r') {
                *p++ = '\\', *p++ = 'r';
            }
            // cjsonish additional:
            else if (c == '\f') {
                *p++ = '\\', *p++ = 'f';
            }
            else if (c == '\b') {
                *p++ = '\\', *p++ = 'b';
            }
            // OC:
            else if (c < ' ' || c >= 0x7f) {
                // For performance, we don't want to call
                // PyOS_snprintf here (extra layers of
                // function call).
                // OC:
                //  sprintf(p, "\\x%02x", c & 0xff);
                sprintf(p, "\\u%04x", c & 0xff);
                p += 4;
            }
            else {
                *p++ = c;
            }
        }
        assert(newsize - (p - PyString_AS_STRING(v)) >= 1);
        *p++ = quote;
        *p = '\0';
        if (_PyString_Resize(&v, (p - PyString_AS_STRING(v)))) {
            return NULL;
        }
        return v;
    }
}
#endif

// This function is a copy of Python-3.4.3/Objects/unicodeobject.c::unicode_repr()
// with the following differences:
// - It quotes \b and \f
// - It uses \u00hh instead of \xhh in output.
// - It always quotes the output using double quotes.
#if PY_MAJOR_VERSION >= 3
static PyObject *
//unicode_repr(PyObject *unicode)
encode_unicode(PyObject *unicode)
{
    PyObject *repr;
    Py_ssize_t isize;
    Py_ssize_t osize, squote, dquote, i, o;
    Py_UCS4 max, quote;
    int ikind, okind, unchanged;
    void *idata, *odata;

    if (PyUnicode_READY(unicode) == -1) {
        return NULL;
    }

    isize = PyUnicode_GET_LENGTH(unicode);
    idata = PyUnicode_DATA(unicode);

    // Compute length of output, quote characters, and maximum character
    osize = 0;
    max = 127;
    squote = dquote = 0;
    ikind = PyUnicode_KIND(unicode);
    for (i = 0; i < isize; i++) {
        Py_UCS4 ch = PyUnicode_READ(ikind, idata, i);
        Py_ssize_t incr = 1;
        switch (ch) {
        case '\'':
            squote++;
            break;
        case '"':
            dquote++;
            break;
        case '\\':
        case '\t':
        case '\r':
        case '\n':
        // cjsonish additional:
        case '\f':
        case '\b':
            incr = 2;
            break;
        default:
            // Fast-path ASCII
            if (ch < ' ' || ch == 0x7f) {
                incr = 4; // \xHH
            }
            else if (ch < 0x7f) {
                ;
            }
            else if (Py_UNICODE_ISPRINTABLE(ch)) {
                max = ch > max ? ch : max;
            }
            else if (ch < 0x100) {
                incr = 4; // \xHH
            }
            else if (ch < 0x10000) {
                incr = 6; // \uHHHH
            }
            else {
                incr = 10; // \uHHHHHHHH
            }
        }
        if (osize > PY_SSIZE_T_MAX - incr) {
            PyErr_SetString(
                PyExc_OverflowError, "string is too long to generate repr"
            );
            return NULL;
        }
        osize += incr;
    }

    // OC:
    //  quote = '\'';
    //  unchanged = (osize == isize);
    //  if (squote) {
    //      unchanged = 0;
    //      if (dquote) {
    //          // Both squote and dquote present. Use squote, and escape them
    //          osize += squote;
    //      }
    //      else {
    //          quote = '"';
    //      }
    //  }
    // cjonish always uses dquotes:
    quote = '"';
    unchanged = (osize == isize);
    if (dquote) {
        unchanged = 0;
        // dquotes are present. Add room for escape characters.
        osize += dquote;
    }

    osize += 2; // quotes

    repr = PyUnicode_New(osize, max);
    if (repr == NULL) {
        return NULL;
    }
    okind = PyUnicode_KIND(repr);
    odata = PyUnicode_DATA(repr);

    PyUnicode_WRITE(okind, odata, 0, quote);
    PyUnicode_WRITE(okind, odata, osize-1, quote);
    if (unchanged) {
        _PyUnicode_FastCopyCharacters(repr, 1,
                                      unicode, 0,
                                      isize);
    }
    else {
        for (i = 0, o = 1; i < isize; i++) {
            Py_UCS4 ch = PyUnicode_READ(ikind, idata, i);

            // Escape quotes and backslashes
            if ((ch == quote) || (ch == '\\')) {
                PyUnicode_WRITE(okind, odata, o++, '\\');
                PyUnicode_WRITE(okind, odata, o++, ch);
                continue;
            }

            // Map special whitespace to '\t', \n', '\r'
            if (ch == '\t') {
                PyUnicode_WRITE(okind, odata, o++, '\\');
                PyUnicode_WRITE(okind, odata, o++, 't');
            }
            else if (ch == '\n') {
                PyUnicode_WRITE(okind, odata, o++, '\\');
                PyUnicode_WRITE(okind, odata, o++, 'n');
            }
            else if (ch == '\r') {
                PyUnicode_WRITE(okind, odata, o++, '\\');
                PyUnicode_WRITE(okind, odata, o++, 'r');
            }
            // cjsonish additional:
            else if (ch == '\f') {
                PyUnicode_WRITE(okind, odata, o++, '\\');
                PyUnicode_WRITE(okind, odata, o++, 'f');
            }
            else if (ch == '\b') {
                PyUnicode_WRITE(okind, odata, o++, '\\');
                PyUnicode_WRITE(okind, odata, o++, 'b');
            }

            // Map non-printable US ASCII to '\xhh'
            else if (ch < ' ' || ch == 0x7F) {
                PyUnicode_WRITE(okind, odata, o++, '\\');
                // OC:
                //  PyUnicode_WRITE(okind, odata, o++, 'x');
                // cjsonish alternate:
                PyUnicode_WRITE(okind, odata, o++, 'u');
                PyUnicode_WRITE(okind, odata, o++, '0');
                PyUnicode_WRITE(okind, odata, o++, '0');
                PyUnicode_WRITE(okind, odata, o++, Py_hexdigits[(ch >> 4) & 0x000F]);
                PyUnicode_WRITE(okind, odata, o++, Py_hexdigits[ch & 0x000F]);
            }

            // Copy ASCII characters as-is
            else if (ch < 0x7F) {
                PyUnicode_WRITE(okind, odata, o++, ch);
            }

            // Non-ASCII characters
            else {
                // Map Unicode whitespace and control characters
                // (categories Z* and C* except ASCII space)
                if (!Py_UNICODE_ISPRINTABLE(ch)) {
                    PyUnicode_WRITE(okind, odata, o++, '\\');
                    // Map 8-bit characters to '\xhh'
                    if (ch <= 0xff) {
                        // OC:
                        //  PyUnicode_WRITE(okind, odata, o++, 'x');
                        // cjsonish alternate:
                        PyUnicode_WRITE(okind, odata, o++, 'u');
                        PyUnicode_WRITE(okind, odata, o++, '0');
                        PyUnicode_WRITE(okind, odata, o++, '0');
                        PyUnicode_WRITE(okind, odata, o++, Py_hexdigits[(ch >> 4) & 0x000F]);
                        PyUnicode_WRITE(okind, odata, o++, Py_hexdigits[ch & 0x000F]);
                    }
                    // Map 16-bit characters to '\uxxxx'
                    else if (ch <= 0xffff) {
                        PyUnicode_WRITE(okind, odata, o++, 'u');
                        PyUnicode_WRITE(okind, odata, o++, Py_hexdigits[(ch >> 12) & 0xF]);
                        PyUnicode_WRITE(okind, odata, o++, Py_hexdigits[(ch >> 8) & 0xF]);
                        PyUnicode_WRITE(okind, odata, o++, Py_hexdigits[(ch >> 4) & 0xF]);
                        PyUnicode_WRITE(okind, odata, o++, Py_hexdigits[ch & 0xF]);
                    }
                    // Map 21-bit characters to '\U00xxxxxx'
                    else {
                        PyUnicode_WRITE(okind, odata, o++, 'U');
                        PyUnicode_WRITE(okind, odata, o++, Py_hexdigits[(ch >> 28) & 0xF]);
                        PyUnicode_WRITE(okind, odata, o++, Py_hexdigits[(ch >> 24) & 0xF]);
                        PyUnicode_WRITE(okind, odata, o++, Py_hexdigits[(ch >> 20) & 0xF]);
                        PyUnicode_WRITE(okind, odata, o++, Py_hexdigits[(ch >> 16) & 0xF]);
                        PyUnicode_WRITE(okind, odata, o++, Py_hexdigits[(ch >> 12) & 0xF]);
                        PyUnicode_WRITE(okind, odata, o++, Py_hexdigits[(ch >> 8) & 0xF]);
                        PyUnicode_WRITE(okind, odata, o++, Py_hexdigits[(ch >> 4) & 0xF]);
                        PyUnicode_WRITE(okind, odata, o++, Py_hexdigits[ch & 0xF]);
                    }
                }
                // Copy characters as-is
                else {
                    PyUnicode_WRITE(okind, odata, o++, ch);
                }
            }
        }
    }
    // Closing quote already added at the beginning
    assert(_PyUnicode_CheckConsistency(repr, 1));
    return repr;
}
#else
// This function is an almost verbatim copy of unicodeescape_string() from
// Python's unicodeobject.c with the following differences:
//
// - it always quotes the output using double quotes.
// - it uses \u00hh instead of \xhh in output.
// - it also quotes \b and \f
/* From older Python2:
static PyObject*
encode_unicode(PyObject *unicode)
{
    PyObject *repr;
    Py_UNICODE *s;
    Py_ssize_t size;
    char *p;

    static const char *hexdigit = "0123456789abcdef";
#ifdef Py_UNICODE_WIDE
    const Py_ssize_t expandsize = 10;
#else
    const Py_ssize_t expandsize = 6;
#endif

    // Initial allocation is based on the longest-possible unichr
    // escape.
    //
    // In wide (UTF-32) builds '\U00xxxxxx' is 10 chars per source
    // unichr, so in this case it's the longest unichr escape. In
    // narrow (UTF-16) builds this is five chars per source unichr
    // since there are two unichrs in the surrogate pair, so in narrow
    // (UTF-16) builds it's not the longest unichr escape.
    //
    // In wide or narrow builds '\uxxxx' is 6 chars per source unichr,
    // so in the narrow (UTF-16) build case it's the longest unichr
    // escape.

    s = PyUnicode_AS_UNICODE(unicode);
    size = PyUnicode_GET_SIZE(unicode);

    if (size > (PY_SSIZE_T_MAX-2-1)/expandsize) {
        PyErr_SetString(PyExc_OverflowError,
                        "unicode object is too large to make repr");
        return NULL;
    }

    repr = PyString_FromStringAndSize(NULL, 2 + expandsize*size + 1);
    if (repr == NULL) {
        return NULL;
    }

    p = PyString_AS_STRING(repr);

    *p++ = '"';

    while (size-- > 0) {
        Py_UNICODE ch = *s++;

        // Escape quotes
        if ((ch == (Py_UNICODE) PyString_AS_STRING(repr)[0] || ch == '\\')) {
            *p++ = '\\';
            *p++ = (char) ch;
            continue;
        }

#ifdef Py_UNICODE_WIDE
        // Map 21-bit characters to '\U00xxxxxx'
        else if (ch >= 0x10000) {
            *p++ = '\\';
            *p++ = 'U';
            *p++ = hexdigit[(ch >> 28) & 0x0000000F];
            *p++ = hexdigit[(ch >> 24) & 0x0000000F];
            *p++ = hexdigit[(ch >> 20) & 0x0000000F];
            *p++ = hexdigit[(ch >> 16) & 0x0000000F];
            *p++ = hexdigit[(ch >> 12) & 0x0000000F];
            *p++ = hexdigit[(ch >> 8) & 0x0000000F];
            *p++ = hexdigit[(ch >> 4) & 0x0000000F];
            *p++ = hexdigit[ch & 0x0000000F];
            continue;
        }
#endif
        // Map UTF-16 surrogate pairs to Unicode \UXXXXXXXX escapes
        else if (ch >= 0xD800 && ch < 0xDC00) {
            Py_UNICODE ch2;
            Py_UCS4 ucs;

            ch2 = *s++;
            size--;
            if (ch2 >= 0xDC00 && ch2 <= 0xDFFF) {
                ucs = (((ch & 0x03FF) << 10) | (ch2 & 0x03FF)) + 0x00010000;
                *p++ = '\\';
                *p++ = 'U';
                *p++ = hexdigit[(ucs >> 28) & 0x0000000F];
                *p++ = hexdigit[(ucs >> 24) & 0x0000000F];
                *p++ = hexdigit[(ucs >> 20) & 0x0000000F];
                *p++ = hexdigit[(ucs >> 16) & 0x0000000F];
                *p++ = hexdigit[(ucs >> 12) & 0x0000000F];
                *p++ = hexdigit[(ucs >> 8) & 0x0000000F];
                *p++ = hexdigit[(ucs >> 4) & 0x0000000F];
                *p++ = hexdigit[ucs & 0x0000000F];
                continue;
            }
            // Fall through: isolated surrogates are copied as-is
            s--;
            size++;
        }

        // Map 16-bit characters to '\uxxxx'
        if (ch >= 256) {
            *p++ = '\\';
            *p++ = 'u';
            *p++ = hexdigit[(ch >> 12) & 0x000F];
            *p++ = hexdigit[(ch >> 8) & 0x000F];
            *p++ = hexdigit[(ch >> 4) & 0x000F];
            *p++ = hexdigit[ch & 0x000F];
        }

        // Map special whitespace to '\t', \n', '\r', '\f', '\b'
        else if (ch == '\t') {
            *p++ = '\\';
            *p++ = 't';
        }
        else if (ch == '\n') {
            *p++ = '\\';
            *p++ = 'n';
        }
        else if (ch == '\r') {
            *p++ = '\\';
            *p++ = 'r';
        }
        else if (ch == '\f') {
            *p++ = '\\';
            *p++ = 'f';
        }
        else if (ch == '\b') {
            *p++ = '\\';
            *p++ = 'b';
        }

        // Map non-printable US ASCII to '\u00hh'
        else if (ch < ' ' || ch >= 0x7F) {
            *p++ = '\\';
            *p++ = 'u';
            *p++ = '0';
            *p++ = '0';
            *p++ = hexdigit[(ch >> 4) & 0x000F];
            *p++ = hexdigit[ch & 0x000F];
        }

        // Copy everything else as-is
        else {
            *p++ = (char) ch;
        }
    }

    *p++ = PyString_AS_STRING(repr)[0];

    *p = '\0';
    _PyString_Resize(&repr, p - PyString_AS_STRING(repr));
    return repr;
}
*/
static PyObject *
//unicodeescape_string(const Py_UNICODE *s, Py_ssize_t size, int quotes)
encode_unicode(PyObject *unicode)
{
    PyObject *repr;
    char *p;

    static const char *hexdigit = "0123456789abcdef";
#ifdef Py_UNICODE_WIDE
    const Py_ssize_t expandsize = 10;
#else
    const Py_ssize_t expandsize = 6;
#endif

    // cjsonish:
    Py_UNICODE *s = PyUnicode_AS_UNICODE(unicode);
    // FIXME: Is this right? Use PyUnicode_GET_SIZE and not Py_SIZE?
    Py_ssize_t size = PyUnicode_GET_SIZE(unicode);
    //Py_ssize_t size = Py_SIZE(str);
    int quotes = 1;

    // XXX(nnorwitz): rather than over-allocating, it would be
    // better to choose a different scheme.  Perhaps scan the
    // first N-chars of the string and allocate based on that size.

    // Initial allocation is based on the longest-possible unichr
    // escape.
    //
    // In wide (UTF-32) builds '\U00xxxxxx' is 10 chars per source
    // unichr, so in this case it's the longest unichr escape. In
    // narrow (UTF-16) builds this is five chars per source unichr
    // since there are two unichrs in the surrogate pair, so in narrow
    // (UTF-16) builds it's not the longest unichr escape.
    //
    // In wide or narrow builds '\uxxxx' is 6 chars per source unichr,
    // so in the narrow (UTF-16) build case it's the longest unichr
    // escape.

    if (size > (PY_SSIZE_T_MAX - 2 - 1) / expandsize) {
        return PyErr_NoMemory();
    }

    repr = PyString_FromStringAndSize(NULL,
                                      2
                                      + expandsize*size
                                      + 1);
    if (repr == NULL) {
        return NULL;
    }

    p = PyString_AS_STRING(repr);

    if (quotes) {
        *p++ = 'u';
        // OC:
        //  *p++ = (findchar(s, size, '\'') && !findchar(s, size, '"')) ? '"' : '\'';
        *p++ = '"';
    }
    while (size-- > 0) {
        Py_UNICODE ch = *s++;

        // Escape quotes and backslashes
        if ((quotes && ch == (Py_UNICODE) PyString_AS_STRING(repr)[1])
            || (ch == '\\')
        ) {
            *p++ = '\\';
            *p++ = (char) ch;
            continue;
        }

#ifdef Py_UNICODE_WIDE
        // Map 21-bit characters to '\U00xxxxxx'
        else if (ch >= 0x10000) {
            *p++ = '\\';
            *p++ = 'U';
            *p++ = hexdigit[(ch >> 28) & 0x0000000F];
            *p++ = hexdigit[(ch >> 24) & 0x0000000F];
            *p++ = hexdigit[(ch >> 20) & 0x0000000F];
            *p++ = hexdigit[(ch >> 16) & 0x0000000F];
            *p++ = hexdigit[(ch >> 12) & 0x0000000F];
            *p++ = hexdigit[(ch >> 8) & 0x0000000F];
            *p++ = hexdigit[(ch >> 4) & 0x0000000F];
            *p++ = hexdigit[ch & 0x0000000F];
            continue;
        }
#else
        // Map UTF-16 surrogate pairs to '\U00xxxxxx'
        else if (ch >= 0xD800 && ch < 0xDC00) {
            Py_UNICODE ch2;
            Py_UCS4 ucs;

            ch2 = *s++;
            size--;
            if (ch2 >= 0xDC00 && ch2 <= 0xDFFF) {
                ucs = (((ch & 0x03FF) << 10) | (ch2 & 0x03FF)) + 0x00010000;
                *p++ = '\\';
                *p++ = 'U';
                *p++ = hexdigit[(ucs >> 28) & 0x0000000F];
                *p++ = hexdigit[(ucs >> 24) & 0x0000000F];
                *p++ = hexdigit[(ucs >> 20) & 0x0000000F];
                *p++ = hexdigit[(ucs >> 16) & 0x0000000F];
                *p++ = hexdigit[(ucs >> 12) & 0x0000000F];
                *p++ = hexdigit[(ucs >> 8) & 0x0000000F];
                *p++ = hexdigit[(ucs >> 4) & 0x0000000F];
                *p++ = hexdigit[ucs & 0x0000000F];
                continue;
            }
            // Fall through: isolated surrogates are copied as-is
            s--;
            size++;
        }
#endif

        // Map 16-bit characters to '\uxxxx'
        if (ch >= 256) {
            *p++ = '\\';
            *p++ = 'u';
            *p++ = hexdigit[(ch >> 12) & 0x000F];
            *p++ = hexdigit[(ch >> 8) & 0x000F];
            *p++ = hexdigit[(ch >> 4) & 0x000F];
            *p++ = hexdigit[ch & 0x000F];
        }

        // Map special whitespace to '\t', \n', '\r', '\f', '\b'
        else if (ch == '\t') {
            *p++ = '\\';
            *p++ = 't';
        }
        else if (ch == '\n') {
            *p++ = '\\';
            *p++ = 'n';
        }
        else if (ch == '\r') {
            *p++ = '\\';
            *p++ = 'r';
        }
        // cjsonish:
        else if (ch == '\f') {
            *p++ = '\\';
            *p++ = 'f';
        }
        else if (ch == '\b') {
            *p++ = '\\';
            *p++ = 'b';
        }

        // Map non-printable US ASCII to '\xhh'
        else if (ch < ' ' || ch >= 0x7F) {
            *p++ = '\\';
            // OC:
            //  *p++ = 'x';
            // cjsonish:
            *p++ = 'u';
            *p++ = '0';
            *p++ = '0';
            // OC:
            *p++ = hexdigit[(ch >> 4) & 0x000F];
            *p++ = hexdigit[ch & 0x000F];
        }

        // Copy everything else as-is
        else {
            *p++ = (char) ch;
        }
    }
    if (quotes) {
        *p++ = PyString_AS_STRING(repr)[1];
    }

    *p = '\0';
    if (_PyString_Resize(&repr, p - PyString_AS_STRING(repr))) {
        return NULL;
    }
    return repr;
}
#endif

// This function is a copy of Python-3.4.3/Objects/tupleobject.c::tuplerepr()
// with the following differences:
// - It uses encode_object() to get the object's JSON representation.
// - It uses [] as decorators instead of () (to masquerade as a JSON array).
#if PY_MAJOR_VERSION >= 3
static PyObject*
//tuplerepr(PyTupleObject *v)
encode_tuple(PyTupleObject *v)
{
    Py_ssize_t i, n;
    _PyUnicodeWriter writer;

    n = Py_SIZE(v);
    if (n == 0) {
        // OC:
        //  return PyUnicode_FromString("()");
        return PyUnicode_FromString("[]");
    }

    // While not mutable, it is still possible to end up with a cycle in a
    // tuple through an object that stores itself within a tuple (and thus
    // infinitely asks for the repr of itself). This should only be
    // possible within a type.
    i = Py_ReprEnter((PyObject *)v);
    if (i != 0) {
        // OC:
        //  return i > 0 ? PyUnicode_FromString("(...)") : NULL;
        if (i > 0) {
            PyErr_SetString(
                JSON_EncodeError,
                "a tuple with references to itself is not JSON encodable");
        }
        // else, i == -1; an expection occurred.
        return NULL;
    }

    _PyUnicodeWriter_Init(&writer);
    writer.overallocate = 1;
    if (Py_SIZE(v) > 1) {
        // "(" + "1" + ", 2" * (len - 1) + ")"
        writer.min_length = 1 + 1 + (2 + 1) * (Py_SIZE(v) - 1) + 1;
    }
    else {
        // "(1,)"
        writer.min_length = 4;
    }

    // OC:
    //  if (_PyUnicodeWriter_WriteChar(&writer, '(') < 0)
    if (_PyUnicodeWriter_WriteChar(&writer, '[') < 0) {
        goto error;
    }

    // Do repr() on each element.
    for (i = 0; i < n; ++i) {
        PyObject *s;

        if (i > 0) {
            if (_PyUnicodeWriter_WriteASCIIString(&writer, ", ", 2) < 0)
                goto error;
        }

        if (Py_EnterRecursiveCall(" while getting the repr of a tuple"))
            goto error;
        // OC:
        //  s = PyObject_Repr(v->ob_item[i]);
        s = encode_object(v->ob_item[i]);
        Py_LeaveRecursiveCall();
        if (s == NULL)
            goto error;

        if (_PyUnicodeWriter_WriteStr(&writer, s) < 0) {
            Py_DECREF(s);
            goto error;
        }
        Py_DECREF(s);
    }

    writer.overallocate = 0;
    if (n > 1) {
        // OC:
        //  if (_PyUnicodeWriter_WriteChar(&writer, ')') < 0)
        if (_PyUnicodeWriter_WriteChar(&writer, ']') < 0) {
            goto error;
        }
    }
    else {
        // OC:
        //  if (_PyUnicodeWriter_WriteASCIIString(&writer, ",)", 2) < 0)
        if (_PyUnicodeWriter_WriteASCIIString(&writer, ",]", 2) < 0) {
            goto error;
        }
    }

    Py_ReprLeave((PyObject *)v);
    return _PyUnicodeWriter_Finish(&writer);

error:
    _PyUnicodeWriter_Dealloc(&writer);
    Py_ReprLeave((PyObject *)v);
    return NULL;
}
#else
// This function is an almost verbatim copy of tuplerepr() from
// Python's tupleobject.c with the following differences:
//
// - it uses encode_object() to get the object's JSON reprezentation.
// - it uses [] as decorations instead of () (to masquerade as a JSON array).
/* From older Python2:
static PyObject*
encode_tuple(PyObject *tuple)
{
    Py_ssize_t i, n;
    PyObject *s, *temp;
    PyObject *pieces, *result = NULL;
    PyTupleObject *v = (PyTupleObject*) tuple;

    n = v->ob_size;
    if (n == 0)
        return PyString_FromString("[]");

    pieces = PyTuple_New(n);
    if (pieces == NULL)
        return NULL;

    // Do repr() on each element.
    for (i = 0; i < n; ++i) {
        s = encode_object(v->ob_item[i]);
        if (s == NULL)
            goto Done;
        PyTuple_SET_ITEM(pieces, i, s);
    }

    // Add "[]" decorations to the first and last items.
    assert(n > 0);
    s = PyString_FromString("[");
    if (s == NULL)
        goto Done;
    temp = PyTuple_GET_ITEM(pieces, 0);
    PyString_ConcatAndDel(&s, temp);
    PyTuple_SET_ITEM(pieces, 0, s);
    if (s == NULL)
        goto Done;

    s = PyString_FromString("]");
    if (s == NULL)
        goto Done;
    temp = PyTuple_GET_ITEM(pieces, n-1);
    PyString_ConcatAndDel(&temp, s);
    PyTuple_SET_ITEM(pieces, n-1, temp);
    if (temp == NULL)
        goto Done;

    // Paste them all together with ", " between.
    s = PyString_FromString(", ");
    if (s == NULL)
        goto Done;
    result = _PyString_Join(s, pieces);
    Py_DECREF(s);

Done:
    Py_DECREF(pieces);
    return result;
}
*/
static PyObject*
//tuplerepr(PyTupleObject *v)
encode_tuple(PyTupleObject *v)
{
    Py_ssize_t i, n;
    PyObject *s, *temp;
    PyObject *pieces, *result = NULL;

    n = Py_SIZE(v);
    if (n == 0) {
        // OC:
        //  return PyString_FromString("()");
        return PyString_FromString("[]");
    }

    // While not mutable, it is still possible to end up with a cycle in a
    // tuple through an object that stores itself within a tuple (and thus
    // infinitely asks for the repr of itself). This should only be
    // possible within a type.
    i = Py_ReprEnter((PyObject *)v);
    if (i != 0) {
        // OC:
        //  return i > 0 ? PyString_FromString("(...)") : NULL;
        if (i > 0) {
            PyErr_SetString(
                JSON_EncodeError,
                "a tuple with references to itself is not JSON encodable");
        }
        // else, i == -1; an expection occurred.
        return NULL;
    }

    pieces = PyTuple_New(n);
    if (pieces == NULL) {
        return NULL;
    }

    // Do repr() on each element.
    for (i = 0; i < n; ++i) {
        if (Py_EnterRecursiveCall(" while getting the repr of a tuple")) {
            goto Done;
        }
        // OC:
        //  s = PyObject_Repr(v->ob_item[i]);
        s = encode_object(v->ob_item[i]);
        Py_LeaveRecursiveCall();
        if (s == NULL) {
            goto Done;
        }
        PyTuple_SET_ITEM(pieces, i, s);
    }

    // Add "()" decorations to the first and last items.
    assert(n > 0);
    // OC:
    //  s = PyString_FromString("(");
    s = PyString_FromString("[");
    if (s == NULL) {
        goto Done;
    }
    temp = PyTuple_GET_ITEM(pieces, 0);
    PyString_ConcatAndDel(&s, temp);
    PyTuple_SET_ITEM(pieces, 0, s);
    if (s == NULL) {
        goto Done;
    }

    // OC:
    //  s = PyString_FromString(n == 1 ? ",)" : ")");
    s = PyString_FromString(n == 1 ? ",]" : "]");
    if (s == NULL) {
        goto Done;
    }
    temp = PyTuple_GET_ITEM(pieces, n-1);
    PyString_ConcatAndDel(&temp, s);
    PyTuple_SET_ITEM(pieces, n-1, temp);
    if (temp == NULL) {
        goto Done;
    }

    // Paste them all together with ", " between.
    s = PyString_FromString(", ");
    if (s == NULL) {
        goto Done;
    }
    result = _PyString_Join(s, pieces);
    Py_DECREF(s);

Done:
    Py_DECREF(pieces);
    Py_ReprLeave((PyObject *)v);
    return result;
}
#endif

// This function is a copy of Python-3.4.3/Objects/listobject.c::list_repr()
// with the following differences:
// - An element or sub-element of a list may not reference the list or any
//   containing parent. In normal, list_repr(), Python just prints ellipses;
//   in cjsonish, we raine an EncodeError.
// - We call our own encode_object() rather than Python's PyObject_Repr()
//   to serialize items, so we can override peculiarities as necessary.
#if PY_MAJOR_VERSION >= 3
static PyObject *
//list_repr(PyListObject *v)
encode_list(PyListObject *v)
{
    Py_ssize_t i;
    PyObject *s;
    _PyUnicodeWriter writer;

    if (Py_SIZE(v) == 0) {
        return PyUnicode_FromString("[]");
    }

    i = Py_ReprEnter((PyObject*)v);
    if (i != 0) {
        // OC:
        //  return i > 0 ? PyUnicode_FromString("[...]") : NULL;
        if (i > 0) {
            PyErr_SetString(
                JSON_EncodeError,
                "a list with references to itself is not JSON encodable");
        }
        // else, i == -1; an expection occurred.
        return NULL;
    }

    _PyUnicodeWriter_Init(&writer);
    writer.overallocate = 1;
    // "[" + "1" + ", 2" * (len - 1) + "]"
    writer.min_length = 1 + 1 + (2 + 1) * (Py_SIZE(v) - 1) + 1;

    if (_PyUnicodeWriter_WriteChar(&writer, '[') < 0)
        goto error;

    // Do repr() on each element. Note that this may mutate the list,
    // so must refetch the list size on each iteration.
    for (i = 0; i < Py_SIZE(v); ++i) {
        if (i > 0) {
            if (_PyUnicodeWriter_WriteASCIIString(&writer, ", ", 2) < 0)
                goto error;
        }

        // OC:
        //  if (Py_EnterRecursiveCall(" while getting the repr of a list"))
        //      goto error;
        //  s = PyObject_Repr(v->ob_item[i]);
        s = encode_object(v->ob_item[i]);
        Py_LeaveRecursiveCall();
        if (s == NULL)
            goto error;

        if (_PyUnicodeWriter_WriteStr(&writer, s) < 0) {
            Py_DECREF(s);
            goto error;
        }
        Py_DECREF(s);
    }

    writer.overallocate = 0;
    if (_PyUnicodeWriter_WriteChar(&writer, ']') < 0)
        goto error;

    Py_ReprLeave((PyObject *)v);
    return _PyUnicodeWriter_Finish(&writer);

error:
    _PyUnicodeWriter_Dealloc(&writer);
    Py_ReprLeave((PyObject *)v);
    return NULL;
}
#else
// This function is an almost verbatim copy of list_repr() from
// Python's listobject.c with the following differences:
// - it uses encode_object() to get the object's JSON reprezentation.
// - it doesn't use the ellipsis to represent a list with references
//   to itself, instead it raises an exception as such lists cannot be
//   represented in JSON.
/* From older Python2:
static PyObject*
encode_list(PyObject *list)
{
    Py_ssize_t i;
    PyObject *s, *temp;
    PyObject *pieces = NULL, *result = NULL;
    PyListObject *v = (PyListObject*) list;

    i = Py_ReprEnter((PyObject*)v);
    if (i != 0) {
        if (i > 0) {
            PyErr_SetString(JSON_EncodeError, "a list with references to "
                            "itself is not JSON encodable");
        }
        return NULL;
    }

    if (v->ob_size == 0) {
        result = PyString_FromString("[]");
        goto Done;
    }

    pieces = PyList_New(0);
    if (pieces == NULL)
        goto Done;

    // Do repr() on each element.  Note that this may mutate the list,
    // so must refetch the list size on each iteration.
    for (i = 0; i < v->ob_size; ++i) {
        int status;
        s = encode_object(v->ob_item[i]);
        if (s == NULL)
            goto Done;
        status = PyList_Append(pieces, s);
        Py_DECREF(s);  // append created a new ref
        if (status < 0)
            goto Done;
    }

    // Add "[]" decorations to the first and last items.
    assert(PyList_GET_SIZE(pieces) > 0);
    s = PyString_FromString("[");
    if (s == NULL)
        goto Done;
    temp = PyList_GET_ITEM(pieces, 0);
    PyString_ConcatAndDel(&s, temp);
    PyList_SET_ITEM(pieces, 0, s);
    if (s == NULL)
        goto Done;

    s = PyString_FromString("]");
    if (s == NULL)
        goto Done;
    temp = PyList_GET_ITEM(pieces, PyList_GET_SIZE(pieces) - 1);
    PyString_ConcatAndDel(&temp, s);
    PyList_SET_ITEM(pieces, PyList_GET_SIZE(pieces) - 1, temp);
    if (temp == NULL)
        goto Done;

    // Paste them all together with ", " between.
    s = PyString_FromString(", ");
    if (s == NULL)
        goto Done;
    result = _PyString_Join(s, pieces);
    Py_DECREF(s);

Done:
    Py_XDECREF(pieces);
    Py_ReprLeave((PyObject *)v);
    return result;
}
*/
static PyObject *
//list_repr(PyListObject *v)
encode_list(PyListObject *v)
{
    Py_ssize_t i;
    PyObject *s, *temp;
    PyObject *pieces = NULL, *result = NULL;

    i = Py_ReprEnter((PyObject*)v);
    if (i != 0) {
        // OC:
        //  return i > 0 ? PyString_FromString("[...]") : NULL;
        if (i > 0) {
            PyErr_SetString(JSON_EncodeError, "a list with references to "
                            "itself is not JSON encodable");
        }
        return NULL;
    }

    if (Py_SIZE(v) == 0) {
        result = PyString_FromString("[]");
        goto Done;
    }

    pieces = PyList_New(0);
    if (pieces == NULL) {
        goto Done;
    }

    // Do repr() on each element.  Note that this may mutate the list,
    // so must refetch the list size on each iteration.
    for (i = 0; i < Py_SIZE(v); ++i) {
        int status;
        if (Py_EnterRecursiveCall(" while getting the repr of a list")) {
            goto Done;
        }
        // OC:
        //  s = PyObject_Repr(v->ob_item[i]);
        s = encode_object(v->ob_item[i]);
        Py_LeaveRecursiveCall();
        if (s == NULL) {
            goto Done;
        }
        status = PyList_Append(pieces, s);
        Py_DECREF(s);  // append created a new ref
        if (status < 0) {
            goto Done;
        }
    }

    // Add "[]" decorations to the first and last items.
    assert(PyList_GET_SIZE(pieces) > 0);
    s = PyString_FromString("[");
    if (s == NULL) {
        goto Done;
    }
    temp = PyList_GET_ITEM(pieces, 0);
    PyString_ConcatAndDel(&s, temp);
    PyList_SET_ITEM(pieces, 0, s);
    if (s == NULL) {
        goto Done;
    }

    s = PyString_FromString("]");
    if (s == NULL) {
        goto Done;
    }
    temp = PyList_GET_ITEM(pieces, PyList_GET_SIZE(pieces) - 1);
    PyString_ConcatAndDel(&temp, s);
    PyList_SET_ITEM(pieces, PyList_GET_SIZE(pieces) - 1, temp);
    if (temp == NULL) {
        goto Done;
    }

    // Paste them all together with ", " between.
    s = PyString_FromString(", ");
    if (s == NULL) {
        goto Done;
    }
    result = _PyString_Join(s, pieces);
    Py_DECREF(s);

Done:
    Py_XDECREF(pieces);
    Py_ReprLeave((PyObject *)v);
    return result;
}
#endif

// This function is a copy of Python-3.4.3/Objects/dictobject.c::dict_repr()
// with the following differences:
// - An element or sub-element of a list may not reference the list or any
//   containing parent. In normal, list_repr(), Python just prints ellipses;
//   in cjsonish, we raine an EncodeError.
// - We call our own encode_object() rather than Python's PyObject_Repr()
//   to serialize items, so we can override peculiarities as necessary.
#if PY_MAJOR_VERSION >= 3
static PyObject*
//dict_repr(PyDictObject *mp)
encode_dict(PyDictObject *mp)
{
    Py_ssize_t i;
    PyObject *key = NULL, *value = NULL;
    _PyUnicodeWriter writer;
    int first;

    i = Py_ReprEnter((PyObject *)mp);
    if (i != 0) {
        // OC:
        //  return i > 0 ? PyUnicode_FromString("{...}") : NULL;
        if (i > 0) {
            PyErr_SetString(
                JSON_EncodeError,
                "a dict with references to itself is not JSON encodable");
        }
        // else, i == -1; an expection occurred.
        return NULL;
    }

    if (mp->ma_used == 0) {
        Py_ReprLeave((PyObject *)mp);
        return PyUnicode_FromString("{}");
    }

    _PyUnicodeWriter_Init(&writer);
    writer.overallocate = 1;
    // "{" + "1: 2" + ", 3: 4" * (len - 1) + "}"
    writer.min_length = 1 + 4 + (2 + 4) * (mp->ma_used - 1) + 1;

    if (_PyUnicodeWriter_WriteChar(&writer, '{') < 0)
        goto error;

    // Do repr() on each key+value pair, and insert ": " between them.
    // Note that repr may mutate the dict.
    i = 0;
    first = 1;
    while (PyDict_Next((PyObject *)mp, &i, &key, &value)) {
        PyObject *s;
        int res;

        // cjsonish: dict keys must be strings:
        if (!PyString_Check(key) && !PyUnicode_Check(key)) {
            PyErr_SetString(
                JSON_EncodeError,
                "JSON encodable dictionaries must have string/unicode keys"
            );
            goto error;
        }

        // Prevent repr from deleting key or value during key format.
        Py_INCREF(key);
        Py_INCREF(value);

        if (!first) {
            if (_PyUnicodeWriter_WriteASCIIString(&writer, ", ", 2) < 0)
                goto error;
        }
        first = 0;

        // OC:
        //  s = PyObject_Repr(key);
        s = encode_object(key);
        if (s == NULL)
            goto error;
        res = _PyUnicodeWriter_WriteStr(&writer, s);
        Py_DECREF(s);
        if (res < 0)
            goto error;

        if (_PyUnicodeWriter_WriteASCIIString(&writer, ": ", 2) < 0)
            goto error;

        // OC:
        //  s = PyObject_Repr(value);
        s = encode_object(value);
        if (s == NULL)
            goto error;
        res = _PyUnicodeWriter_WriteStr(&writer, s);
        Py_DECREF(s);
        if (res < 0)
            goto error;

        Py_CLEAR(key);
        Py_CLEAR(value);
    }

    writer.overallocate = 0;
    if (_PyUnicodeWriter_WriteChar(&writer, '}') < 0)
        goto error;

    Py_ReprLeave((PyObject *)mp);

    return _PyUnicodeWriter_Finish(&writer);

error:
    Py_ReprLeave((PyObject *)mp);
    _PyUnicodeWriter_Dealloc(&writer);
    Py_XDECREF(key);
    Py_XDECREF(value);
    return NULL;
}
#else
// This function is an almost verbatim copy of dict_repr() from
// Python's dictobject.c with the following differences:
// - it uses encode_object() to get the object's JSON reprezentation.
// - only accept strings for keys.
// - it doesn't use the ellipsis to represent a dictionary with references
//   to itself, instead it raises an exception as such dictionaries cannot
//   be represented in JSON.
/* From older Python2:
static PyObject*
encode_dict(PyObject *dict)
{
    Py_ssize_t i;
    PyObject *s, *temp, *colon = NULL;
    PyObject *pieces = NULL, *result = NULL;
    PyObject *key, *value;
    PyDictObject *mp = (PyDictObject*) dict;

    i = Py_ReprEnter((PyObject *)mp);
    if (i != 0) {
        if (i > 0) {
            PyErr_SetString(JSON_EncodeError, "a dict with references to "
                            "itself is not JSON encodable");
        }
        return NULL;
    }

    if (mp->ma_used == 0) {
        result = PyString_FromString("{}");
        goto Done;
    }

    pieces = PyList_New(0);
    if (pieces == NULL)
        goto Done;

    colon = PyString_FromString(": ");
    if (colon == NULL)
        goto Done;

    // Do repr() on each key+value pair, and insert ": " between them.
    // Note that repr may mutate the dict.
    i = 0;
    while (PyDict_Next((PyObject *)mp, &i, &key, &value)) {
        int status;

        if (!PyString_Check(key) && !PyUnicode_Check(key)) {
            PyErr_SetString(JSON_EncodeError, "JSON encodable dictionaries "
                            "must have string/unicode keys");
            goto Done;
        }

        // Prevent repr from deleting value during key format.
        Py_INCREF(value);
        s = encode_object(key);
        PyString_Concat(&s, colon);
        PyString_ConcatAndDel(&s, encode_object(value));
        Py_DECREF(value);
        if (s == NULL)
            goto Done;
        status = PyList_Append(pieces, s);
        Py_DECREF(s);  // append created a new ref
        if (status < 0)
            goto Done;
    }

    // Add "{}" decorations to the first and last items.
    assert(PyList_GET_SIZE(pieces) > 0);
    s = PyString_FromString("{");
    if (s == NULL)
        goto Done;
    temp = PyList_GET_ITEM(pieces, 0);
    PyString_ConcatAndDel(&s, temp);
    PyList_SET_ITEM(pieces, 0, s);
    if (s == NULL)
        goto Done;

    s = PyString_FromString("}");
    if (s == NULL)
        goto Done;
    temp = PyList_GET_ITEM(pieces, PyList_GET_SIZE(pieces) - 1);
    PyString_ConcatAndDel(&temp, s);
    PyList_SET_ITEM(pieces, PyList_GET_SIZE(pieces) - 1, temp);
    if (temp == NULL)
        goto Done;

    // Paste them all together with ", " between.
    s = PyString_FromString(", ");
    if (s == NULL)
        goto Done;
    result = _PyString_Join(s, pieces);
    Py_DECREF(s);

Done:
    Py_XDECREF(pieces);
    Py_XDECREF(colon);
    Py_ReprLeave((PyObject *)mp);
    return result;
}
*/
static PyObject*
//dict_repr(PyDictObject *mp)
encode_dict(PyDictObject *mp)
{
    Py_ssize_t i;
    PyObject *s, *temp, *colon = NULL;
    PyObject *pieces = NULL, *result = NULL;
    PyObject *key, *value;

    i = Py_ReprEnter((PyObject *)mp);
    if (i != 0) {
        // OC:
        //  return i > 0 ? PyString_FromString("{...}") : NULL;
        if (i > 0) {
            PyErr_SetString(JSON_EncodeError, "a dict with references to "
                            "itself is not JSON encodable");
        }
        return NULL;
    }

    if (mp->ma_used == 0) {
        result = PyString_FromString("{}");
        goto Done;
    }

    pieces = PyList_New(0);
    if (pieces == NULL) {
        goto Done;
    }

    colon = PyString_FromString(": ");
    if (colon == NULL) {
        goto Done;
    }

    // Do repr() on each key+value pair, and insert ": " between them.
    // Note that repr may mutate the dict.
    i = 0;
    while (PyDict_Next((PyObject *)mp, &i, &key, &value)) {
        int status;
        // cjsonish: dict keys must be strings:
        if (!PyString_Check(key) && !PyUnicode_Check(key)) {
            PyErr_SetString(
                JSON_EncodeError,
                "JSON encodable dictionaries must have string/unicode keys"
            );
            goto Done;
        }
        // Prevent repr from deleting value during key format.
        Py_INCREF(value);
        // OC:
        //  s = PyObject_Repr(key);
        s = encode_object(key);
        PyString_Concat(&s, colon);
        // OC:
        //  PyString_ConcatAndDel(&s, PyObject_Repr(value));
        PyString_ConcatAndDel(&s, encode_object(value));
        Py_DECREF(value);
        if (s == NULL) {
            goto Done;
        }
        status = PyList_Append(pieces, s);
        Py_DECREF(s); // append created a new ref
        if (status < 0) {
            goto Done;
        }
    }

    // Add "{}" decorations to the first and last items.
    assert(PyList_GET_SIZE(pieces) > 0);
    s = PyString_FromString("{");
    if (s == NULL) {
        goto Done;
    }
    temp = PyList_GET_ITEM(pieces, 0);
    PyString_ConcatAndDel(&s, temp);
    PyList_SET_ITEM(pieces, 0, s);
    if (s == NULL) {
        goto Done;
    }

    s = PyString_FromString("}");
    if (s == NULL) {
        goto Done;
    }
    temp = PyList_GET_ITEM(pieces, PyList_GET_SIZE(pieces) - 1);
    PyString_ConcatAndDel(&temp, s);
    PyList_SET_ITEM(pieces, PyList_GET_SIZE(pieces) - 1, temp);
    if (temp == NULL) {
        goto Done;
    }

    // Paste them all together with ", " between.
    s = PyString_FromString(", ");
    if (s == NULL) {
        goto Done;
    }
    result = _PyString_Join(s, pieces);
    Py_DECREF(s);

Done:
    Py_XDECREF(pieces);
    Py_XDECREF(colon);
    Py_ReprLeave((PyObject *)mp);
    return result;
}
#endif

static PyObject*
encode_object(PyObject *object)
{
    if (object == Py_True) {
        #if PY_MAJOR_VERSION >= 3
            return PyUnicode_FromString("true");
        #else
            return PyString_FromString("true");
        #endif
    }
    else if (object == Py_False) {
        #if PY_MAJOR_VERSION >= 3
            return PyUnicode_FromString("false");
        #else
            return PyString_FromString("false");
        #endif
    }
    else if (object == Py_None) {
        #if PY_MAJOR_VERSION >= 3
            return PyUnicode_FromString("null");
        #else
            return PyString_FromString("null");
        #endif
    }
#if PY_MAJOR_VERSION >= 3
    else if (PyBytes_Check(object)) {
        PyErr_Format(
            JSON_EncodeError,

// FIXME: Here and elsewhere: include line and col.
            //"unexpected bytes/string object found at position " SSIZE_T_F,
            //(Py_ssize_t)(jsondata.ptr - jsondata.str)
            "unexpected bytes/string object found"

        );
        return NULL;
#else
    else if (PyString_Check(object)) {
        return encode_string(object);
#endif
    }
    else if (PyUnicode_Check(object)) {
        return encode_unicode(object);
    }
    else if (PyFloat_Check(object)) {
        double val = PyFloat_AS_DOUBLE(object);
        if (Py_IS_NAN(val)) {
            #if PY_MAJOR_VERSION >= 3
                return PyUnicode_FromString("NaN");
            #else
                return PyString_FromString("NaN");
            #endif
        }
        else if (Py_IS_INFINITY(val)) {
            if (val > 0) {
                #if PY_MAJOR_VERSION >= 3
                    return PyUnicode_FromString("Infinity");
                #else
                    return PyString_FromString("Infinity");
                #endif
            }
            else {
                #if PY_MAJOR_VERSION >= 3
                    return PyUnicode_FromString("-Infinity");
                #else
                    return PyString_FromString("-Infinity");
                #endif
            }
        }
        else {
            return PyObject_Repr(object);
        }
    }
    else if (PyInt_Check(object) || PyLong_Check(object)) {
        return PyObject_Str(object);
    }
    else if (PyList_Check(object)) {
        return encode_list((PyListObject*)object);
    }
    else if (PyTuple_Check(object)) {
        return encode_tuple((PyTupleObject*)object);
    }
    else if (PyDict_Check(object)) { // use PyMapping_Check(object) instead? -Dan
        return encode_dict((PyDictObject*)object);
    }
    else {
        PyErr_SetString(JSON_EncodeError, "object is not JSON encodable");
        return NULL;
    }
}

// Encode object into its JSON representation

static PyObject*
JSON_encode(PyObject *self, PyObject *object)
{
    return encode_object(object);
}

// Decode JSON representation into python objects
static PyObject*
JSON_decode(PyObject *self, PyObject *args, PyObject *kwargs)
{
    static char *kwlist[] = {"json", "all_unicode", NULL};
    int all_unicode = False; // by default return unicode only when needed
    PyObject *object, *string, *str;
    JSONData jsondata;

    if (!PyArg_ParseTupleAndKeywords(
        args, kwargs, "O|i:decode", kwlist, &string, &all_unicode)
    ) {
        return NULL;
    }

    if (PyUnicode_Check(string)) {
        str = PyUnicode_AsRawUnicodeEscapeString(string);
        if (str == NULL) {
            return NULL;
        }
    }
    else {
        Py_INCREF(string);
        str = string;
    }

    if (PyBytes_AsStringAndSize(str, &(jsondata.str), NULL) == -1) {
        Py_DECREF(str);
        return NULL; // not a string object or it contains null bytes
    }

    #if PY_MAJOR_VERSION >= 3
    //Py_ssize_t str_size = PyUnicode_GET_SIZE(str);
    Py_ssize_t str_size = Py_SIZE(str);
    #else
    Py_ssize_t str_size = PyString_GET_SIZE(str);
    #endif
    jsondata.ptr = jsondata.str;
    jsondata.end = jsondata.str + str_size;
    jsondata.all_unicode = all_unicode;

    object = decode_json(&jsondata);

    if (object != NULL) {
        skipSpaces(&jsondata);
        if (jsondata.ptr < jsondata.end) {
            PyErr_Format(
                JSON_DecodeError,
                "extra data after JSON description at position " SSIZE_T_F,
                (Py_ssize_t)(jsondata.ptr - jsondata.str)
            );
            Py_DECREF(str);
            Py_DECREF(object);
            return NULL;
        }
    }

    Py_DECREF(str);

    return object;
}

static PyMethodDef cjsonish_methods[] = {
    {
        "encode",
        (PyCFunction)JSON_encode,
        METH_O,
        PyDoc_STR("encode(object) -> generate the JSON representation for object.")
    },
    {
        "decode",
        (PyCFunction)JSON_decode,
        METH_VARARGS|METH_KEYWORDS,
        PyDoc_STR(
            "decode(string, all_unicode=False) -> parse the JSON representation into\n"
            "python objects. The optional argument `all_unicode', specifies how to\n"
            "convert the strings in the JSON representation into python objects.\n"
            "If it is False (default), it will return strings everywhere possible\n"
            "and unicode objects only where necessary, else it will return unicode\n"
            "objects everywhere (this is slower)."
        )
    },
    {NULL, NULL}  // sentinel
};

PyDoc_STRVAR(
    module_doc,
    "Fast and loose JSON encoder/decoder module."
);

#if PY_MAJOR_VERSION >= 3
static struct PyModuleDef cjsonish_moduledef = {
    PyModuleDef_HEAD_INIT,
    "cjsonish", // m_name
    module_doc, // m_doc
    -1, // m_size
    cjsonish_methods, // m_methods
    NULL, // m_reload
    NULL, // m_traverse
    NULL, // m_clear
    NULL, // m_free
};
#endif

// Initialization function for the module.

MOD_INIT(cjsonish)
{
    PyObject *m;

    MOD_DEF(m, "cjsonish", cjsonish_methods, module_doc);

    if (m == NULL) {
        return MOD_ERROR_VAL;
    }

    JSON_Error = PyErr_NewException("cjsonish.Error", NULL, NULL);
    if (JSON_Error == NULL) {
        return MOD_ERROR_VAL;
    }
    Py_INCREF(JSON_Error);
    PyModule_AddObject(m, "Error", JSON_Error);

    JSON_EncodeError = PyErr_NewException("cjsonish.EncodeError", JSON_Error, NULL);
    if (JSON_EncodeError == NULL) {
        return MOD_ERROR_VAL;
    }
    Py_INCREF(JSON_EncodeError);
    PyModule_AddObject(m, "EncodeError", JSON_EncodeError);

    JSON_DecodeError = PyErr_NewException("cjsonish.DecodeError", JSON_Error, NULL);
    if (JSON_DecodeError == NULL) {
        return MOD_ERROR_VAL;
    }
    Py_INCREF(JSON_DecodeError);
    PyModule_AddObject(m, "DecodeError", JSON_DecodeError);

    // Module version (the MODULE_VERSION macro is defined by setup.py)
    PyModule_AddStringConstant(m, "__version__", string(MODULE_VERSION));

    return MOD_SUCCESS_VAL(m);
}

