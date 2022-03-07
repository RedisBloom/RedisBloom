Merges all of the values from 'from' to 'this' sketch.

#### Parameters:

* **to-key**: Sketch to copy values to.
* **from-key**: Sketch to copy values from.

@return

OK on success, error otherwise

@examples

```
redis> TDIGEST.MERGE to-sketch from-sketch
OK
```