FROM redislabsmodules/rmbuilder:latest as builder

# Build the source
ADD . /
WORKDIR /
RUN set -ex;\
    make clean; \
    make all -j 4; \
    make test;

# Package the runner
FROM redis:latest
ENV LIBDIR /usr/lib/redis/modules
WORKDIR /data
RUN set -ex;\
    mkdir -p "$LIBDIR";
COPY --from=builder /rebloom.so "$LIBDIR"

CMD ["redis-server", "--loadmodule", "/var/lib/redis/modules/rebloom.so"]
