Get the maximum observation value from the sketch.

#### Parameters:

* **key**: The name of the sketch (a t-digest data structure)

@return

@simple-string-reply of the maximum observation value from the sketch.
Return 'nan' if the sketch is empty.

@examples

```
redis> TDIGEST.MAX t-digest
"10"
```
