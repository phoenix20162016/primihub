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

#ifndef SRC_PRIMIHUB_TASK_SEMANTIC_TEE_TASK_H_
#define SRC_PRIMIHUB_TASK_SEMANTIC_TEE_TASK_H_
#ifdef SGX
#include <vector>
#include <memory>
#include <string>

#include "src/primihub/task/semantic/task.h"
#include "src/primihub/task/semantic/psi_task.h"

#include "sgx/secure_channel/service.h"
#include "sgx/engine/sgx_engine.h"


namespace primihub::task {
/**
 * @brief TEE DataProvider role task
 *
 */
// todo add teetask as base of TEEDataProviderTask & TEEComputeTask
class TEEDataProviderTask: public TaskBase, public PsiCommonOperator {
 public:
  TEEDataProviderTask(
      const std::string &node_id,
      const TaskParam *task_config,
      std::shared_ptr <DatasetService> dataset_service,
      sgx::RaTlsService* ra_service,
      sgx::TeeEngine* executor);
  ~TEEDataProviderTask();
  int execute();

 private:
  std::string gen_data_id(const std::string &task_id, const std::string &nodeid) {
    return task_id + "_" + nodeid;
  }
  std::string gen_result_id(const std::string &task_id) {
    return task_id + "_result";
  }
  retcode LoadDataset();
  retcode LoadParams();
  retcode GetTeeExecutor(rpc::Node* executor_info, std::string* party_name);

 private:
  sgx::RaTlsService* ra_service_{nullptr};
  sgx::TeeEngine* executor_{nullptr};
  std::string node_id_;
  std::string dataset_id_;
  std::vector<std::string> elements_;
  std::vector<std::string> result_;
  std::string result_file_path_;
  std::vector<int> data_index_;
  std::vector<std::string> extra_info_;
  nlohmann::json component_params;
};

class TEEComputeTask: public TaskBase {
 public:
  TEEComputeTask(const std::string &node_id, const TaskParam *task_param,
                 std::shared_ptr<DatasetService> dataset_service,
                 sgx::RaTlsService* ra_service,
                 sgx::TeeEngine* executor);
  ~TEEComputeTask();
  int execute();
  sgx::TeeEngine* get_tee_executor() { return executor_; }

 protected:
  retcode LoadParams();
  std::string DataIdPrefix() {
    return request_id();
  }
  std::vector<std::string> GetDataProvider();

 private:
  sgx::RaTlsService* ra_service_{nullptr};
  sgx::TeeEngine* executor_{nullptr};
  std::string node_id_;
  std::vector<std::string> files_;
  std::vector<std::string> extra_info_;
  int receive_encrypted_data(const std::string &task_id, const std::string &node_id);
  int send_enrypted_result(const std::string &task_id, const std::string &node_id,
                           const primihub::Node &remote_node,
                           const std::string &result_name, size_t res_size);
  std::string gen_data_id(const std::string &task_id, const std::string &nodeid) {
    return task_id + "_" + nodeid;
  }
  std::string gen_result_id(const std::string &task_id) {
    return task_id + "_result";
  }
  std::vector<std::string> &get_data_files() { return files_;}
  void set_data_file(const std::string &file) { files_.push_back(file); }
  std::vector<std::string>& get_extra_info() {return extra_info_;}
  nlohmann::json component_params;
};

}  // namespace primihub::task
#endif  // SGX
#endif  // SRC_PRIMIHUB_TASK_SEMANTIC_TEE_TASK_H_
