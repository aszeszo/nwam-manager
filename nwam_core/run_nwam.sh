#!/bin/sh

NWAM_CORE_BASE=`dirname $0`
NWAM_CORE_BASE=`sh -c "cd ${NWAM_CORE_BASE};pwd"`

LD_RUN_PATH=${NWAM_CORE_BASE}/usr/lib
export LD_RUN_PATH

LD_LIBRARY_PATH=${NWAM_CORE_BASE}/usr/lib
export LD_LIBRARY_PATH

exec ${1+"$@"}
