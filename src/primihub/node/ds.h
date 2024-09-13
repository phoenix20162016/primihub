/*
 Copyright 2022 PrimiHub

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

#ifndef SRC_PRIMIHUB_NODE_DS_H_
#define SRC_PRIMIHUB_NODE_DS_H_

#include <glog/logging.h>
#include <grpc/grpc.h>
#include <grpcpp/security/server_credentials.h>
#include <grpcpp/server.h>
#include <grpcpp/server_builder.h>
#include <grpcpp/server_context.h>

#include <memory>
#include <string>

#include "src/primihub/protos/service.grpc.pb.h"
#include "src/primihub/protos/service.pb.h"
#include "src/primihub/node/ds_handler.h"
#include "src/primihub/common/common.h"

namespace primihub {
class DataServiceImpl final: public rpc::DataSetService::Service {
 public:
  explicit DataServiceImpl(service::DatasetService* service) {
    ds_handler_ = std::make_unique<DataServiceHandler>(service);
  }

  grpc::Status NewDataset(grpc::ServerContext *context,
                          const rpc::NewDatasetRequest *request,
                          rpc::NewDatasetResponse *response) override;
  grpc::Status QueryResult(grpc::ServerContext* context,
                           const rpc::QueryResultRequest* request,
                           rpc::QueryResultResponse* response);
  grpc::Status DownloadData(grpc::ServerContext* context,
                            const rpc::DownloadRequest* request,
                            grpc::ServerWriter<rpc::DownloadRespone>* writer);
  grpc::Status UploadData(grpc::ServerContext* context,
                          grpc::ServerReader<rpc::UploadFileRequest>* reader,
                          rpc::UploadFileResponse* response);

 private:
  std::unique_ptr<DataServiceHandler> ds_handler_{nullptr};
};

}  // namespace primihub
#endif  // SRC_PRIMIHUB_NODE_DS_H_
