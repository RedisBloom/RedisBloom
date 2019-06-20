
# Quick Start Guide for RedisBloom

Bloom filters and cuckoo filters work fairly similarly at the API level; they
both allow you to add items to a set (in effect making sure the item has been
'seen' by the set).

## Building and running

```sh
git clone https://github.com/RedisBloom/RedisBloom.git
cd redisbloom
make

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

> A new filter is created for you if it does not yet exist

```
127.0.0.1:6379> CF.ADD newFilter foo
(integer) 1
```

You can add the item multiple times. The filter will attempt to count it.

## Cuckoo: Checking whether item exists

```
127.0.0.1:6379> CF.EXISTS newFilter foo
(integer) 1
```

```
127.0.0.1:6379> CF.EXISTS newFilter notpresent
(integer) 0
```

## Cuckoo: Deleting item from filter

```
127.0.0.1:6379> CF.DEL newFilter foo
(integer) 1
```
