from common import *

class testResp3():
    def __init__(self):
        self.env = Env(protocol=3)

    def test_bf_resp3(self):
        env = self.env
        env.cmd('FLUSHALL')
        res = env.cmd('bf.add', 'test', 'item')
        assert res == True

        res = env.cmd('bf.card', 'test')
        assert res == 1

        res = env.cmd('bf.EXISTS', 'test', 'item')
        assert res == True

        res = env.cmd('bf.info', 'test')
        assert res == {b'Capacity': 100, b'Size': 240, b'Number of filters': 1,
         b'Number of items inserted': 1, b'Expansion rate': 2}

        res = env.cmd('bf.insert', 'test', 'ITEMS', 'item2', 'item3')
        assert res == [True, True]

        res = env.cmd('bf.madd', 'test', 'item4', 'item5')
        assert res == [True, True]

        res = env.cmd('bf.MEXISTS', 'test', 'item4', 'item5', 'item6')
        assert res == [True, True, False]
