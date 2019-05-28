# RedisBloom TopK Filter Command Documentation

## TOPK.RESERVE

### Format:

```sql
BF.RESERVE key topk width depth decay
```

## TOPK.ADD

### Format:

```sql
TOPK.ADD key item [item ...]
```

## TOPK.QUERY

### Format:

```sql
TOPK.QUERY key item [item ...]
```

## TOPK.COUNT

### Format:

```sql
TOPK.COUNT key item [item ...]
```

## TOPK.LIST

### Format:

```sql
TOPK.LIST key
```

## TOPK.INFO

### Format:

```sql
TOPK.INFO key
```