Estimate the fraction of all observations added which are <= value.

Multiple quantiles can be returned with one call.

#### Parameters:

* **key**: The name of the sketch (a t-digest data structure)
* **value**: upper limit of observation value, for which the fraction of all observations added which are <= value

@return

@array-reply - the command returns an array of results populated with fraction_1, fraction_2, ..., fraction_N.

@examples

```
redis> TDIGEST.CDF t-digest 10
1) "0.041666666666666664"
```
```
redis> TDIGEST.CDF t-digest 10 13
1) "0.041666666666666664"
2) "0.042"
```
```
redis> TDIGEST.QUANTILE nonexist 42
"nan"
```
