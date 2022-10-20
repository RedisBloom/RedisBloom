Returns compression, capacity, total merged and unmerged nodes, the total compressions 
made up to date on that key, and merged and unmerged weight.


## Required arguments

<details open><summary><code>key</code></summary> 

is key name for an existing t-digest sketch.
</details>

## Return value

@array-reply with information about the sketch.

## Examples

```
redis> tdigest.info t-digest
 1) Compression
 2) (integer) 100
 3) Capacity
 4) (integer) 610
 5) Merged nodes
 6) (integer) 3
 7) Unmerged nodes
 8) (integer) 2
 9) Merged weight
10) "120"
11) Unmerged weight
12) "1000"
13) Sum weights
14) "10"
15) Total compressions
16) (integer) 1
17) Memory usage
18) (integer) 96168
```
