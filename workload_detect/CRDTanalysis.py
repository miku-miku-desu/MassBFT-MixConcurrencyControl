import random
import threading
import time
from typing import Type, TypeVar

T = TypeVar('T')


class CRDTAnalyzer:
    chaincode_class: T
    db_class: T

    def __init__(self, cc_class, db_class):
        self.chaincode_class = cc_class
        self.db_class = db_class

    def check_commutativity(self, func_a, func_b, func_c=None, times=10000):

        db = self.db_class(self.chaincode_class.random_data_generator)
        cc = self.chaincode_class(db)

        success = 0
        failed = 0

        for _ in range(0, times):

            seed = time.time_ns()
            db.seed(seed)

            cc_a_args = self.chaincode_class.random_operator_generator(func_a)
            cc_b_args = self.chaincode_class.random_operator_generator(func_b)

            if func_c:
                cc_c_args = self.chaincode_class.random_operator_generator(func_c)

                db.reset_all()
                cc.reset()

                # a + b + c
                cc.invoke(func_a, cc_a_args)
                cc.invoke(func_b, cc_b_args)
                cc.invoke(func_c, cc_c_args)

                state_1 = db.get_state()
                db.reset_all()
                cc.reset()

                # a + c + b
                cc.invoke(func_a, cc_a_args)
                cc.invoke(func_c, cc_c_args)
                cc.invoke(func_b, cc_b_args)

                state_2 = db.get_state()
                db.reset_all()
                cc.reset()

                # b + a + c
                cc.invoke(func_b, cc_b_args)
                cc.invoke(func_a, cc_a_args)
                cc.invoke(func_c, cc_c_args)

                state_3 = db.get_state()
                db.reset_all()
                cc.reset()

                # b + c + a
                cc.invoke(func_b, cc_b_args)
                cc.invoke(func_c, cc_c_args)
                cc.invoke(func_a, cc_a_args)

                state_4 = db.get_state()
                db.reset_all()
                cc.reset()

                # c + a + b
                cc.invoke(func_c, cc_c_args)
                cc.invoke(func_a, cc_a_args)
                cc.invoke(func_b, cc_b_args)

                state_5 = db.get_state()
                db.reset_all()
                cc.reset()

                # c + b + a
                cc.invoke(func_c, cc_c_args)
                cc.invoke(func_b, cc_b_args)
                cc.invoke(func_a, cc_a_args)

                state_6 = db.get_state()

                if state_1 == state_2 == state_3 == state_4 == state_5 == state_6:
                    success += 1
                else:
                    failed += 1
                continue

            db.reset_all()
            cc.reset()

            # a then b
            cc.invoke(func_a, cc_a_args)
            cc.invoke(func_b, cc_b_args)

            state_a = db.get_state()

            db.reset_all()
            cc.reset()

            # b then a
            cc.invoke(func_b, cc_b_args)
            cc.invoke(func_a, cc_a_args)

            state_b = db.get_state()

            if state_a == state_b:
                success += 1
            else:
                failed += 1

        return success, failed

    def check_associativity(self, func_a, func_b, func_c, times=10000):

        db = self.db_class(self.chaincode_class.random_data_generator)
        cc = self.chaincode_class(db)

        success = 0
        failed = 0

        for _ in range(0, times):

            seed = time.time_ns()
            db.seed(seed)

            cc_a_args = self.chaincode_class.random_operator_generator(func_a)
            cc_b_args = self.chaincode_class.random_operator_generator(func_b)
            cc_c_args = self.chaincode_class.random_operator_generator(func_c)

            db.reset_all()
            cc.reset()

            # (a + b) + c
            ta = threading.Thread(target=cc.invoke, args=(func_a, cc_a_args))
            tb = threading.Thread(target=cc.invoke, args=(func_b, cc_b_args))
            ta.start()
            tb.start()
            ta.join()
            tb.join()
            cc.invoke(func_c, cc_c_args)

            state_a = db.get_state()

            db.reset_all()
            cc.reset()

            # a + (b + c)
            tb = threading.Thread(target=cc.invoke, args=(func_b, cc_b_args))
            tc = threading.Thread(target=cc.invoke, args=(func_c, cc_c_args))
            tb.start()
            tc.start()
            tb.join()
            tc.join()
            cc.invoke(func_a, cc_a_args)

            state_b = db.get_state()

            if state_a == state_b:
                success += 1
            else:
                failed += 1

        return success, failed

    # useless for blockchain because consensus make sure that one transaction will not be invoked twice
    def check_idempotence(self, func, times=100):

        db = self.db_class(self.chaincode_class.random_data_generator)
        cc = self.chaincode_class(db)

        success = 0
        failed = 0

        for _ in range(0, times):
            seed = time.time_ns()
            db.seed(seed)

            cc_a_args = self.chaincode_class.random_operator_generator(func)

            db.reset_all()
            cc.reset()

            # a + a
            cc.invoke(func, cc_a_args)
            cc.invoke(func, cc_a_args)

            state_a = db.get_state()

            db.reset_all()
            cc.reset()

            # a
            cc.invoke(func, cc_a_args)

            state_b = db.get_state()

            if state_a == state_b:
                success += 1
            else:
                failed += 1

        return success, failed

    def analyze_all(self):
        if len(self.chaincode_class.function_list()) >= 3:
            for func_a in self.chaincode_class.function_list():
                for func_b in self.chaincode_class.function_list():
                    for func_c in self.chaincode_class.function_list():
                        print(f"checking: {func_a} {func_b} {func_c}")
                        success, failed = self.check_commutativity(func_a, func_b, func_c)
                        print(f"commutativity: {success} {failed}, result: {failed == 0}")
                        success, failed = self.check_associativity(func_a, func_b, func_c)
                        print(f"associativity: {success} {failed}, result: {failed == 0}")
        if len(self.chaincode_class.function_list()) <= 2:
            for func_a in self.chaincode_class.function_list():
                for func_b in self.chaincode_class.function_list():
                    print(f"checking: {func_a} {func_b}")
                    success, failed = self.check_commutativity(func_a, func_b)
                    print(f"commutativity: {success} {failed}, result: {failed == 0}")
                    success, failed = self.check_associativity(func_a, func_b, func_b)
                    print(f"associativity: {success} {failed}, result: {failed == 0}")


