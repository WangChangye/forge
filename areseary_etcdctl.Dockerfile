FROM alpine:3.9
ADD etcdctl /
ADD etcd_backup.sh /
#CMD ["sh"]
#ENTRYPOINT ["sh","/etcd_backup.sh"]
CMD ["sh","/etcd_backup.sh"]
