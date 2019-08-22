FROM ubuntu:16.04
ADD httpsrv /
ADD htdocs /htdocs
RUN chmod 755 /httpsrv
ENTRYPOINT ["/httpsrv"]
