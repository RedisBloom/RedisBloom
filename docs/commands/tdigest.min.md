Returns the minimum observation value from a sketch.

## Required arguments
<details open><summary><code>key</code></summary>
is key name for an existing t-digest sketch.
</details>

## Return value

@simple-string-reply of minimum observation value from a sketch. The result is always accurate. 'nan' if the sketch is empty.

## Examples

{{< highlight bash >}}
redis> TDIGEST.CREATE t
OK
redis> TDIGEST.MIN t
"nan"
redis> TDIGEST.ADD t 3 4 1 2 5
OK
redis> TDIGEST.MIN t
"1"
{{< / highlight >}}
