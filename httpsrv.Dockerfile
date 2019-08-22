FROM scratch
ADD httpsrv /
ADD htdocs /htdocs
ENTRYPOINT ["/httpsrv"]
