#!/bin/bash
SCRIPT_DIR="$(dirname ${BASH_SOURCE[0]})"
python2 ./${SCRIPT_DIR}/setup.py $*
python3 ./${SCRIPT_DIR}/setup.py $*

