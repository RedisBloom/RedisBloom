Return information about `key` filter.

### Parameters

* **key**: Name of the key to return information about

Optional parameters:
* **CAPACITY** Capacity
* **SIZE** Size
* **FILTERS** Number of filters
* **ITEMS** Numner of items inserted
* **ERATE** Expansion rate

@return

@array-reply with information of the filter.

@examples

```sql
redis> BF.INFO bf
1) Capacity
2) (integer) 1709
3) Size
4) (integer) 2200
5) Number of filters
6) (integer) 1
7) Number of items inserted
8) (integer) 0
9) Expansion rate
10) (integer) 1
redis> BF.INFO bf CAPACITY
1) (integer) 1709
```
