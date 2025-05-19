import random
from typing import List, Tuple
import database


# chaincode prototype


# simple vote chaincode prototype
class Chaincode:

    result: str
    MAX_CANDIDATE = 100
    MAX_VOTE_COUNT = 10
    MAX_INITIAL_COUNT = 100
    VOTE_FUNC_NAME = "vote"
    GET_FUNC_NAME = "get"

    def __init__(self, db: database.Database):
        self.db = db
        self.result = ""

    # need rewrite
    def invoke(self, func: str, args: List[str]) -> bool:

        if func == self.VOTE_FUNC_NAME:
            if len(args) != 2:
                return False
            else:
                return self.vote(args[0], args[1])

        if func == self.GET_FUNC_NAME:
            if len(args) != 1:
                return False
            else:
                return self.get(args[0])

    def vote(self, candidate, count) -> bool:
        if not count.isdigit():
            return False

        count = int(count)
        old_count = int(self.db.get(candidate))
        self.db.put(candidate, str(count + old_count))
        return True

    def get(self, candidate) -> bool:
        self.result = self.db.get(candidate)
        return True

    # reset read write set and get result
    def reset(self) -> Tuple[database.ReadWriteSet, str]:
        r = self.result
        self.result = ""
        return self.db.reset(), r

    # using for database to generator simulated data
    # use seed + key as random seed, make sure the same key will generate the same data if the seed is the same
    # need rewrite
    @staticmethod
    def random_data_generator(seed: str, key: str) -> str:
        random.seed(seed + key)
        return str(random.randint(0, Chaincode.MAX_INITIAL_COUNT))

    # using for analyzer to generator simulated data, func is the chaincode function name, return args
    # need rewrite
    @staticmethod
    def random_operator_generator(func: str, seed: int = None) -> List[str]:
        if not seed:
            rand_instance = random.Random()
        else:
            rand_instance = random.Random(seed)

        if func == Chaincode.VOTE_FUNC_NAME:
            return [str(rand_instance.randint(0, Chaincode.MAX_CANDIDATE)), str(random.randint(0, Chaincode.MAX_VOTE_COUNT))]

        if func == Chaincode.GET_FUNC_NAME:
            return [str(rand_instance.randint(0, Chaincode.MAX_CANDIDATE))]

    @staticmethod
    def function_list() -> List[str]:
        return [Chaincode.VOTE_FUNC_NAME, Chaincode.GET_FUNC_NAME]


class NonCRDTChaincode:

    result: str
    MAX_KEY_SIZE = 1000
    MAX_VALUE_SIZE = 1000
    MAX_INITIAL_COUNT = 1000
    DIVIDE_FUNC_NAME = "div"
    MULTI_FUNC_NAME = "mul"
    ADD_FUNC_NAME = "add"
    SUB_FUNC_NAME = "sub"

    def __init__(self, db: database.Database):
        self.db = db
        self.result = ""

    def invoke(self, func, args):
        if len(args) != 2:
            return False

        if func == self.ADD_FUNC_NAME:
            return self.add(args[0], args[1])

        if func == self.SUB_FUNC_NAME:
            return self.sub(args[0], args[1])

        if func == self.MULTI_FUNC_NAME:
            return self.mul(args[0], args[1])

        if func == self.DIVIDE_FUNC_NAME:
            return self.div(args[0], args[1])

    def add(self, key, value):
        old_count = int(self.db.get(key))
        self.db.put(key, str(old_count + int(value)))
        return True

    def sub(self, key, value):
        old_count = int(self.db.get(key))
        self.db.put(key, str(old_count - int(value)))
        return True

    def mul(self, key, value):
        old_count = int(self.db.get(key))
        self.db.put(key, str(old_count * int(value)))
        return True

    def div(self, key, value):
        if value == "0":
            return False
        old_count = int(self.db.get(key))
        self.db.put(key, str(old_count // int(value)))
        return True

    def reset(self):
        r = self.result
        self.result = ""
        return self.db.reset(), r

    @staticmethod
    def random_data_generator(seed: str, key: str) -> str:
        random.seed(seed + key)
        return str(random.randint(0, NonCRDTChaincode.MAX_INITIAL_COUNT))

    @staticmethod
    def random_operator_generator(func: str, seed: int = None) -> List[str]:
        if not seed:
            rand_instance = random.Random()
        else:
            rand_instance = random.Random(seed)

        if func == NonCRDTChaincode.ADD_FUNC_NAME:
            return [str(rand_instance.randint(0, NonCRDTChaincode.MAX_KEY_SIZE)), str(rand_instance.randint(0, NonCRDTChaincode.MAX_VALUE_SIZE))]

        if func == NonCRDTChaincode.SUB_FUNC_NAME:
            return [str(rand_instance.randint(0, NonCRDTChaincode.MAX_KEY_SIZE)), str(rand_instance.randint(0, NonCRDTChaincode.MAX_VALUE_SIZE))]

        if func == NonCRDTChaincode.MULTI_FUNC_NAME:
            return [str(rand_instance.randint(0, NonCRDTChaincode.MAX_KEY_SIZE)), str(rand_instance.randint(0, NonCRDTChaincode.MAX_VALUE_SIZE))]

        if func == NonCRDTChaincode.DIVIDE_FUNC_NAME:
            return [str(rand_instance.randint(0, NonCRDTChaincode.MAX_KEY_SIZE)), str(rand_instance.randint(1, NonCRDTChaincode.MAX_VALUE_SIZE))]

    @staticmethod
    def function_list() -> List[str]:
        return [NonCRDTChaincode.ADD_FUNC_NAME, NonCRDTChaincode.SUB_FUNC_NAME, NonCRDTChaincode.MULTI_FUNC_NAME, NonCRDTChaincode.DIVIDE_FUNC_NAME]
