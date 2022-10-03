Adds one or more observations to a t-digest sketch.

#### Parameters:

* **key**: The name of the sketch (a t-digest data structure)
* **val**: The value of the observation (floating-point). The value should be a finite number

@return

@simple-string-reply - `OK` if executed correctly, or @error-reply otherwise.

@examples

```
redis> TDIGEST.ADD t-digest 42 194
OK
```

```
redis> TDIGEST.ADD t-digest string 1
(error) ERR T-Digest: error parsing val parameter
```
