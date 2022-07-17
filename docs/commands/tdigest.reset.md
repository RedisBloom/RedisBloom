Reset the sketch - empty the sketch and re-initialize it.

#### Parameters:

* **key**: The name of the sketch (a t-digest data structure)

@return

@simple-string-reply - `OK` if executed correctly, or @error-reply otherwise.

@examples

```
redis> TDIGEST.RESET t-digest
OK
```
