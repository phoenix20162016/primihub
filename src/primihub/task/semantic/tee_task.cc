/*
 Copyright 2022 Primihub

 Licensed under the Apache License, Version 2.0 (the "License");
 you may not use this file except in compliance with the License.
 You may obtain a copy of the License at

      https://www.apache.org/licenses/LICENSE-2.0

 Unless required by applicable law or agreed to in writing, software
 distributed under the License is distributed on an "AS IS" BASIS,
 WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 See the License for the specific language governing permissions and
 limitations under the License.
 */
#include "src/primihub/task/semantic/tee_task.h"
#ifdef SGX
#include <glog/logging.h>

#include <set>
#include <map>
#include <memory>
#include <utility>
#include <string>

#include <nlohmann/json.hpp>
#include <boost/filesystem.hpp>
#include "src/primihub/common/defines.h"
#include "src/primihub/util/util.h"
#include "src/primihub/util/crypto/hash.h"
#include "sgx/engine/sgx_engine.h"
#include "fmt/format.h"

namespace primihub::task {
TEEDataProviderTask::TEEDataProviderTask(
    const std::string& node_id,
    const TaskParam *task_param,
    std::shared_ptr<DatasetService> dataset_service,
    sgx::RaTlsService* ra_service,
    sgx::TeeEngine* executor)
    : TaskBase(task_param, dataset_service) {
  this->executor_ = executor;
  this->ra_service_ = ra_service;
  this->node_id_ = node_id;
}

TEEDataProviderTask::~TEEDataProviderTask() {
}

retcode TEEDataProviderTask::LoadParams() {
  VLOG(0) << "begin to load params for party: " << this->party_name();
  auto task_config = this->getTaskParam();
  // dataset id
  const auto& party_datasets = task_config->party_datasets();
  auto iter = party_datasets.find(this->party_name());
  if (iter == party_datasets.end()) {
    LOG(ERROR) << "no party dataset info for party name: " << this->party_name();
    return retcode::FAIL;
  }
  auto& dataset = iter->second.data();
  auto ds_it = dataset.find(this->party_name());
  if (ds_it == dataset.end()) {
    LOG(ERROR) << "no dataset id is found for: " << this->party_name();
    return retcode::FAIL;
  }
  this->dataset_id_ = ds_it->second;
  // parse parameter
  const auto& param_map = task_config->params().param_map();
  auto it = param_map.find("component_params");
  if (it == param_map.end()) {
    LOG(ERROR) << "component_params config is not found for party name: " << this->party_name();
    return retcode::FAIL;
  }
  // param common param for data pre process type
  auto& component_params_str = it->second.value_string();
  try {
    component_params = nlohmann::json::parse(component_params_str);
    auto& common_params = component_params["common_params"];
    int op_code = common_params["data_pre_process_type"].get<int>();
    this->extra_info_ = {std::to_string(op_code)};
    auto& party_params = component_params["role_params"][this->party_name()];

    for (const auto idx : party_params["index"]) {
      this->data_index_.push_back(idx);
    }
    // output path
    if (party_params.contains("output")) {
      this->result_file_path_ = party_params["output"].get<std::string>();
    }
  } catch (std::exception& e) {
    LOG(ERROR) << "parse paramer failed, " << e.what();
    return retcode::FAIL;
  }
  return retcode::SUCCESS;
}

retcode TEEDataProviderTask::GetTeeExecutor(rpc::Node* executor_info, std::string* party_name) {
  auto& executors_name = component_params["roles"]["EXECUTOR"];
  if (executors_name.empty()) {
    LOG(ERROR) << "no executor is configured";
    return retcode::FAIL;
  }
  std::string executor_name = executors_name[0];
  auto task_config = this->getTaskParam();
  const auto& party_access_info = task_config->party_access_info();
  auto it = party_access_info.find(executor_name);
  if (it == party_access_info.end()) {
    LOG(ERROR) << "no party name " << executor_name << " is configured";
    return retcode::FAIL;
  }
  *executor_info = it->second;
  *party_name = executor_name;
  return retcode::SUCCESS;
}

retcode TEEDataProviderTask::LoadDataset() {
  CHECK_TASK_STOPPED(retcode::FAIL);
  auto driver = this->getDatasetService()->getDriver(this->dataset_id_);
  if (driver == nullptr) {
    std::string err_msg = fmt::format("get driver for dataset: {} failed", this->dataset_id_);
    CHECK_RETCODE_WITH_ERROR_MSG(retcode::FAIL, err_msg);
  }
  auto ret = LoadDatasetInternal(driver, data_index_, elements_);
  CHECK_RETCODE_WITH_ERROR_MSG(ret, "Load dataset for psi client failed");
  return retcode::SUCCESS;
}

std::set<std::string> remove_duplicate_data(const std::vector<std::string> &data_buf) {
  std::set<std::string> unique_data_buf;
  for (auto& it : data_buf) {
    unique_data_buf.insert(std::move(it));
  }
  return unique_data_buf;
}

std::map<std::string, std::string>
make_data_map_with_specified_alg(const std::set<std::string> &unique_data_buf,
                                  const std::string &alg) {
  std::map<std::string, std::string> data_hash_map;
  primihub::Hash h_alg;
  if (!h_alg.init()) {
    LOG(ERROR) << "unknown hash alg:" << alg;
    return data_hash_map;
  }

  for (const auto& it : unique_data_buf) {
    data_hash_map.insert(std::pair{h_alg.hash_str(it), it});
  }

  return data_hash_map;
}

int TEEDataProviderTask::execute() {
  LOG(INFO) << "begin execute";
  SCopedTimer timer;
  auto ret = LoadParams();
  if (ret != retcode::SUCCESS) {
    return -1;
  }
  auto load_param_time_cost = timer.timeElapse();
  VLOG(5) << "load params time cost(ms): " << load_param_time_cost;

  auto rv = LoadDataset();
  if (rv != retcode::SUCCESS) {
    LOG(ERROR) << "load dataset failed.";
    return -1;
  }
  auto load_dataset_ts = timer.timeElapse();
  auto load_dataset_time_cost = load_dataset_ts - load_param_time_cost;
  VLOG(5) << "load dataset time cost(ms): " << load_dataset_time_cost;

  LOG(INFO) << "before tee engine start";

  rpc::Node executor_node;
  std::string executor_party_name;
  ret = GetTeeExecutor(&executor_node, &executor_party_name);
  if (ret != retcode::SUCCESS) {
    LOG(ERROR) << "can't get remote node info";
    return -1;
  }
  sgx::Node remote_node(executor_node.ip(), executor_node.port(), executor_node.use_tls());
  auto success_flag = this->executor_->build_ratls_channel(request_id(), remote_node);
  if (!success_flag) {
    LOG(ERROR) << "build_ratls_channel error!";
    return -1;
  }
  LOG(INFO) << "build_ratls_channel success";

  auto key_id = this->executor_->get_key(request_id(), remote_node.ip());
  // todo data pre process should impelment in data class not here
  auto data_set = remove_duplicate_data(this->elements_);
  auto data_map = make_data_map_with_specified_alg(data_set, this->extra_info_[0]);
  if (data_map.empty()) {
    LOG(ERROR) << "compute hash value for input data error";
    return -1;
  }

  // put all string data into one string
  // need reserve memory
  std::string data;
  for (auto &it : data_map) {
    data += it.first;
  }

  // encrypt data with share key and compute mac
  auto encrypted_data_size = this->executor_->get_encrypted_size(data.size());
  auto encrypted_buf = std::make_unique<char[]>(encrypted_data_size);
  success_flag = this->executor_->encrypt_data_with_sharekey(key_id, data.data(), data.size(),
                                                    encrypted_buf.get(), encrypted_data_size);
  if (!success_flag) {
    LOG(ERROR) << "encrypt data with key:" << key_id << " failed!";
    return -1;
  }
  LOG(INFO) << "encrypt data with key:" << key_id << " successed!";
    // send encrypted data to compute node
  std::string id = this->gen_data_id(request_id(), this->node_id_);
  LOG(INFO) << "id: " << id << " encrypted_data_size:" << encrypted_data_size;
  primihub::Node _remote_node{remote_node.ip(), remote_node.port(), remote_node.use_tls()};
  std::string_view encrypted_data(encrypted_buf.get(), encrypted_data_size);
  rv = this->send(id, _remote_node, encrypted_data);
  if (rv != retcode::SUCCESS) {
    LOG(ERROR) << "send data to peer: [" << _remote_node.to_string()
               << "] failed";
    return -1;
  }
  LOG(INFO) << "send data to peer: [" << _remote_node.to_string()
             << "] successed";

  // wait result if this node is result node

  if (this->party_name() == PARTY_CLIENT) {
    id = this->gen_result_id(request_id());
    std::string recv_buff;
    rv = this->recv(id, &recv_buff);
    if (rv != retcode::SUCCESS) {
      LOG(ERROR) << "recv result failed";
      return -1;
    } else {
      LOG(INFO) << "recv result data len:" << recv_buff.size() << " bytes successed";
    }

    std::string plain_result;
    size_t plain_result_len = 0;
    success_flag = this->executor_->get_plain_size(recv_buff.data(),
                                                    recv_buff.size(),
                                                    &plain_result_len);
    if (!success_flag) {
      LOG(ERROR) << "get_plain_size failed:";
      return -1;
    }
    LOG(INFO) << "plain_result_len: " << plain_result_len;
    auto plain_result_buf = std::make_unique<char[]>(plain_result_len);
    success_flag = this->executor_->decrypt_data_with_sharekey(key_id,
                                                    recv_buff.data(),
                                                    recv_buff.size(),
                                                    plain_result_buf.get(),
                                                    &plain_result_len);
    if (!success_flag) {
      LOG(ERROR) << "encrypt data with key:" << key_id << " failed!";
      return -1;
    }
    // todo fix data already pre_process before read
    // todo wrap it into post process
    data_set.clear();
    auto hash_size = data_map.begin()->first.size();
    for (size_t i = 0; i < plain_result_len / hash_size; i++) {
      std::string temp{plain_result_buf.get() + i * hash_size, hash_size};
      if (data_map.find(temp) != data_map.end()) {
        result_.push_back(data_map[temp]);
      }
    }
    std::string col_title = "intersection_row";
    LOG(INFO) << "result size:" << result_.size();
    saveDataToCSVFile(result_, result_file_path_, col_title);
  }
  return 0;
}

TEEComputeTask::TEEComputeTask(const std::string& node_id, const TaskParam *task_param,
                               std::shared_ptr<DatasetService> dataset_service,
                               sgx::RaTlsService* ra_service,
                               sgx::TeeEngine* executor)
                               : TaskBase(task_param, dataset_service) {
  auto param_map = task_param->params().param_map();
  try {
    this->ra_service_ = ra_service;
    this->executor_ = executor;
    this->node_id_ = node_id;
  } catch (std::exception& e) {
    LOG(ERROR) << "Failed to load params: " << e.what();
    return;
  }
}

retcode TEEComputeTask::LoadParams() {
  auto task_config = this->getTaskParam();
  const auto& param_map = task_config->params().param_map();
  auto it = param_map.find("component_params");
  if (it == param_map.end()) {
    LOG(ERROR) << "component_params config is not found for party name: " << this->party_name();
    return retcode::FAIL;
  }
  // param common param for data pre process type
  auto& component_params_str = it->second.value_string();
  try {
    component_params = nlohmann::json::parse(component_params_str);
    auto& common_params = component_params["common_params"];
    int op_code = common_params["data_pre_process_type"].get<int>();
    this->extra_info_ = {std::to_string(op_code)};
  } catch (std::exception& e) {
    LOG(ERROR) << "parse paramer failed, " << e.what();
    return retcode::FAIL;
  }
  return retcode::SUCCESS;
}

std::vector<std::string> TEEComputeTask::GetDataProvider() {
  std::vector<std::string> data_provider;
  auto& role_info = component_params["roles"]["DATA_PROVIDER"];
  for (const auto& party_name : role_info) {
    data_provider.push_back(party_name.get<std::string>());
  }
  return data_provider;
}

TEEComputeTask::~TEEComputeTask() {
  for (auto &it : files_) {
    if (boost::filesystem::exists(it)) {
      boost::filesystem::remove(it);
    }
  }
}

int TEEComputeTask::receive_encrypted_data(const std::string &task_id, const std::string &node_id) {
  std::string recv_buff;
  auto id = this->gen_data_id(task_id, node_id);
  auto ret = this->recv(id, &recv_buff);
  if (ret != retcode::SUCCESS) {
    LOG(ERROR) << "recv data from peer: [" << node_id
               << "] failed";
    return -1;
  } else {
    LOG(INFO) << "recv data from peer: [" << node_id
              << " data len:" << recv_buff.size() << " bytes"
              << "] successed";
  }
  std::string key_id = this->get_tee_executor()->get_key(task_id, node_id);
  std::string data_file = key_id;
  this->set_data_file(data_file);
  auto rv = this->get_tee_executor()->decrypt_data_with_sharekey_inside(key_id,
      recv_buff.data(), recv_buff.size(), data_file);
  if (!rv) {
    LOG(ERROR) << "decrypt_data_with_sharekey_inside into file " << data_file << " error!";
    return -1;
  }
  return 0;
}

int TEEComputeTask::send_enrypted_result(const std::string &task_id,
                                        const std::string &node_id,
                                        const primihub::Node &remote_node,
                                        const std::string &result_name,
                                        size_t result_size) {
  std::string key_id = this->get_tee_executor()->get_key(task_id, node_id);

  auto cipher_size = this->get_tee_executor()->get_encrypted_size(result_size);
  LOG(INFO) << "cipher_size:" <<cipher_size;
  auto cipher_buf = std::make_unique<char[]>(cipher_size);

  auto ret = this->get_tee_executor()->encrypt_data_with_sharekey_inside(key_id,
      result_name, result_size, cipher_buf.get(), cipher_size);
  if (!ret) {
    LOG(ERROR) << "encrypt_data_with_sharekey_inside for result: " << result_name << " error!";
    return -1;
  }

  auto id = this->gen_result_id(request_id());
  auto rv = this->send(id, remote_node, std::move(std::string(cipher_buf.get(), cipher_size)));
  if (rv != retcode::SUCCESS) {
    LOG(ERROR) << "send data to peer: [" << remote_node.to_string()
               << "] failed";
    return -1;
  }
  LOG(INFO) << "send data to peer: [" << remote_node.to_string()
            << "] successed";
  return 0;
}


int TEEComputeTask::execute()  {
  auto ret = LoadParams();
  if (ret != retcode::SUCCESS) {
    return -1;
  }
  // receive encrypted data and code
  std::string id;
  auto executor = this->get_tee_executor();
  std::string task_id = this->task_param_.task_info().task_id();
  LOG(INFO) << "before receive data";
  // receive data from all data provider
  auto data_provider = this->GetDataProvider();
  if (data_provider.empty()) {
    LOG(ERROR) << "no data provider is configured";
    return -1;
  }
  auto task_config = this->getTaskParam();
  const auto& party_access_info = task_config->party_access_info();
  for (const auto& provider_name : data_provider) {
    auto it = party_access_info.find(provider_name);
    if (it == party_access_info.end()) {
      LOG(ERROR) << "no access info configured for data provider: " << provider_name;
      return -1;
    }
    auto& node_info = it->second;
    if (this->receive_encrypted_data(this->DataIdPrefix(), node_info.node_id()) != 0) {
      LOG(ERROR) << "receive data from " << provider_name << " failed";
      return -1;
    }
  }

  // compute
  auto code = this->task_param_.code();
  std::string result_name = this->DataIdPrefix() + "_result";
  this->set_data_file(result_name);

  size_t result_size = 0;

  auto rv = this->get_tee_executor()->do_compute(this->get_data_files(),
                                                code,
                                                this->get_extra_info(),
                                                &result_size);
  if (!rv) {
    LOG(ERROR) << "compute :" << code << " failed!";
    return -1;
  }

  // send encrypted result to result node
  auto it = party_access_info.find(PARTY_CLIENT);
  if (it == party_access_info.end()) {
    LOG(ERROR) << "no party name " << PARTY_CLIENT << " to config to receive data";
    return -1;
  }
  Node remote_node;
  pbNode2Node(it->second, &remote_node);
  auto ret_int = this->send_enrypted_result(this->DataIdPrefix(),
                                          remote_node.id(),
                                          remote_node,
                                          result_name,
                                          result_size);
  if (ret_int != 0) {
    LOG(ERROR) << "send result to "<< PARTY_CLIENT << " failed";
    return -1;
  }
  return 0;
}

}  // namespace primihub::task
#endif  // SGX

