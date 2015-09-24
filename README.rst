cjsonish
========

The `cjonish` python module implements a loose and fast
`JSON <http://www.json.org/>`__` encoder and decoder in native C.

This module works in both Python2 and Python3.

The module is derived from Dan Pascu's Python2 module,
`python-cjson 1.1.0
<https://pypi.python.org/pypi/python-cjson>`__`.

Fast
----

The module is a C extension, so processing
is faster than a native Python JSON module.

Loose
-----

The module is based on demjson notation:
specifically, it accepts more human-readable "JSON".
You can use single-line comments (starting with //)
and you can use trailing commas in objects and lists.

Usage example
-------------

.. code-block:: python

    >>> import cjsonish
    >>> cjsonish.encode({'q': True, '23': None,})
    '{"q": true, "23": null}'
    >>> cjsonish.decode('{"q": true, "23": null}')
    {'q': True, '23': None}

Performance
-----------

FIXME: Document this.

https://gist.github.com/lightcatcher/1136415
http://stackoverflow.com/questions/706101/python-json-decoding-performance
https://liangnuren.wordpress.com/2012/08/13/python-json-performance/

