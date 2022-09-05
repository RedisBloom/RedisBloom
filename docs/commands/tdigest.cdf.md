Estimate the fraction of all observations added which are <= value.

#### Parameters:

* **key**: The name of the sketch (a t-digest data structure)
* **value**: upper limit of observation value, for which the fraction of all observations added which are <= value

@return

@double-reply - estimation of the fraction of all observations added which are <= value

@examples

```
redis> TDIGEST.CDF t-digest 10
"0.041666666666666664"
```
```
redis> TDIGEST.QUANTILE nonexist 42
"nan"
```
