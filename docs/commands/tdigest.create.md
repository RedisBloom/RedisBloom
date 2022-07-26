Allocate memory and initialize a t-digest sketch.

#### Parameters:

* **key**: The name of the sketch (a t-digest data structure)


Optional parameters:

* **COMPRESSION**: The compression parameter. 100 is a common value for normal uses. 1000 is extremely large. If no value value is passed by default the compression will be 100. See the further notes bellow.



**Further notes on compression vs accuracy:**
Constructing a t-digest sketch requires a compression parameter, which determines the size of the digest and accuracy of quantile estimation. For more information on scaling of accuracy versus the compression parameter see [_The t-digest: Efficient estimates of distributions_](https://www.sciencedirect.com/science/article/pii/S2665963820300403).

@return

@simple-string-reply - `OK` if executed correctly, or @error-reply otherwise.

@examples

```
redis> TDIGEST.CREATE t-digest 100
OK
```
