Estimate the mean value from the sketch, excluding observation values outside the low and high cutoff quantiles.

#### Parameters:

* **key**: The name of the sketch (a t-digest data structure)
* **low_cut_quantile**: Exclude observation values lower than this quantile
* **high_cut_quantile**: Exclude observation values higher than this quantile

@return

@simple-string-reply estimation of the mean value.
Will return __DBL_MAX__ if the sketch is empty.

@examples

```
redis> TDIGEST.TRIMMED_MEAN t-digest 0.1 0.9
"9.500001"
```
