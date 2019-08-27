#!/bin/bash

docker version >/dev/null 2>&1
[ $? -eq 0 ]||exit 1

backup_root="/var/lib/etcd_backup"
# To simplify the implementation, we assume that the etcd container
# has such one volume: /var/lib/etcd, which is mapped to the same path
# of the host server, and the data-dir of etcd is right the path.
data_dir="/var/lib/etcd"
[ -d ${backup_root} ]||mkdir -p ${backup_root}
[ -d ${data_dir} ]||mkdir -p ${data_dir}

restore()
{
    docker ps --format "{{.ID}} {{.Command}}" -f status=running \
              -f volume=${data_dir} -f name=etcd.*  --no-trunc 2>/dev/null | \
           grep "etcd\ .*\ --data-dir=${data_dir}"|grep ":2379\ " | \
           awk '{print $1}'|xargs -n1 docker stop  2>/dev/null
    docker ps --format "{{.ID}} {{.Command}}" -f status=exited \
              -f volume=${data_dir} -f name=etcd.*  --no-trunc 2>/dev/null | \
           grep "etcd\ .*\ --data-dir=${data_dir}"|grep ":2379\ " | \
           awk '{print $1}'|xargs -n1 docker rm  2>/dev/null

    cd ${backup_root}
    latest_backup="`ls etcd_v2_backup_* -td 2>/dev/null|awk 'NR==1{print $1}'`"
    if [ "${latest_backup}" == "" ]||[ ! -d ${backup_root}/${latest_backup}/member ];then
        echo "Error: no valid backup found under path ${backup_root}."
        exit 1;
    fi

    cd ${data_dir}
    if [ ! -e ${data_dir}/member ];then
        echo "Warnning: original ${data_dir}/member not found, can't decide the proper owner for it, assume it to be root:root."
        dir_owner="root:root"
    else
        dir_owner=`ls -ld member|awk '{print $3":"$4;exit}'`
        mv ${data_dir}/member ${data_dir}/broken_data_`date +%Y%m%d_%H%M%S`
    fi
    cp -rf ${backup_root}/${latest_backup}/member ${data_dir}/
    chown -R ${dir_owner} ${data_dir}/member
    docker pull registry.local/google_containers/etcd-amd64:3.1.10 >/dev/null 2>&1
    docker inspect registry.local/google_containers/etcd-amd64:3.1.10 >/dev/null 2>&1
    if [ $? -eq 0 ];then
        docker_image="registry.local/google_containers/etcd-amd64:3.1.10"
        cmd_path=etcd
    else
        docker pull bitnami/etcd:latest >/dev/null 2>&1
        docker inspect bitnami/etcd:latest >/dev/null 2>&1
        if [ $? -eq 0 ];then
            docker_image="bitnami/etcd:latest"
            cmd_path=bin/etcd
        else
            echo "Error: no etcd docker image is avaliable."
            exit 1
        fi
    fi
    
    docker run -itd --name etcd -e ALLOW_NONE_AUTHENTICATION=yes \
           -v ${data_dir}:${data_dir} \
           -e ETCDCTL_API=2 --network host \
           ${docker_image} \
           ${cmd_path} --data-dir=${data_dir} --force-new-cluster \
           --listen-client-urls=http://127.0.0.1:2379 --advertise-client-urls=http://127.0.0.1:2379

    sleep 8
    docker exec etcd etcdctl ls >/dev/null 2>&1
    if [ $? -eq 0 ];then
        echo "Info: etcd service has been restored with backup ${latest_backup}."
        exit 0
    else
        echo "Error: failed to restore etcd with backup ${latest_backup}."
        exit 1
    fi
}
add_cron_item()
{
    curdir=`pwd`
    cd `dirname $0`
    srcdir=`pwd`
    absolute_path=${srcdir}/`basename $0`
    if [ `grep -c ${absolute_path} /etc/crontab` -eq 0 ];then
        echo "  27 */8  *  *  * root ${absolute_path}" >> /etc/crontab
        systemctl reload crond
    fi
}

if [ "$1" == "install" ];then
    add_cron_item
    exit 0
elif [ "$1" == "restore" ];then
    restore
    exit $?
fi

container_ID=X`docker ps --format "{{.ID}} {{.Command}}" -f status=running -f volume=${data_dir} -f name=etcd.*  --no-trunc 2>/dev/null|grep "etcd\ .*\ --data-dir=${data_dir}"|grep ":2379\ "|awk 'NR==1{print $1}'`
if [ "${container_ID}" == "X" ];then
    exit 0;
fi
container_ID="${container_ID#X}"

bkp_suffix="`date +%Y%m%d-%H%M%S`"
if [ -e ${data_dir}/backup/etcd_backup_${bkp_suffix} ];then
    rm -rf ${data_dir}/backup/etcd_backup_${bkp_suffix}
fi

# ETCDCTL_API=2
if [ `docker exec ${container_ID} etcdctl --version 2>/dev/null|grep "API\ version:\ *2"|wc -l` -gt 0 ];then
    docker exec ${container_ID} etcdctl backup --data-dir=${data_dir} --backup-dir=${data_dir}/backup/etcd_v2_backup_${bkp_suffix}
    cd ${backup_root}/
    mv ${data_dir}/backup/etcd_v2_backup_${bkp_suffix} ./
    ls -t etcd_v2_backup_* |sed '1,16d'|xargs -n1 rm -rf
    echo "New backup: ${backup_root}/etcd_v2_backup_${bkp_suffix}"
else
    docker exec ${container_ID} etcdctl snapshot save ${data_dir}/backup/etcd_v3_backup_${bkp_suffix}
    cd ${backup_root}/
    mv ${data_dir}/backup/etcd_v3_backup_${bkp_suffix} ./
    ls -t etcd_v3_backup_* |sed '1,16d'|xargs -n1 rm -rf
    echo "New backup: ${backup_root}/etcd_v3_backup_${bkp_suffix}"
fi

