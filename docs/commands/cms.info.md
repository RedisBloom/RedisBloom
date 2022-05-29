Returns width, depth and total count of the sketch.

### Parameters:

* **key**: The name of the sketch.

@return

@array-reply with information of the filter.

@examples

```
redis> CMS.INFO test
 1) width
 2) (integer) 2000
 3) depth
 4) (integer) 7
 5) count
 6) (integer) 0
```
