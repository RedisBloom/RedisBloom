
Increases the count of item by increment. Multiple items can be increased with one call. 

### Parameters:

* **key**: The name of the sketch.
* **item**: The item which counter is to be increased.
* **increment**: Amount by which the item counter is to be increased.

@return


@array-reply of @integer-reply with an updated min-count of each of the items in the sketch.

Count of each item after increment.

@examples

```
redis> CMS.INCRBY test foo 10 bar 42
1) (integer) 10
2) (integer) 42
```
