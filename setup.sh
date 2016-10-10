#!/bin/bash
#  vim:tw=0:ts=4:sw=4:et

if [[ -z ${DEBUG_BUILD+x} ]]; then
    DEBUG_BUILD=false
fi

SCRIPT_DIR=$(dirname $(readlink -f $0))

if [[ -z ${SCRIPT_DIR} || ${SCRIPT_DIR} = '/' ]]; then
    # Silent but deadly.
    exit 1
fi

CFLAGS=''
if ${DEBUG_BUILD}; then
    CFLAGS='-Wall -O0 -g'
fi

skip_py3_imports=''
if false; then
    skip_py3_imports='CPPFLAGS="" CXXFLAGS="" LDFLAGS=""'
fi

local_install_path=''
if [[ -n ${INSTALL_PREFIX} ]]; then
    local_install_path="--prefix ${INSTALL_PREFIX}"
fi

cleanup_build() {
    /bin/rm -rf build/ dist/ python_chjson.egg-info/
}

build_for_py() {

    pushd ${SCRIPT_DIR} &> /dev/null

    pyexe=$1

    pycmd="${skip_py3_imports} ${pyexe}"

    cleanup_build

    ${pycmd} ./setup.py clean

    CFLAGS=${CFLAGS} ${pycmd} ./setup.py build

    ${pycmd} ./setup.py install ${local_install_path}

    cleanup_build

    popd &> /dev/null
}

for pyexe in python2 python3; do
    build_for_py $pyexe
done

