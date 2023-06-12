//
// Created by root on 3/16/23.
//

#ifndef PRIMIHUB_HASH_H
#define PRIMIHUB_HASH_H
#include <string>
#include <openssl/evp.h>

namespace primihub{
class Hash {
  public:
  explicit Hash(const std::string &alg = "sha256") {
    if (alg == "sha256") {
      alg_ = NID_sha256;
      md_ = EVP_sha256();
    }
  }
  bool init() {
    return (md_ != nullptr);
  }
  std::string hash_str(const std::string &msg);

  private:
  int alg_ = NID_sha256;
  const EVP_MD *md_ = nullptr;
};
}

#endif //PRIMIHUB_HASH_H
