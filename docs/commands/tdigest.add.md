Adds one or more observations to a t-digest sketch.

#### Parameters:

* **key**: The key name of the sketch (an existing t-digest data structure)
* **value**: A value of an observation (floating-point).

@return

@simple-string-reply - `OK` if executed correctly, or @error-reply otherwise.

@examples

```
redis> TDIGEST.ADD t 1 2 3
OK
```

```
redis> TDIGEST.ADD t string
(error) ERR T-Digest: error parsing val parameter
```
