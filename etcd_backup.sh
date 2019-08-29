if [ -e /etc/kubernetes/manifests/kube-apiserver.yaml ];then
    ref_file=/etc/kubernetes/manifests/kube-apiserver.yaml
elif [ -e /etc/systemd/system/kube-apiserver.service ]; then
    ref_file=/etc/systemd/system/kube-apiserver.service
else
    echo "Error: failed to find etcd cfg from kube-api."
    exit 0
fi

etcd_cafile=X`sed -n 's/^.*\ --etcd-cafile=\([^[:space:]]\{1,\}\).*$/\1/p;1!d' ${ref_file}`
etcd_keyfile=X`sed -n 's/^.*\ --etcd-keyfile=\([^[:space:]]\{1,\}\).*$/\1/p;1!d' ${ref_file}`
etcd_certfile=X`sed -n 's/^.*\ --etcd-certfile=\([^[:space:]]\{1,\}\).*$/\1/p;1!d' ${ref_file}`
etcd_servers=X`sed -n 's/^.*\ --etcd-servers=\([^[:space:]]\{1,\}\).*$/\1/p;1!d' ${ref_file}`

if [ "${etcd_servers}" == "X" ];then
    echo "Error: failed to find etcd-servers from kube-api service."
    exit 0
fi

export ETCDCTL_API=3
if [ "${etcd_servers%%https:*}" == "X" ];then
    if [ "${etcd_cafile}" == "X" ]||[ "${etcd_keyfile}" == "X" ]||[ "${etcd_certfile}" == "X" ];then
        echo "Error: got etcd endpoints with tls, but failed to find out the tls cert files."
        exit 0
    fi

    cmd="/etcdctl --endpoints ${etcd_servers#X} --cert=${etcd_certfile#X} --key=${etcd_keyfile#X} --cacert=${etcd_cafile#X}"
else
    cmd="/etcdctl --endpoints ${etcd_servers#X}"
fi

polit_v=`date +%s`
${cmd} put "/fnt/polit" "${polit_v}"
tvar=X`ETCDCTL_API=3 /etcdctl --endpoints="http://127.0.0.1:2379" get "/fnt/polit"|awk 'NR==2{print $1}'`
if [ ! -d /var/lib/k8s_etcd_backup ];then
    mkdir -p /var/lib/k8s_etcd_backup
fi
cd /var/lib/k8s_etcd_backup/
if [ "${tvar#X}" == "${polit_v}" ];then
    ${cmd} snapshot save /var/lib/k8s_etcd_backup/etcd_v3_backup_`date +%Y%m%d-%H%M%S`
    ls -t etcd_v3_backup_* |sed '1,7d'|xargs -n1 rm -rf
else
    echo "Warning: detected error when tried to put&get etcd data."
    latest_backup=X`ls etcd_v3_backup_* -td 2>/dev/null|awk 'NR==1{print $1}'`
    if [ "${latest_backup}" == "X" ];then
        echo "Error: there is no valid backup."
        exit 0
    fi
    mv /var/lib/etcd /var/lib/broken_etcd_`date +%Y%m%d-%H%M%S`
    echo "Restoring data from latest backup ${latest_backup#X}..."
    ${cmd} snapshot restore /var/lib/k8s_etcd_backup/${latest_backup#X} --data-dir=/var/lib/etcd
fi
