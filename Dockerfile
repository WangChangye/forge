FROM scracth
ADD httpsrv /
ADD htdocs /htdocs
RUN chmod 755 /httpsrv
ENTRYPOINT ["/httpsrv"]
