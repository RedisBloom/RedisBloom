Returns the maximum observation value from a sketch.

## Required arguments

<details open><summary><code>key</code></summary>
is key name for an existing t-digest sketch.
</details>

## Return value

@simple-string-reply of maximum observation value from a sketch. The result is always accurate. 'nan' if the sketch is empty.

## Examples

{{< highlight bash >}}
redis> TDIGEST.CREATE t
OK
redis> TDIGEST.MAX t
"nan"
redis> TDIGEST.ADD t 3 4 1 2 5
OK
redis>TDIGEST.MAX t
"5"
{{< / highlight >}}
