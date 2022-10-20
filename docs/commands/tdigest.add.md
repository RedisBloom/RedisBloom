Adds one or more observations to a t-digest sketch.

## Required arguments

<details open><summary><code>key</code></summary> 

is key name for an existing t-digest sketch.
</details>

<details open><summary><code>value</code></summary> 

is value of an observation (floating-point).
</details>

## Return value

@simple-string-reply - `OK` if executed correctly, or @error-reply otherwise.

## Examples

```
redis> TDIGEST.ADD t 1 2 3
OK
```

```
redis> TDIGEST.ADD t string
(error) ERR T-Digest: error parsing val parameter
```
