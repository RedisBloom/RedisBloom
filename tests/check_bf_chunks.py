import requests
import redis
import time

def detect(list_a, list_b):
	return len(set(list_a).intersection(list_b)) / float(len(list_a))

redis = redis.Redis(host='localhost', port=6379, db=0)
redis_pipe = redis.pipeline()
redis.flushall()

error_rate = 0.001
quantity = 10000000
start_time = time.time()
redis.execute_command('bf.reserve', 'myBloom', error_rate, quantity)

def do_verify(add):
  false_positives = 0.0
  if add is True:
    print 'add'
    for x in xrange(quantity):
      redis_pipe.execute_command('bf.add', 'myBloom', x)
  redis_pipe.execute()
  for x in xrange(quantity):
    redis_pipe.execute_command('bf.exists', 'myBloom', x)
  print 'exist inserted'
  responses = redis_pipe.execute()
  for rv in responses:  
    assert rv is not None
  for x in xrange(quantity):
    redis_pipe.execute_command('bf.exists', 'myBloom', 'nonexist_{}'.format(x))
  print 'exist not inserted'
  responses = redis_pipe.execute()
  for rv in responses:
    if  rv == 1:
      false_positives += 1
  print false_positives
  print error_rate
  assert false_positives/quantity < error_rate

do_verify(True)
cmds = []
cur = redis.execute_command('bf.scandump', 'myBloom', 0)
first = cur[0]
cmds.append(cur)

while True:
    cur = redis.execute_command('bf.scandump', 'myBloom', first)
    first = cur[0]
    if first == 0:
        break
    else:
        cmds.append(cur)
        print("Scaning chunk... (P={}. Len={})".format(cur[0], len(cur[1])))

prev_info = redis.execute_command('bf.debug', 'myBloom')
# Remove the filter
redis.execute_command('del', 'myBloom')

# Now, load all the commands:
for cmd in cmds:
    print("Loading chunk... (P={}. Len={})".format(cmd[0], len(cmd[1])))
    redis.execute_command('bf.loadchunk', 'myBloom', *cmd)

cur_info = redis.execute_command('bf.debug', 'myBloom')
assert prev_info == cur_info
do_verify(False)