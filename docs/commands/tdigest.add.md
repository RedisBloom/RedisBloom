Adds one or more observations to a t-digest sketch.

#### Parameters:

* **key** is key name for an existing t-digest data structure.
* **value**: is value of an observation (floating-point).

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
