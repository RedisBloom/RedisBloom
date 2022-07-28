Merges all the observation values from the 'from' sketch to the 'to' sketch.

#### Parameters:

* **destination-key**: Sketch to copy observation values to (a t-digest data structure)
* **source-key**: Sketch to copy observation values from (a t-digest data structure)

@return

OK on success, error otherwise

@examples

```
redis> TDIGEST.MERGE to-sketch from-sketch
OK
```
