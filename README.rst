cjsonish
========

The ``cjonish`` Python C extension implements a developer friendly
`JSON <http://www.json.org/>`__ codec.

In addition to the machine-centric JSON standard,
it accepts developer-pleasing syntax:

* Single- ``//`` and multi-line ``/* */`` comments.
* Trailing commas ``,``
* Single-quoted ``''`` object keys (as opposed to requiring double-quotes).
* Fractional numbers without a leading zero, like ``.123``.
* Multi-line strings -- either use a line continuation character at the end
  of lines (and it and the newline will be removed from the string), or just
  start a new line before the string closing quote (and the newline will be
  left in the string)..

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

Strict Mode
^^^^^^^^^^^

Yes, ``cjsonish`` can be tricked into adhering to the exact JSON spec.
Just set ``strict=True``.

.. code-block:: python

    >>> import cjsonish
    >>> cjsonish.decode('{"q": true, "23": null,}', strict=True)
    Traceback (most recent call last):
      File "<stdin>", line 1, in <module>
    cjsonish.DecodeError: expecting object property name rather than
        trailing comma at position 23 (lineno 1, offset 23)

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

Compilation
-----------

Prerequisites
^^^^^^^^^^^^^

You'll at least need a C compiler and the Python headers.

.. code-block:: bash

    sudo apt-get install -y libpython3-dev

Debug Build
^^^^^^^^^^^

From the source directory, to make a debug build, try:

.. code-block:: bash

    /bin/rm -rf build/ dist/ python_cjsonish.egg-info/
    python3 ./setup.py clean
    CFLAGS='-Wall -O0 -g' python3 ./setup.py build
    python3 ./setup.py install

and then, e.g.,

.. code-block:: bash

    gdb python3
    b JSON_decode
    run
    import cjsonish
    cjsonish.decode('{"my": "example",} // ignored')

And if you want a python2 build, do it all over again.

.. code-block:: bash

    /bin/rm -rf build/ dist/ python_cjsonish.egg-info/
    python2 ./setup.py clean
    CFLAGS='-Wall -O0 -g' python2 ./setup.py build
    python2 ./setup.py install

Production Build
^^^^^^^^^^^^^^^^

Omit the ``CFLAGS`` to make a production build, 'natch.

.. Install Using Node.js Package Manager (npm)
.. -------------------------------------------
.. 
.. You can install the project using ``npm`` but it won't compile
.. the C Python extension, so you really have no reason to run:
.. 
.. .. code-block:: bash
.. 
..     # Weird. Need to relax privileges to install.
..     sudo chmod 664 /usr/local/lib/python2.7/dist-packages/easy-install.pth
..     # Weird. Need to restrict group write to squelch UserWarning on import.
..     chmod 2755 ${HOME}/.python-eggs
..     # Finally...
..     npm install landonb/cjsonish

Additional Information
----------------------

Some articles on JSON performance in Python:

* https://gist.github.com/lightcatcher/1136415
* http://stackoverflow.com/questions/706101/python-json-decoding-performance
* https://liangnuren.wordpress.com/2012/08/13/python-json-performance/
* https://gist.github.com/techno/4486729

Some articles on writing C Python extensions:

* https://docs.python.org/3.4/c-api/module.html
* https://docs.python.org/3.4/c-api/structures.html
* http://python3porting.com/cextensions.html

