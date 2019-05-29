# RedisBloom Count Min Sketch Documentation

## CMS.INITBYDIM

### Format:

```
CMS.INITBYDIM key width depth
```

Initializes a CMS to the dimensions specified by width and depth.

## Parameters:

* **key**: Key under which the sketch is to be found.
* **width**: Number of counters kept in each array.
* **Depth**: Number of arrays.

### Complexity

O(1)

### Return

OK on success, error otherwise

## CMS.INITBYPROB

### Format:

```
CMS.INITBYPROB key error probabilty
```

Initializes a CMS to the dimensions specified by error and probabilty.

## Parameters:

* **key**: Key under which the sketch is to be found.
* **error**: Maximum acceptable error given probability. Should be a double (example 0.1).
* **probabilty**: Probability of error.

### Complexity

O(1)

### Return

OK on success, error otherwise

