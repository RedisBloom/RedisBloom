# RedisBloom TopK Filter Command Documentation

Based on paper - **HeavyKeeper: An Accurate Algorithm for Finding Top-k Elephant Flows.**

Paper and additional information can be found [here](https://www.usenix.org/conference/atc18/presentation/gong).

***

## TOPK.RESERVE

Initializes a TopK with specified parameters.

```sql
BF.RESERVE key topk width depth decay
```

### Parameters

* **key**: Key under which the sketch is to be found.
* **topk**: Number of top occurring items to keep.
* **width**: Number of counters kept in each array.
* **Depth**: Number of arrays.
* **Decay**: The probability of reducing a counter in an occupied bucket. It is raised to power of it's counter (decay ^ bucket[i].counter). Therefore, as the counter gets higher, the chance of a reduction is being reduced.

### Complexity

O(1)

### Return

OK on success, error otherwise

#### Reserve example

```sql
TOPK.RESERVE test 50 2000 7 0.925
```

***

## TOPK.ADD

Adds an item to the data structure.

```sql
TOPK.ADD key item [item ...]
```

### Parameters

* **key**: Name of sketch where item is added.
* **item**: Item to be added.

### Complexity

O(k + depth)

### Return

OK on success

#### Reserve example

```sql
TOPK.ADD test foo bar 42
```

***

## TOPK.QUERY

Checks whether an item is one of Top-K items.

```sql
TOPK.QUERY key item [item ...]
```

### Parameters

* **key**: Name of sketch where item is queried.
* **item**: Item to be queried.

### Complexity

O(k)

### Return

1 if item is in Top-K, otherwise 0.

#### Reserve example

```sql
TOPK.QUERY test 42 nonexist
1) (integer) 1
2) (integer) 0
```

***

## TOPK.COUNT

Returns count for an item. Please note this number will never be higher than the real count and likely to be lower.

```sql
TOPK.COUNT key item [item ...]
```

### Parameters

* **key**: Name of sketch where item is counted.
* **item**: Item to be counted.

### Complexity

O(k + depth)

### Return

1 if item is in Top-K, otherwise 0.

#### Reserve example

```sql
TOPK.COUNT test foo 42 nonexist
1) (integer) 3
2) (integer) 1
3) (integer) 0
```

***

## TOPK.LIST

Return full list of items in Top K list.

```sql
TOPK.LIST key
```

### Parameters

* **key**: Name of sketch where item is counted.

### Complexity

O(k)

### Return

k (or less) items in Top K list.

#### Reserve example

```sql
TOPK.LIST test
1) foo
2) 42
3) bar
```

***

## TOPK.INFO

Returns number of required items (k), width, depth and decay values.

```sql
TOPK.INFO key
```

### Parameters

* **key**: Name of sketch.

### Complexity

O(1)

### Return

Information.

#### Reserve example

```sql
TOPK.INFO test
1) k
2) (integer) 50
3) width
4) (integer) 2000
5) depth
6) (integer) 7
7) decay
8) "0.92500000000000004"
```