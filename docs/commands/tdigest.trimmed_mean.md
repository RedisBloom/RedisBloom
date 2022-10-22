Estimate the mean value from the sketch, excluding observation values outside the low and high cutoff quantiles.

## Required arguments

<details open><summary><code>key</code></summary> 
is key name for an existing t-digest sketch.
</details>

<details open><summary><code>low_cut_quantile</code></summary> 
  
Exclude observation values lower than this quantile.

Foating-point value in the range [0..1], should be lower than `high_cut_quantile`
</details>

<details open><summary><code>high_cut_quantile</code></summary> 
  
Exclude observation values higher than this quantile.

Floating-point value in the range [0..1], should be higher than `low_cut_quantile`  
  
</details>

## Return value

@simple-string-reply estimation of the mean value. 'nan' if the sketch is empty.

## Examples

{{< highlight bash >}}
redis> TDIGEST.CREATE t COMPRESSION 1000
OK
redis> TDIGEST.ADD t 1 2 3 4 5 6 7 8 9 10
OK
redis> TDIGEST.TRIMMED_MEAN t 0.1 0.6
"4"
redis> TDIGEST.TRIMMED_MEAN t 0.3 0.9
"6.5"
redis> TDIGEST.TRIMMED_MEAN t 0 1
"5.5"

{{< / highlight >}}
