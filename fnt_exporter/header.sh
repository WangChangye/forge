#!/bin/bash
if [ "`whoami`" != "root" ];then
    echo "ERROR: this utility cannot be run with nonroot account."
    exit 1
fi
export target_dir="/opt/fnt_exporter"
export srcfname="`basename $0`"
cd `dirname $0`
export srcdir="`pwd`"

usage()
{
    echo "Usage:"
    echo "  ${srcfname} install | uninstall"
}
package()
{
    if [ -e ./fnt_exporter ];then
        rm -rf ./fnt_exporter
    fi
    mkdir ./fnt_exporter
    cp fnt-daemon ./fnt_exporter/
    cp fnt_exporter_server ./fnt_exporter/
    cp get_kube_crtinfo.sh ./fnt_exporter/
    cp cronjob-fnt-scrape-and-check ./fnt_exporter/
    cp -r htdocs ./fnt_exporter/
    cp *.yaml ./fnt_exporter/
    tar cf payload.tar ./fnt_exporter
    cat ${srcfname} payload.tar >fnt_exporter.bin
    chmod 755 fnt_exporter.bin
    rm -rf ./fnt_exporter payload.tar
    echo "Package fnt_exporter.bin is ready."
}
install()
{
    sed '1,/^PAYLOAD_FOLLOWED$/d' $0|tar -C /opt/ -xvf -
    cd ${target_dir}
    if [ -e /etc/cron.d/cronjob-fnt-scrape-and-check ];then
        rm -f /etc/cron.d/cronjob-fnt-scrape-and-check
    fi
    mv cronjob-fnt-scrape-and-check /etc/cron.d/cronjob-fnt-scrape-and-check
    chmod 755 fnt-daemon fnt_exporter_server get_kube_crtinfo.sh
}
uninstall()
{
    rm -rf ${target_dir}
    if [ -e /etc/cron.d/cronjob-fnt-scrape-and-check ];then
        rm -f /etc/cron.d/cronjob-fnt-scrape-and-check
    fi
}

if [ "$1" == "install" ];then
    install
elif [ "$1" == "uninstall" ];then
    uninstall
elif [ "$1" == "pkg" ];then
    package
else
    usage
    exit 1
fi

exit 0
PAYLOAD_FOLLOWED
