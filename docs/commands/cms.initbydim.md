Initializes a Count-Min Sketch to dimensions specified by user.

### Parameters:

* **key**: The name of the sketch.
* **width**: Number of counters in each array. Reduces the error size.
* **depth**: Number of counter-arrays. Reduces the probability for an
    error of a certain size (percentage of total count).

@return

@simple-string-reply - `OK` if executed correctly, or @error-reply otherwise.

@examples

```
redis> CMS.INITBYDIM test 2000 5
OK
```