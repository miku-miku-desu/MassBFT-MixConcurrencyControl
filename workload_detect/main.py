import CRDTanalysis
import database
import chaincode


if __name__ == "__main__":

    analyzer = CRDTanalysis.CRDTAnalyzer(chaincode.NonCRDTChaincode, database.Database)

    analyzer.analyze_all()
