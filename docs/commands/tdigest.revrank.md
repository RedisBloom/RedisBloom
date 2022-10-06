Retrieve the estimated rank of value (the number of observations in the sketch that are larger than value + half the number of observations that are equal to value).

Multiple ranks can be returned with one call.

#### Parameters:

* **key**: The name of the sketch (a t-digest data structure)
* **value**: input value, for which the rank will be determined

@return

@array-reply - the command returns an array of results populated with rank_1, rank_2, ..., rank_N.

@examples

```
redis> TDIGEST.REVRANK t-digest 5 10
1) "200"
2) "100"
```
