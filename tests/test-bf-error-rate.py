import requests
import redis
import time

redis = redis.Redis(host='localhost', port=6379, db=0)
redis_pipe = redis.pipeline()
redis.flushall()

start_time = time.time()
error_rate = 0.001
q = 10000

redis.execute_command('BF.RESERVE bloom_filter ', error_rate, q / 10)

for x in range(q):
  redis_pipe.execute_command('bf.add bloom_filter ' + str(x))
redis_pipe.execute()

for x in range(q):
  redis_pipe.execute_command('bf.exists bloom_filter ' + str(x))
responses = redis_pipe.execute()

for response in responses:
  assert(response == 1)

print 'Test for False Positives'
for x in range(q, q * 101):
  redis_pipe.execute_command('bf.exists bloom_filter ' + str(x))
responses = redis_pipe.execute()

false_positive = 0.0
for response in responses:
  if response == 1:
    false_positive += 1

print 'Error rate required ' + str(error_rate) + ', actual ' + str(false_positive / (q * 100))

print redis.execute_command('bf.debug bloom_filter')