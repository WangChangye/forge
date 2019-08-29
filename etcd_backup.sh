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
    /etcdctl --endpoints "${etcd_servers#X}" \
        --cert=${etcd_certfile#X} \
        --key=${etcd_keyfile#X} \
        --cacert=${etcd_cafile#X} \
        snapshot save /var/lib/etcd/etcd_v3_backup_`date +%Y%m%d-%H%M%S`
else
    /etcdctl --endpoints "${etcd_servers#X}" snapshot save /var/lib/etcd/etcd_v3_backup_`date +%Y%m%d-%H%M%S`
fi
cd /var/lib/etcd/
ls -t etcd_v3_backup_* |sed '1,7d'|xargs -n1 rm -rf
