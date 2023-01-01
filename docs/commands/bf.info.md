Return information about `key` filter.

### Parameters

* **key**: Name of the key to return information about

Optional parameters:
* **CAPACITY** Number of supposedly unique items that can be stored in this Bloom Filter before scaling would be required
* **SIZE** Number of bytes allocated for this Bloom Filter
* **FILTERS** Number of filters
* **ITEMS** Number of supposedly unique items added to this Bloom Filter (items that caused at least one bit to be set in at least one filter)
* **EXPANSION** Expansion rate

@return

@array-reply with information of the filter.

@examples

```sql
redis> BF.ADD key item
(integer) 1
redis> BF.INFO key
 1) Capacity
 2) (integer) 100
 3) Size
 4) (integer) 296
 5) Number of filters
 6) (integer) 1
 7) Number of items inserted
 8) (integer) 1
 9) Expansion rate
10) (integer) 2
redis> BF.INFO key CAPACITY
1) (integer) 100
```
