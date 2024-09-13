#ifndef SRC_PRIMIHUB_NODE_HTTP_NODE_REQUEST_HANDLER_H_
#define SRC_PRIMIHUB_NODE_HTTP_NODE_REQUEST_HANDLER_H_
#include <glog/logging.h>
#include "Poco/Timestamp.h"
#include "Poco/DateTimeFormatter.h"
#include "Poco/DateTimeFormat.h"
#include "Poco/Net/HTTPRequestHandler.h"
#include "Poco/Net/HTTPServerRequest.h"
#include "Poco/Net/HTTPServerResponse.h"
#include "src/primihub/node/http/node_http_interface.h"
#include "src/primihub/node/ds_handler.h"
namespace primihub {
using Poco::Timestamp;
using Poco::DateTimeFormatter;
using Poco::DateTimeFormat;
using Poco::Net::HTTPRequestHandler;
using Poco::Net::HTTPServerRequest;
using Poco::Net::HTTPServerResponse;

class TimeRequestHandler: public HTTPRequestHandler {
	/// Return a HTML document with the current date and time.
public:
	TimeRequestHandler(const std::string& format):
		_format(format) {}

	void handleRequest(HTTPServerRequest& request, HTTPServerResponse& response) {
		LOG(INFO) << "Request from " + request.clientAddress().toString();

		Timestamp now;
		std::string dt(DateTimeFormatter::format(now, _format));

		response.setChunkedTransferEncoding(true);
		response.setContentType("text/html");

		std::ostream& ostr = response.send();
		ostr << "<html><head><title>"
            "HTTPTimeServer powered by POCO C++ Libraries</title>";
		ostr << "<meta http-equiv=\"refresh\" content=\"1\"></head>";
		ostr << "<body><p style=\"text-align: center; font-size: 48px;\">";
		ostr << dt;
    ostr << "testesttest";
		ostr << "</p></body></html>";
	}

private:
	std::string _format;
};

// class SendDataRequestHandler: public HTTPRequestHandler {
// 	/// Return a HTML document with the current date and time.
// public:
// 	SendDataRequestHandler() = default;

// 	void handleRequest(HTTPServerRequest& request, HTTPServerResponse& response) {
// 		LOG(INFO) << "Request from " + request.clientAddress().toString();
//     std::istream &istr = request.stream();
//     auto len = request.getContentLength();
//     if (len > 0) {
//       LOG(INFO) << "XXXXXXXXX content len: " << len;
//       std::string buf;
//       buf.resize(len);
//       istr.read(&buf[0], len);
//       LOG(INFO) << "received data: " << buf;
//     } else {
//       LOG(INFO) << "content length is: " << len;
//     }
// 		response.setChunkedTransferEncoding(true);
//     std::string msg_info{"kdsjfaskfjalskdfjlsakdfjslakdfjslkdfjaslkdfjslkfjsalkfdjasklfj"};
// 		std::ostream& ostr = response.send();
//     ostr << msg_info;
// 	}
// };

class TaskRequestHandler: public HTTPRequestHandler {
 public:
  TaskRequestHandler(void* service_impl);
  virtual ~TaskRequestHandler() = default;

 protected:
  // template <typename Func, typename REQ, typename RES>
  // retcode TaskProcessHandler(HTTPServerRequest& req,
  //                            HTTPServerResponse& resp, Func func) {
  //   std::string data_str;
  //   ExtractRequestData(request, &data_str);
  //   REQ rpc_req;
  //   rpc_req.ParseFromString(data_str);
  //   RES rpc_resp;
  //   func(rpc_req, &rpc_resp);
  //   std::string resp_str;
  //   rpc_resp.SerializeToString(&resp_str);
  //   response.setChunkedTransferEncoding(true);
  //   std::ostream& ostr = response.send();
  //   ostr << resp_str;
  //   return retcode::SUCCESS;
  // }

  VMNodeHttpInterface* Interface() {return interface_;}
  retcode ExtractRequestData(HTTPServerRequest& request,
                             std::string* payload);

 private:
  VMNodeHttpInterface* interface_{nullptr};
};

// -----------------------------SubmitTask------------------------------------
class SubmitTaskRequestHandler: public TaskRequestHandler {
 public:
  SubmitTaskRequestHandler(void* service_impl);
  void handleRequest(HTTPServerRequest& request,
                     HTTPServerResponse& response) override;
};

// -----------------------------ExecuteTask------------------------------------
class ExecuteTaskRequestHandler: public TaskRequestHandler {
 public:
  ExecuteTaskRequestHandler(void* service_impl);
  void handleRequest(HTTPServerRequest& request,
                    HTTPServerResponse& response) override;
};

// -----------------------------StopTask------------------------------------
class StopTaskRequestHandler: public TaskRequestHandler {
 public:
  StopTaskRequestHandler(void* service_impl);
  void handleRequest(HTTPServerRequest& request,
                    HTTPServerResponse& response) override;
};

// -----------------------------KillTask------------------------------------
class KillTaskRequestHandler: public TaskRequestHandler {
 public:
  KillTaskRequestHandler(void* service_impl);
  void handleRequest(HTTPServerRequest& request,
                    HTTPServerResponse& response) override;
};

// -----------------------------FetchTaskStatus-----------------------------
class FetchTaskStatusRequestHandler: public TaskRequestHandler {
 public:
  FetchTaskStatusRequestHandler(void* service_impl);
  void handleRequest(HTTPServerRequest& request,
                    HTTPServerResponse& response) override;
};

// -----------------------------UpdateTaskStatus-----------------------------
class UpdateTaskStatusRequestHandler: public TaskRequestHandler {
 public:
  UpdateTaskStatusRequestHandler(void* service_impl);
  void handleRequest(HTTPServerRequest& request,
                    HTTPServerResponse& response) override;
};

// -----------------------------SendData------------------------------------
class SendDataRequestHandler: public TaskRequestHandler {
 public:
  SendDataRequestHandler(void* service_impl);
  void handleRequest(HTTPServerRequest& request,
                    HTTPServerResponse& response) override;
};

// -----------------------------RecvData------------------------------------
class RecvDataRequestHandler: public TaskRequestHandler {
 public:
  RecvDataRequestHandler(void* service_impl);
  void handleRequest(HTTPServerRequest& request,
                    HTTPServerResponse& response) override;
};

// -----------------------------ForwardRecvData-------------------------------
class ForwardRecvRequestHandler: public TaskRequestHandler {
 public:
  ForwardRecvRequestHandler(void* service_impl);
  void handleRequest(HTTPServerRequest& request,
                    HTTPServerResponse& response) override;
};

// // -----------------------------NewDataset-------------------------------
class NewDatasetRequestHandler: public TaskRequestHandler {
 public:
  NewDatasetRequestHandler(void* service_impl, DataServiceHandler* ds_handler);
  void handleRequest(HTTPServerRequest& request,
                     HTTPServerResponse& response) override;
 private:
  DataServiceHandler* ref_ds_handler_;
};
} // namespace primihub


#endif  // SRC_PRIMIHUB_NODE_HTTP_NODE_REQUEST_HANDLER_H_
