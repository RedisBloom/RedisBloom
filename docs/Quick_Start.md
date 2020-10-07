
# Quick Start Guide for RedisBloom

## Redis Cloud

RedisBloom is available on all Redis Cloud managed services.  Redis Cloud Essentials offers a completely free managed databbases up to 30MB.

[Get started here](https://redislabs.com/try-free/)

## Launch RedisBloom with Docker
```
docker run -p 6379:6379 --name redis-redisbloom redislabs/rebloom:latest
```

## Download

A pre-compiled version can be downloaded from [RedisLabs download center](https://redislabs.com/download-center/modules/).

## Building

```sh
git clone https://github.com/RedisBloom/RedisBloom.git
cd redisbloom
make
```
## running

```
# Assuming you have a redis build from the unstable branch:
/path/to/redis-server --loadmodule ./redisbloom.so
```

# Bloom Filters

## Bloom: Adding new items to the filter

> A new filter is created for you if it does not yet exist

```
127.0.0.1:6379> BF.ADD newFilter foo
(integer) 1
```

## Bloom: Checking if an item exists in the filter

```
127.0.0.1:6379> BF.EXISTS newFilter foo
(integer) 1
```

```
127.0.0.1:6379> BF.EXISTS newFilter notpresent
(integer) 0
```

## Bloom: Adding and checking multiple items

```
127.0.0.1:6379> BF.MADD myFilter foo bar baz
1) (integer) 1
2) (integer) 1
3) (integer) 1
```

```
127.0.0.1:6379> BF.MEXISTS myFilter foo nonexist bar
1) (integer) 1
2) (integer) 0
3) (integer) 1
```

## Bloom: Creating a new filter with custom properties

```
127.0.0.1:6379> BF.RESERVE customFilter 0.0001 600000
OK
```

```
127.0.0.1:6379> BF.MADD customFilter foo bar baz
```

# Cuckoo Filters

## Cuckoo: Adding new items to a filter


> Create an empty cuckoo filter with an initial capacity (of 1000 items)

```
127.0.0.1:6379> CF.RESERVE newCuckooFilter 1000
(integer) 1
```

> A new filter is created for you if it does not yet exist

```
127.0.0.1:6379> CF.ADD newCuckooFilter foo
(integer) 1
```

You can add the item multiple times. The filter will attempt to count it.

## Cuckoo: Checking whether item exists

```
127.0.0.1:6379> CF.EXISTS newCuckooFilter foo
(integer) 1
```

```
127.0.0.1:6379> CF.EXISTS newCuckooFilter notpresent
(integer) 0
```

## Cuckoo: Deleting item from filter

```
127.0.0.1:6379> CF.DEL newCuckooFilter foo
(integer) 1
```
