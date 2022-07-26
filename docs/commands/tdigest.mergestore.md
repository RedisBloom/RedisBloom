Merges all of the values from 'from' keys to 'destination-key' sketch.

It is mandatory to provide the number of input keys (numkeys) before passing the input keys and the other (optional) arguments.

If destination already exists, it is overwritten.

#### Parameters:

* **destination-key**: Sketch to copy observation values to (a t-digest data structure)
* **numkeys**: Sketch(es) to copy observation values from (a t-digest data structure)
* **from**: Sketch(es) to copy observation values from (a t-digest data structure)


Optional parameters:

* **COMPRESSION**: The compression parameter. 100 is a common value for normal uses. 1000 is extremely large. If no value value is passed by default the compression will be 100. For more information on scaling of accuracy versus the compression parameter see [_The t-digest: Efficient estimates of distributions_](https://www.sciencedirect.com/science/article/pii/S2665963820300403).

@return

OK on success, error otherwise

@examples

```
redis> TDIGEST.CREATE from-sketch-1
OK
redis> TDIGEST.CREATE from-sketch-2
OK
redis> TDIGEST.ADD from-sketch-1 10.0 1.0
OK
redis> TDIGEST.ADD from-sketch-2 50.0 1.0
OK
redis> TDIGEST.MERGESTORE destination-key 2 from-sketch-1 from-sketch-2
OK
```
