import copy
import random
from typing import Dict, List, Callable


# database prototype


class KV:
    key: str
    value: str

    def __init__(self, key: str = "", value: str = ""):
        self.key = key
        self.value = value


class KVList:

    data: List[KV]

    def __init__(self):
        self.data = []

    def add(self, key, value):
        self.data.append(KV(key, value))

    def reset(self):
        self.data = []


class ReadWriteSet:
    read_set: KVList
    write_set: KVList

    def __init__(self):
        self.read_set = KVList()
        self.write_set = KVList()

    def read(self, key, value):
        self.read_set.add(key, value)

    def write(self, key, value):
        self.write_set.add(key, value)

    def reset(self):
        self.read_set.reset()
        self.write_set.reset()

    def delete(self, key):
        self.write_set.add(key, "")


# a prototype of database
class Database:
    read_write_set: ReadWriteSet
    data: Dict[str, str]
    random_data_generator: Callable[[str, str], str]
    _seed: str
    _lock: Dict[str, bool]

    def __init__(self, generator: Callable[[str, str], str]):
        self.data = {}
        self.read_write_set = ReadWriteSet()
        self.random_data_generator = generator
        self._lock = {}
        self._seed = ""

    def get(self, key: str):
        while self._lock.get(key, False):
            pass
        self.lock(key)
        value = self.data.get(key, None)
        # generator a random value for simulation
        if value is None:
            value = self.random_data_generator(self._seed, key)
            self.data[key] = value
        self.read_write_set.read(key, value)
        self.unlock(key)
        return value

    def put(self, key, value):
        while self._lock.get(key, False):
            pass
        self.lock(key)
        self.data[key] = value
        self.read_write_set.write(key, value)
        self.unlock(key)

    def delete(self, key):
        while self._lock.get(key, False):
            pass
        self.lock(key)
        self.read_write_set.delete(key)
        self.unlock(key)
        return self.data.pop(key, None)

    def reset_all(self):
        self.data = {}
        self.read_write_set.reset()

    # use for get chaincode read write set
    def reset(self) -> ReadWriteSet:
        t = self.read_write_set
        self.read_write_set = ReadWriteSet()
        return t

    def set_generator(self, generator: Callable[[str, str], str]):
        self.random_data_generator = generator

    def get_read_write_set(self):
        return self.read_write_set

    def seed(self, x):
        self._seed = str(x)

    def get_state(self):
        return copy.deepcopy(self.data)

    def lock(self, key):
        self._lock[key] = True

    def unlock(self, key):
        self._lock[key] = False


