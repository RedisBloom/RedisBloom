from common import *

class testResp3():
    def __init__(self):
        self.env = Env(protocol=3)

    def test_bf_resp3(self):
        env = self.env
        env.cmd('FLUSHALL')
        res = env.cmd('bf.add', 'test', 'item')
        print(res)

