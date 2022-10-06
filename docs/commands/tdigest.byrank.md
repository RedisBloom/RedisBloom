Retrieve an estimation of the value with the given the rank.

Multiple estimations can be returned with one call.

#### Parameters:

* **key**: The name of the sketch (a t-digest data structure)
* **value**: input rank, for which the value will be determined

@return

@array-reply - the command returns an array of results populated with value_1, value_2, ..., value_N.

@examples

```
redis> TDIGEST.BYRANK t-digest 100 200
1) "5"
2) "10"
```
