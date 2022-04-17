Returns the fraction of all points added which are <= value.

#### Parameters:

* **key**: The name of the sketch.
* **quantile**: upper limit for which the fraction of all points added which are <= value.

@return

@double-reply - fraction of all points added which are <= value.

@examples

```
redis> TDIGEST.CDF t-digest 10
"0.041666666666666664"
```
```
redis> TDIGEST.QUANTILE nonexist 42
"nan"
```