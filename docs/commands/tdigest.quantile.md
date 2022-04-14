Returns an estimate of the cutoff such that a specified fraction of the data
added to this TDigest would be less than or equal to the cutoff.

#### Parameters:

* **key**: The name of the sketch.
* **quantile**: The desired fraction ( between 0 and 1 inclusively ).

@return

@double-reply - value estimate of the cutoff such that a specified fraction of the data
added to this TDigest would be less than or equal to the cutoff.

@examples

```
redis> TDIGEST.QUANTILE t-digest 0.5
"100.42"
```
```
redis> TDIGEST.QUANTILE nonexist 0.5
"nan"
```