Returns information and statistics about a t-digest sketch.

## Required arguments

<details open><summary><code>key</code></summary> 

is key name for an existing t-digest sketch.
</details>

## Return value

@array-reply with information about the sketch (name-value pairs):

| Name<br>@simple-string-reply | Description
| ---------------------------- | -
| `Compression`        | @integer-reply<br> The compression (controllable trade-off between accuracy and memory consumption) of the sketch 
| `Capacity`           | @integer-reply<br> Size of the buffer used for storing the centroids and for the incoming unmerged observations
| `Merged nodes`       | @integer-reply<br> Number of merged observations
| `Unmerged nodes`     | @integer-reply<br> Number of buffered nodes (uncompressed observations)
| `Merged weight`      | @integer-reply<br> Weight of values of the merged nodes
| `Unmerged weight`    | @integer-reply<br> Weight of values of the unmerged nodes (uncompressed observations)
| `Observations`       | @integer-reply<br> Number of observations added to the sketch
| `Total compressions` | @integer-reply<br> Number of times this sketch compressed data together
| `Memory usage`       | @integer-reply<br> Number of bytes allocated for the sketch

## Examples

{{< highlight bash >}}
redis> TDIGEST.CREATE t
OK
redis> TDIGEST.ADD t 1 2 3 4 5
OK
redis> TDIGEST.INFO t
 1) Compression
 2) (integer) 100
 3) Capacity
 4) (integer) 610
 5) Merged nodes
 6) (integer) 0
 7) Unmerged nodes
 8) (integer) 5
 9) Merged weight
10) (integer) 0
11) Unmerged weight
12) (integer) 5
13) Observations
14) (integer) 5
15) Total compressions
16) (integer) 0
17) Memory usage
18) (integer) 9768
{{< / highlight >}}
