FROM redisfab/rmbuilder:6.0.9-x64-bionic as builder

# Build the source
ADD . /
WORKDIR /
RUN set -ex;\
    make clean; \
    make all -j 4; \
    make test;

# Package the runner
FROM redisfab/redis:6.0-latest-x64-bionic
ENV LIBDIR /usr/lib/redis/modules
WORKDIR /data
RUN set -ex;\
    mkdir -p "$LIBDIR";
COPY --from=builder /redisbloom.so "$LIBDIR"

CMD ["redis-server", "--loadmodule", "/usr/lib/redis/modules/redisbloom.so"]
