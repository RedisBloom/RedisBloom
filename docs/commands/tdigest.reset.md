Resets a t-digest sketch: empty the sketch and re-initializes it.

## Required arguments
<details open><summary><code>key</code></summary>
is key name for an existing t-digest sketch.
</details>

## Return value

@simple-string-reply - `OK` if executed correctly, or @error-reply otherwise.

## Examples

{{< highlight bash >}}
redis> TDIGEST.RESET t
OK
{{< / highlight >}}
