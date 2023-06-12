//
// Created by root on 3/16/23.
//
#include "hash.h"
#include <memory>
#include <openssl/ossl_typ.h>
#include <openssl/sha.h>
#include <glog/logging.h>
using primihub::Hash;

std::string Hash::hash_str(const std::string &msg) {
  unsigned int dgstlen = EVP_MD_meth_get_result_size(md_);
  std::string hash_res;
  if (0 == dgstlen) {
    LOG(ERROR) << "unexpected size from EVP_MD_meth_get_result_size";
    return hash_res;
  }
  auto dgst = std::make_unique<unsigned char[]>(dgstlen);
  memset(dgst.get(), 0x0, dgstlen);
  int ret = EVP_Digest(reinterpret_cast<const unsigned char *>(msg.c_str()), msg.size(),
                       dgst.get(), &dgstlen, md_, nullptr);
  if (!ret) {
    LOG(ERROR) << "EVP_Digest return err" << ret;
  }
  hash_res = std::string(reinterpret_cast<char*>(dgst.get()), dgstlen);
  return hash_res;
}
