FROM ubuntu:16.04
ADD httpsrv /
ADD htdocs /htdocs
ENTRYPOINT ["tail","-f","/etc/passwd"]
