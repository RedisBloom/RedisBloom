import requests
import redis
import time

redis = redis.Redis(host='localhost', port=6379, db=0)
redis_pipe = redis.pipeline()
redis.flushall()

start_time = time.time()

q = 16000000
redis.execute_command('CF.RESERVE cf ' + str(q / 64) + ' MAXITERATIONS 50')

print 'add'
batches = 100
for i in range(batches):
  for x in xrange(q / batches):
    redis_pipe.execute_command('cf.add cf ' + str(x + q / batches * i))
  redis_pipe.execute()

print("--- %s seconds to add ---" % (time.time() - start_time))
start_time = time.time()
'''
print 'check'
for x in xrange(q):
  redis_pipe.execute_command('cf.exists cf', str(x))
responses = redis_pipe.execute()
for response in responses:
	pass

print("--- %s seconds to check ---" % (time.time() - start_time))
start_time = time.time()
'''
print redis.execute_command('cf.debug cf')

redis.execute_command('cf.compact cf')
print("--- %s seconds to compact ---" % (time.time() - start_time))
start_time = time.time()

print redis.execute_command('cf.debug cf')