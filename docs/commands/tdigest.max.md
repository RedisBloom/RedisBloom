Get maximum value from the sketch. Will return __DBL_MIN__ if the sketch is empty.

#### Parameters:

* **key**: The name of the sketch.

@return

@simple-string-reply of MAXIMUM value from the sketch.
Will return __DBL_MIN__ if the sketch is empty.

@examples

```
redis> TDIGEST.MAX t-digest
"10"
```
