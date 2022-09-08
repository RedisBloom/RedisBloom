# BUILD redisfab/redistimeseries:${VERSION}-${ARCH}-${OSNICK}

ARG REDIS_VER=6.2.7

# stretch|bionic|buster
ARG OSNICK=buster

# ARCH=x64|arm64v8|arm32v7
ARG ARCH=x64

#----------------------------------------------------------------------------------------------
FROM redisfab/redis:${REDIS_VER}-${ARCH}-${OSNICK} AS builder

ARG REDIS_VER

ADD ./ /build
WORKDIR /build

RUN ./deps/readies/bin/getpy3
RUN ./system-setup.py
RUN set -ex ;\
    if [ -e /usr/bin/apt-get ]; then \
        apt-get update -qq; \
        apt-get upgrade -yqq; \
        rm -rf /var/cache/apt; \
    fi
RUN if [ -e /usr/bin/yum ]; then \
        yum update -y; \
        rm -rf /var/cache/yum; \
    fi

RUN bash -l -c "make fetch"
RUN bash -l -c "make all"
RUN bash -l -c "TEST= make test"

#----------------------------------------------------------------------------------------------
FROM redisfab/redis:${REDIS_VER}-${ARCH}-${OSNICK}

ARG REDIS_VER
ENV LIBDIR /usr/lib/redis/modules
WORKDIR /data
RUN mkdir -p "$LIBDIR"

COPY --from=builder /build/redisbloom.so "$LIBDIR"

EXPOSE 6379
CMD ["redis-server", "--loadmodule", "/usr/lib/redis/modules/redisbloom.so"]
