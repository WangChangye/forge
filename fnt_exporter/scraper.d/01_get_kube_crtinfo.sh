#!/bin/bash
export LC_ALL="en_US.UTF-8"
export LC_TIME="en_US.UTF-8"
current_sec="`date +%s`"
echo '# HELP kube_cert_validity_days The remained period of validity for the kube certs.'
echo '# TYPE kube_cert_validity_days gauge'
find /root/.minikube/ /root/k8s_setup/ssl_forge -type f| \
xargs -n1 file|awk -F": " '{if($2=="PEM certificate")print$1}'| \
while read line; do
    expiry_str="`openssl x509 -in /root/k8s_setup/ssl_forge/kube-apiserver.pem -noout -dates 2>/dev/null|awk -F= '{if($1=="notAfter")print $2}'`"
    if [ "${expiry_str}" == "" ];then
        validity_days="unknown"
        expiry_str="unknown"
    else
        expiry_sec=`date -d "${expiry_str}" +%s`
        validity_days=$(((${expiry_sec}-${current_sec})/86400))
    fi
    echo "kube_cert_validity_days{cert_filename=\"${line}\",expiry_str=\"${expiry_str}\"} ${validity_days}";
done
