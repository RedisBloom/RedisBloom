Reset the sketch to zero - empty out the sketch and re-initialize it.

#### Parameters:

* **key**: The name of the sketch.

@return

@simple-string-reply - `OK` if executed correctly, or @error-reply otherwise.

@examples

```
redis> TDIGEST.RESET t-digest
OK
```