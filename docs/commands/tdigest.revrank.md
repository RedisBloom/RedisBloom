Retrieve, for each floating-point input value, the estimated reverse rank of the value (the number of observations in the sketch that are larger than the value + half the number of observations that are equal to the value).

Multiple reverse ranks can be retrieved in a signle call.

## Required arguments
<details open><summary><code>key</code></summary>
is key name for an existing t-digest sketch.
</details>

<details open><summary><code>value</code></summary>
is input value for which the reverse rank should be estimated.

## Return value

@array-reply - an array of results populated with revrank_1, revrank_2, ..., revrank_N:
  
- -1 - when value is larger than the value of the largest observation
- The number of observations - when value is smaller than the value of the smallest observation
- Otherwise: an estimation of the number of (observations larger than a given value + half the observations equal to the given value).
  
The reverse rank of the largest observation, when there is a single largest observation, is 0.  

## Examples

{{< highlight bash >}}
redis> TDIGEST.CREATE t
OK
redis> TDIGEST.ADD t 10 20 30 40 50
OK
redis> TDIGEST.REVRANK t 0 10 20 30 40 50 60
1) (integer) 5
2) (integer) 4
3) (integer) 3
4) (integer) 2
5) (integer) 1
6) (integer) 0
7) (integer) -1
{{< / highlight >}}
  
redis> TDIGEST.CREATE s
OK
redis> TDIGEST.ADD s 10 20 20 20 20
OK
redis> TDIGEST.REVRANK s 20
1) (integer) 2
  
