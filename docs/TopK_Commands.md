# RedisBloom TopK Filter Command Documentation

Based on paper - **HeavyKeeper: An Accurate Algorithm for Finding Top-k Elephant Flows.**

Paper and additional information can be found [here](https://www.usenix.org/conference/atc18/presentation/gong).

***

Note - all counts in this data structure are probabilistic and are likely to be lower than real value.

***

## TOPK.RESERVE

Initializes a TopK with specified parameters.

```sql
TOPK.RESERVE key topk width depth decay
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

#### Example

```sql
TOPK.RESERVE test 50 2000 7 0.925
```

***

## TOPK.ADD

Adds an item to the data structure. 
Multiple items can be added at once.
If an item enters the Top-K list, the item which is expelled is returned.
This allows dynamic heavy-hitter detection of items being entered or expelled from Top-K list. 

```sql
TOPK.ADD key item [item ...]
```

### Parameters

* **key**: Name of sketch where item is added.
* **item**: Item/s to be added.

### Complexity

O(k + depth)

### Return

(nil), if no change to Top-K list occurred else, returns item dropped from list. 

#### Example

```sql
TOPK.ADD test foo bar 42
1) (nil)
2) baz
3) (nil)
```

***

## TOPK.INCRBY

Increase the score of an item in the data structure by increment. 
Multiple items' score can be increased at once.
If an item enters the Top-K list, the item which is expelled is returned.

```sql
TOPK.INCRBY key item increment [item increment ...]
```

### Parameters

* **key**: Name of sketch where item is added.
* **item**: Item/s to be added.
* **increment**: increment to current item score.

### Complexity

O(k + (increment * depth))

### Return

(nil), if no change to Top-K list occurred else, returns item dropped from list. 

#### Example

```sql
TOPK.INCRBY test foo 3 bar 2 42 30
1) (nil)
2) (nil)
3) foo
```

***

## TOPK.QUERY

Checks whether an item is one of Top-K items.
Multiple items can be checked at once.

```sql
TOPK.QUERY key item [item ...]
```

### Parameters

* **key**: Name of sketch where item is queried.
* **item**: Item/s to be queried.

### Complexity

O(k)

### Return

For each item requested, return 1 if item is in Top-K, otherwise 0.

#### Example

```sql
TOPK.QUERY test 42 nonexist
1) (integer) 1
2) (integer) 0
```

***

## TOPK.COUNT

Returns count for an item. Please note this number will never be higher than the real count and likely to be lower.
Multiple items can be added at once.

```sql
TOPK.COUNT key item [item ...]
```

### Parameters

* **key**: Name of sketch where item is counted.
* **item**: Item/s to be counted.

### Complexity

O(k + depth)

### Return

For each item requested, count for item.

#### Example

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

#### Example

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

#### Example

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
