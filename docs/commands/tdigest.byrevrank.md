Retrieve an estimation of the value with the given the reverse rank.

Multiple estimations can be returned with one call.

#### Parameters:

* **key**: The name of the sketch (a t-digest data structure)
* **value**: input reverse rank, for which the value will be determined

@return

@array-reply - the command returns an array of results populated with value_1, value_2, ..., value_N.

@examples

```
redis> TDIGEST.BYREVRANK t-digest 100 200
1) "10"
2) "5"
```
