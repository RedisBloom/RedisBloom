Allocates memory and initializes a new t-digest sketch.

## Required arguments

<details open><summary><code>key</code></summary> 
is key name for this new t-digest sketch.
</details>

## Optional arguments

<details open><summary><code>COMPRESSION compression</code></summary> 

is a controllable tradeoff between accuracy and memory consumption. 100 is a common value for normal uses. 1000 is more accurate. If no value is passed by default the compression will be 100. For more information on scaling of accuracy versus the compression parameter see [_The t-digest: Efficient estimates of distributions_](https://www.sciencedirect.com/science/article/pii/S2665963820300403).

</details>
  
## Return value

@simple-string-reply - `OK` if executed correctly, or @error-reply otherwise.

## Examples

{{< highlight bash >}}
redis> TDIGEST.CREATE t COMPRESSION 100
OK
{{< / highlight >}}
