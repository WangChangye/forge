#FROM scratch
#FROM busybox
FROM amd64/gcc:8
ADD fnt_http_fun.c /
ADD fnt_http_fun.h /
ADD http_server.c /
ADD htdocs /htdocs
RUN gcc --static -o /httpsrv fnt_http_fun.c http_server.c
ENTRYPOINT ["/httpsrv"]
