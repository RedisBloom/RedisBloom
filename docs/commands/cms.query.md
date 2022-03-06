Returns the count for one or more items in a sketch.

### Parameters:

* **key**: The name of the sketch.
* **item**: One or more items for which to return the count.

@return

Count of one or more items

@array-reply of @integer-reply with a min-count of each of the items in the sketch.

@examples

```
redis> CMS.QUERY test foo bar
1) (integer) 10
2) (integer) 42
```