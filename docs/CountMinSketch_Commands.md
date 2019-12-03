# RedisBloom Count-Min Sketch Documentation

## Create

### CMS.INITBYDIM

Initializes a Count-Min Sketch to dimensions specified by user.

```sql
CMS.INITBYDIM key width depth
```

### Parameters:

* **key**: The name of the sketch.
* **width**: Number of counter in each array. Reduces the error size.
* **depth**: Number of counter-arrays. Reduces the probability for an
    error of a certain size (percentage of total count).
    
### Complexity

O(1)

### Return

OK on success, error otherwise

#### Example

```sql
CMS.INITBYDIM test 2000 5
```

### CMS.INITBYPROB

Initializes a Count-Min Sketch to accommodate requested capacity.

```sql
CMS.INITBYPROB key error probability
```

### Parameters:

* **key**: The name of the sketch.
* **error**: Estimate size of error. The error is a percent of total counted
    items. This effects the width of the sketch.
* **probability**: The desired probability for inflated count. This should
    be a decimal value between 0 and 1. This effects the depth of the sketch.
    For example, for a desired false positive rate of 0.1% (1 in 1000),
    error_rate should be set to 0.001. The closer this number is to zero, the
    greater the memory consumption per item and the more CPU usage per operation. 
    
### Complexity

O(1)

### Return

OK on success, error otherwise

#### Example

```sql
CMS.INITBYPROB test 0.001 0.01
```

## Update

### CMS.INCRBY

Increases the count of item by increment. Multiple items can be increased with one call. 

```sql
CMS.INCRBY key item increment [item increment ...]
```

### Parameters:

* **key**: The name of the sketch.
* **item**: The item which counter to be increased.
* **increment**: Counter to be increased by this integer.

### Complexity

O(1)

### Return

Count of each item after increment.

#### Example

```sql
CMS.INCRBY test foo 10 bar 42
```

## Query

### CMS.QUERY

Returns count for item. Multiple items can be queried with one call. 

```sql
CMS.QUERY key item [item ...]
```

### Parameters:

* **key**: The name of the sketch.
* **item**: The item which counter to be increased.
* **increment**: Counter to be increased by this integer.

### Complexity

O(1)

### Return

Count of one or more items

#### Example 

```sql
127.0.0.1:6379> CMS.QUERY test foo bar
1) (integer) 10
2) (integer) 42
```

## Merge

### CMS.MERGE

Merges several sketches into one sketch. All sketches must have identical width and depth. Weights can be used to multiply certain sketches. Default weight is 1. 

```sql
CMS.MERGE dest numKeys src1 [src2 ...] [WEIGHTS weight1 ...] 
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

#### Example 

```sql
CMS.MERGE dest 2 test1 test2 WEIGHTS 1 3
```

## General

### CMS.INFO

Returns width, depth and total count of the sketch.

```sql
CMS.INFO key
```

### Parameters:

* **key**: The name of the sketch.

### Complexity

O(n) due to fill rate percentage

#### Example

```sql
127.0.0.1:6379> CMS.INFO test
 1) width
 2) (integer) 2000
 3) depth
 4) (integer) 7
 5) count
 6) (integer) 0
```