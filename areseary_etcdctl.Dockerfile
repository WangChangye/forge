FROM alpine:3.9
ADD etcdctl /
ADD etcd_backup.sh /
ADD run.sh /
#CMD ["sh"]
#ENTRYPOINT ["sh","/etcd_backup.sh"]
CMD ["sh","/run.sh"]
