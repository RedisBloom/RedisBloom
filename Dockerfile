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
COPY --from=builder /redisbloom.so "$LIBDIR"

CMD ["redis-server", "--loadmodule", "/usr/lib/redis/modules/redisbloom.so"]
