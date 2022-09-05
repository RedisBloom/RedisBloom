Return information about `key` filter.

### Parameters

* **key**: Name of the key to return information about

@return

@array-reply with information of the filter.

@example

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
```
