
# Quick Start Guide for ReBloom

## Building and running

```sh
git clone https://github.com/RedisLabsModules/rebloom.git
cd rebloom
make

# Assuming you have a redis build from the unstable branch:
/path/to/redis-server --loadmodule ./rebloom.so
```

## Adding new items to the filter

> A new filter is created for you if it does not yet exist

```
127.0.0.1:6379> BF.ADD newFilter foo
(integer) 1
```

## Checking if an item exists in the filter

```
127.0.0.1:6379> BF.EXISTS newFilter foo
(integer) 1
```

```
127.0.0.1:6379> BF.EXISTS newFilter notpresent
(integer) 0
```

## Adding and checking multiple items

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

## Creating a new filter with custom properties

```
127.0.0.1:6379> BF.RESERVE customFilter 0.0001 600000
OK
```

```
127.0.0.1:6379> BF.MADD customFilter foo bar baz
```
