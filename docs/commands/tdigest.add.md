Adds one or more observations to a t-digest sketch.

#### Parameters:

* **key**: The name of the sketch (a t-digest data structure)
* **val**: The value of the observation (floating-point)
* **weight**: The weight of this observation (positive integer). **weight** can indicate the number of observations with such value

@return

@simple-string-reply - `OK` if executed correctly, or @error-reply otherwise.

@examples

```
redis> TDIGEST.ADD t-digest 42 1 194 1
OK
```
```
redis> TDIGEST.ADD t-digest string 1
(error) ERR T-Digest: error parsing val parameter
```
```
redis> TDIGEST.ADD t-digest 42 string
(error) ERR T-Digest: error parsing weight parameter
```
