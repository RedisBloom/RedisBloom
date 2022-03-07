Returns compression, capacity, total merged and unmerged nodes, the total compressions 
made up to date on that key, and merged and unmerged weight.

### Parameters:

* **key**: The name of the sketch.

@return

@array-reply with information of the sketch.

@examples

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
13) Total compressions
14) (integer) 1
```