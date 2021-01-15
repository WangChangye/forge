#!/bin/bash
export LC_ALL="en_US.UTF-8"
export LC_TIME="en_US.UTF-8"
cudir=`pwd`
srcdir=`dirname $0`
cd $srcdir
srcdir=`pwd`
srcexe="`basename $0`"
>${srcdir}/cache/data.tmp
find ${srcdir}/scraper.d/ -type f -name "*.sh"| \
while read line; do
    ${line} >>${srcdir}/cache/data.tmp
done
cp -f ${srcdir}/cache/data.tmp ${srcdir}/htdocs/exporter/data
