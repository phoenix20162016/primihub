/*
 * Copyright (c) 2023 by PrimiHub
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      https://www.apache.org/licenses/
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef SRC_PRIMIHUB_NODE_NODE_INTERFACE_H_
#define SRC_PRIMIHUB_NODE_NODE_INTERFACE_H_
#include <glog/logging.h>
#include <grpc/grpc.h>
#include <grpcpp/security/server_credentials.h>
#include <grpcpp/server.h>
#include <grpcpp/server_builder.h>
#include <grpcpp/server_context.h>

#include <memory>
#include <thread>
#include <string>
#include <vector>

#include "src/primihub/protos/worker.grpc.pb.h"
#include "src/primihub/protos/common.pb.h"
#include "src/primihub/common/common.h"
#include "src/primihub/util/network/link_context.h"

using Server = grpc::Server;
using ServerBuilder = grpc::ServerBuilder;
using ServerContext = grpc::ServerContext;
template<typename T>
using ServerReader = grpc::ServerReader<T>;

template<typename T, typename U>
using ServerReaderWriter = grpc::ServerReaderWriter<T, U>;

template<typename T>
using ServerWriter = grpc::ServerWriter<T>;

using Status = grpc::Status;

using LinkContext = primihub::network::LinkContext;
namespace primihub {
class VMNodeProxyInterface final : public rpc::VMNode::Service {
 public:
  explicit VMNodeProxyInterface(LinkContext* link_ctx, const Node& dest_node) :
    link_ctx_(link_ctx), dest_node_(dest_node) {

  }
  ~VMNodeProxyInterface() = default;

  Status SubmitTask(ServerContext *context,
                    const rpc::PushTaskRequest *pushTaskRequest,
                    rpc::PushTaskReply *pushTaskReply) override;

  Status ExecuteTask(ServerContext* context,
                     const rpc::PushTaskRequest* request,
                     rpc::PushTaskReply* response) override;
  Status StopTask(ServerContext* context,
                  const rpc::TaskContext* request,
                  rpc::Empty* response) override;
  Status KillTask(ServerContext* context,
                  const rpc::KillTaskRequest* request,
                  rpc::KillTaskResponse* response) override;
  // task status operation
  Status FetchTaskStatus(ServerContext* context,
                         const rpc::TaskContext* request,
                         rpc::TaskStatusReply* response) override;

  Status UpdateTaskStatus(ServerContext* context,
                          const rpc::TaskStatus* request,
                          rpc::Empty* response) override;

  Status Send(ServerContext* context,
              ServerReader<rpc::TaskRequest>* reader,
              rpc::TaskResponse* response) override;

  Status Recv(ServerContext* context,
              const rpc::TaskRequest* request,
              ServerWriter<rpc::TaskResponse>* writer) override;
  /**
   * @param
   *
  */
  Status SendRecv(ServerContext* context,
                  ServerReaderWriter<rpc::TaskResponse,
                                     rpc::TaskRequest>* stream) override;

  // for communication between different process
  Status ForwardSend(ServerContext* context,
                     ServerReader<rpc::ForwardTaskRequest>* reader,
                     rpc::TaskResponse* response) override;

  // for communication between different process
  Status ForwardRecv(ServerContext* context,
                     const rpc::TaskRequest* request,
                     ServerWriter<rpc::TaskRequest>* writer) override;
  /**
   * wait until complete queue has filled expected number of complete status
  */
  Status CompleteStatus(ServerContext* context,
                        const rpc::CompleteStatusRequest* request,
                        rpc::Empty* response);

 private:
  LinkContext* link_ctx_{nullptr};
  Node dest_node_;
};
}  // namespace primihub
#endif  // SRC_PRIMIHUB_NODE_NODE_INTERFACE_H_
