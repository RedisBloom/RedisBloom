# RedisBloom Count-Min Sketch Documentation

## Create

### CMS.INITBYDIM

Initializes a Count-Min Sketch to dimensions specified by user.

```
CMS.INITBYDIM {key} {width} {depth}
```

### Parameters:

* **key**: The name of the sketch.
* **width**: Number of counters in each array. Reduces the error size.
* **depth**: Number of counter-arrays. Reduces the probability for an
    error of a certain size (percentage of total count).
    
### Complexity

O(1)

### Return

OK on success, error otherwise

#### Example

```
CMS.INITBYDIM test 2000 5
```

### CMS.INITBYPROB

Initializes a Count-Min Sketch to accommodate requested tolerances.

```
CMS.INITBYPROB {key} {error} {probability}
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

```
CMS.INITBYPROB test 0.001 0.01
```

## Update

### CMS.INCRBY

Increases the count of item by increment. Multiple items can be increased with one call. 

```
CMS.INCRBY {key} {item} {increment} [{item} {increment} ...]
```

### Parameters:

* **key**: The name of the sketch.
* **item**: The item which counter is to be increased.
* **increment**: Amount by which the item counter is to be increased.

### Complexity

O(1)

### Return

Count of each item after increment.

#### Example

```
CMS.INCRBY test foo 10 bar 42
```

## Query

### CMS.QUERY

Returns count for item. Multiple items can be queried with one call. 

```
CMS.QUERY {key} {item ...}
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

```
127.0.0.1:6379> CMS.QUERY test foo bar
1) (integer) 10
2) (integer) 42
```

## Merge

### CMS.MERGE

Merges several sketches into one sketch. All sketches must have identical width and depth. Weights can be used to multiply certain sketches. Default weight is 1. 

```
CMS.MERGE {dest} {numKeys} {src ...} [WEIGHTS {weight ...}]
```

### Parameters:

* **dest**: The name of destination sketch. Must be initialized. 
* **numKeys**: Number of sketches to be merged.
* **src**: Names of source sketches to be merged.
* **weight**: Multiple of each sketch. Default =1.

### Complexity

O(n)

### Return

OK on success

#### Example 

```
CMS.MERGE dest 2 test1 test2 WEIGHTS 1 3
```

## General

### CMS.INFO

Returns width, depth and total count of the sketch.

```
CMS.INFO {key}
```

### Parameters:

* **key**: The name of the sketch.

### Complexity

O(n) due to fill rate percentage

#### Example

```
127.0.0.1:6379> CMS.INFO test
 1) width
 2) (integer) 2000
 3) depth
 4) (integer) 7
 5) count
 6) (integer) 0
```
