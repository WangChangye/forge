FROM scratch
ADD httpsrv /
ADD htdocs /htdocs
RUN sudo chmod 755 /httpsrv
ENTRYPOINT ["/httpsrv"]
