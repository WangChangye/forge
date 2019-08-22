#FROM scratch
FROM busybox
ADD httpsrv /
ADD htdocs /htdocs
RUN chmod 755 /httpsrv
ENTRYPOINT ["/httpsrv>log 2>&1"]
