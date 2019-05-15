# RedisBloom Count-Min Sketch Documentation

## Create

### CMS.RESERVE

Initializes a Count-Min Sketch to the to accommodate requested capacity.

```sql
CMS.RESERVE key capacity [PROBABILITY error]
```

### Parameters:

* **key**: The name of the sketch.
* **capacity**: The number of unique entries you intend to count using sketch.
* **error**: The desired probability for inflated count. This should
    be a decimal value between 0 and 1. For example, for a desired false
    positive rate of 0.1% (1 in 1000), error_rate should be set to 0.001.
    The closer this number is to zero, the greater the memory consumption per
    item and the more CPU usage per operation.
    
### Complexity

O(1)

### Return

OK on success, error otherwise

#### Reserve example

```sql
CMS.RESERVE test 1000
```

## Update

### CMS.INCRBY

Increases the count of item by increment. Multiple items can be increase with one call. 

```sql
CMS.INCRBY key item increment [item increment]
```

### Parameters:

* **key**: The name of the sketch.
* **item**: The item which counter to be increased.
* **increment**: Counter to be increased by this integer.

### Complexity

O(1)

### Return

OK on success

#### Incrby example

```sql
CMS.INCRBY test foo 10 bar 42
```

## Query

### CMS.QUERY

Increases the count of item by increment. Multiple items can be increase with one call. 

```sql
CMS.QUERY {key} item increment [item increment]
```

### Parameters:

* **key**: The name of the sketch.
* **item**: The item which counter to be increased.
* **increment**: Counter to be increased by this integer.

### Complexity

O(1)

### Return

OK on success

#### Query example 

```sql
127.0.0.1:6379> CMS.QUERY test foo bar
1) (integer) 10
2) (integer) 42
```

## Merge

### CMS.MERGE

Merges several sketches into one sketch. All sketches must have identical width and depth. Weights can be used to multiply certain sketches. Default weight is 1. 

```sql
CMS.MERGE dest numKeys src1 [src2 ...] [WEIGHT weight1 ...] 
```

### Parameters:

* **dest**: The name of destination sketch. Must be initialized. 
* **numKeys**: Number of sketches to be merged.
* **src**: Names of source sketches to be merged.
* **weights**: Multiple of each sketch. Default =1.

### Complexity

O(n)

### Return

OK on success

#### Merge example 

```sql
CMS.MERGE merge 2 test1 test2 WEIGHTS 1 3
```

## General

### CMS.INFO

Returns width, depth, total count and approximate fill rate 
percentage in the sketch.

```sql
CMS.INFO key
```

### Parameters:

* **key**: The name of the sketch.

### Complexity

O(1)

#### Info Example

```sql
127.0.0.1:6379> CMS.INFO test
 1) capacity
 2) (integer) 1000
 3) width
 4) (integer) 2700
 5) depth
 6) (integer) 5
 7) count
 8) (integer) 0
 9) fill rate %
10) "0"
```