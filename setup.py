#!/usr/bin/python

from setuptools import setup, Extension

__version__ = "1.2.0"

macros = [('MODULE_VERSION', __version__)]

setup(
    name = "python-cjsonish",
    version = __version__,
    #packages = find_packages(),
    author = "Landon Bouma (using Dan Pascu's python-cjson code)",
    author_email = "XXX",
    #keywords = "XXX XXX XXX",
    url = "https://github.com/landonb/python-cjsonish",
    download_url = "https://github.com/landonb/python-cjsonish",
    description = "Fast and loose JSON encoder/decoder for Python",
    long_description = open('README.rst').read(),
    license = "LGPL",
    platforms = ["Platform Independent"],
    classifiers = [
        "Development Status :: 5 - Production/Stable",
        "Intended Audience :: Developers",
        "License :: OSI Approved :: GNU Library or Lesser General Public License (LGPL)",
        "Operating System :: OS Independent",
        "Programming Language :: Python",
        "Topic :: Software Development :: Libraries :: Python Modules"
    ],
    ext_modules = [
        Extension(name='cjsonish', sources=['cjsonish.c'], define_macros=macros)
    ]
)

