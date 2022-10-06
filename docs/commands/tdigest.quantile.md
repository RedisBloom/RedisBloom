Returns estimates of one or more cutoffs such that a specified fraction of the observations
added to this t-digest would be less than or equal to each of the specified cutoffs.

Multiple quantiles can be returned with one call.

#### Parameters:

* **key**: The name of the sketch (a t-digest data structure)
* **quantile**: The desired fraction (between 0 and 1 inclusively)

@return

@array-reply - the command returns an array of results populated with cutoff_1, cutoff_2, ..., cutoff_N.

@examples

```
redis> TDIGEST.QUANTILE t-digest 0.5
1) "100.42"
```
```
redis> TDIGEST.QUANTILE t-digest 0.5 0.999
1) "100.42"
2) "190.01"
```
