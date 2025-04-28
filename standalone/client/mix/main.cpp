//
// Created by miku on 25-4-28.
//


# include "client/mix/mix_engine.h"


int main(int argc, char* argv[]) {
  util::OpenSSLSHA256::initCrypto();
  util::OpenSSLED25519::initCrypto();
  util::Properties::LoadProperties("peer.yaml");

  auto p = util::Properties::GetProperties();
  client::mix::MixEngine engine(*p);
  engine.startTest();
  return 0;
}

