Returns an estimate of the cutoff such that a specified fraction of the data
added to this TDigest would be less than or equal to the specified cutoffs.

Multiple quantiles can be returned with one call.


#### Parameters:

* **key**: The name of the sketch.
* **quantile**: The desired fraction ( between 0 and 1 inclusively ).

@return

@array-reply - the command returns an array of results populated with quantile_1, cutoff_1, quantile_2, cutoff_2, ..., quantile_N, cutoff_N.

The returned array of results will be ordered in ascending order by quantile.

@examples

```
redis> TDIGEST.QUANTILE t-digest 0.5
1) "0.5"
2) "100.42"
```
```
redis> TDIGEST.QUANTILE t-digest 0.5 0.999
1) "0.5"
2) "100.42"
3) "0.999"
4) "190.01"
```