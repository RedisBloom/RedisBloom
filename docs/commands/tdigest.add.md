Adds one or more samples to a sketch.

#### Parameters:

* **key**: The name of the sketch.
* **val**: The value to add.
* **weight**: The weight of this point.

@return

@simple-string-reply - `OK` if executed correctly, or @error-reply otherwise.

@examples

```
redis> TDIGEST.ADD t-digest 42 1 194 0.3
OK
```
```
redis> TDIGEST.ADD t-digest string 1.0
(error) ERR T-Digest: error parsing val parameter
```
```
redis> TDIGEST.ADD t-digest 42 string
(error) ERR T-Digest: error parsing weight parameter
```