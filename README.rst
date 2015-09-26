cjsonish
========

The ``cjonish`` Python C extension implements a developer friendly
`JSON <http://www.json.org/>`__ codec.

In addition to the machine-friendly JSON standard, it accepts:

* Single- ``//`` and multi-line ``/* */`` comments.
* Trailing commas ``,``
* Single-quoted ``''`` object keys.
* Fractional numbers without a leading zero, like ``.123``.
  
and it reports the line number and character offset on error.

This module works in Python 2.7, 3.3., and 3.4.

It should be easy to adapt to other versions as necessary.

The module is derived from Dan Pascu's Python2 module,
`python-cjson 1.1.0
<https://pypi.python.org/pypi/python-cjson>`__.

Usage example
-------------

Simple encoding and decoding string example:

.. code-block:: python

    >>> import cjsonish
    >>> cjsonish.encode({'q': True, '23': None,})
    '{"q": true, "23": null}'
    >>> cjsonish.decode('{"q": true, "23": null /* ignored \n */ , \'abc\': .123, } // ignored')
    {'23': None, 'q': True, 'abc': 0.123}

Simple file decoding example:

.. code-block:: python

    >>> try:
    ...     cjsonish.decode(open(json_path, 'r').read())
    ... except cjsonish.DecodeError as e:
    ...     # Log filename, line number, and column (offset).
    ...     fatal('Failed to load file "%s": %s' % (json_path, e.args[0],)
    ...     raise

Performance
-----------

The author tested this plugin against ``demjson`` (the only comparable
loose (developer-friendly) JSON codec) on 2015.09.25 and saw a seven times
improvement in performance over ``demjson`` for a real-world usage scenario
involving reading JSON files in Python, parsing them with either ``demjson`` or
``cjsonish``, and checking all dictionary values to see if they indicate a JSON
path that should be side-loaded. (Read: ``cjsonish`` is faster than ``demjson``
by around a factor, but it'll depend on your usage scenario.) In real-world
numbers, application boot time decreased from 6.65 seconds to 0.95 for this
scenario. Not a crazy gain but enough to ease development pains.

References
~~~~~~~~~~

Some articles on JSON performance in Python:

* https://gist.github.com/lightcatcher/1136415
* http://stackoverflow.com/questions/706101/python-json-decoding-performance
* https://liangnuren.wordpress.com/2012/08/13/python-json-performance/
* https://gist.github.com/techno/4486729

